/*
 * Copyright (c) 2014 Johannes Postma <jgmpostma@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define _XOPEN_SOURCE 700	/* Needed for wcwidth */
#define _XOPEN_SOURCE_EXTENDED	/* Needed for wchar support */

#include <bsd/string.h>
#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include <curl/curl.h>
#include <jansson.h>
#include <ncurses.h>
#include <vlc/vlc.h>

#define APIKEY		"e233c13d38d96e3a3a0474723f6b3fcd21904979"
#define DELAY		15	/* Half-delay time */
#define ESCDELAY	10	/* Time (ms)  to wait after reading ESC */
#define BODY_HEIGHT	800	/* Pad height of body*/
#define FOOTER_HEIGHT	1	/* Pad height of footer */
#define HEADER_HEIGHT	2	/* Pad height of header */
#define FALSE		0
#define TRUE		1

enum states {START, SEARCH, SELECT, PLAYING};

struct buf_t {
	char	*data;
	size_t	 size;
};
struct searchstr_node {
	struct searchstr_node	*next;
	wchar_t			 c;
};

/* Init */
static	void	 checklocale(void);
static	void	 initglobals(void);
static	void	 initncurses(void);
static	void	 initvlc(void);
/* Cleanup */
static	void	 cleanupncurses(void);
static	void	 cleanupvlc(void);
/* Search */
static	void	 search(void);
static	void	 searchaddchar(wint_t);
static	void	 searchbackspace(void);
static	void	 searchchangepos(wint_t);
static	void	 searchdelete(void);
static	void	 searchescape(void);
static	void	 searchinit(void);
static	void	 searchstr_clear(void);
static	size_t	 searchstr_length(void);
static	void	 searchstr_pop(void);
static	void	 searchstr_push(wint_t);
/* Select */
static	void	 selectchangepos(wint_t);
static	void	 selectescape(void);
static	void	 selectmix(void);
/* Play */
static	char	*getplaytoken(void);
static	int	 needtoreport(void);
static	void	 mixnext(void);
static	void	 playpause(void);
static	void	 trackfirst(void);
static	void	 tracknext(void);
static	void	 trackplay(void);
static	void	 trackreport(void);
static	void	 trackskip(void);
	void	 vlc_event_error(const struct libvlc_event_t*, void*);
	void	 vlc_event_trackend(const struct libvlc_event_t*, void*);
/* Draw */
static	void	 drawbody(void);
static	void	 drawborders(void);
static	void	 drawerror(char*);
static	void	 drawfooter(void);
static	void	 drawheader(void);
static	void	 drawplaylistinit(void);
static	void	 drawplaylistupdate(void);
static	void	 drawsearch(void);
static	void	 drawsearch_wait(void);
static	void	 drawselect(void);
static	void	 drawwelcome(void);
static	void	 handleresize(void);
/* Other */
static	size_t	 curlwrite(void*, size_t, size_t, void*);
static	char	*fetch(char*);
static	int	 handlekey(int,wint_t);
static	size_t	 intlen(int);
static	int	 mod(int, int);

/* Globals */
static int			 pstate, state;
static char			*playtoken;
static int			 mix_id;
static char			*mix_name;
static int			 track_ended;
static int			 track_id;
static int			 track_last;
static char			*track_name;
static int			 track_number;
static int			 track_reported;
static int			 track_skip_allowed;
static struct searchstr_node	*searchstr_head;
static char			*searchstr_last;
static int			 searchstr_pos;
static int			 select_pos;
static json_t			*select_root;
static libvlc_instance_t	*vlc_inst;
static libvlc_media_player_t	*vlc_mp;
static libvlc_event_manager_t	*vlc_em;
static WINDOW			*win_body;
static WINDOW			*win_footer;
static WINDOW			*win_header;
static WINDOW			*win_main;
static WINDOW			*win_search;
static WINDOW			*win_select;
static int			 win_pos;
static int			 win_y;

static void
checklocale(void)
{
	char *locale;

	locale = setlocale(LC_ALL, "");
	if (locale == NULL || 
	    (strstr(locale, "UTF-8") == NULL &&
	     strstr(locale, "utf-8") == NULL &&
	     strstr(locale, "UTF8")  == NULL &&
	     strstr(locale, "utf8")  == NULL))
		errx(1, "UTF-8 locale expected");
}

static void
cleanupncurses(void)
{
	(void) endwin();
}

static void
cleanupvlc(void)
{
	libvlc_media_player_release(vlc_mp);
	libvlc_release(vlc_inst);
}

static size_t
curlwrite(void *contents, size_t size, size_t nmemb, void *stream)
{
	size_t total;
	struct buf_t *buf;

	total = size * nmemb;
	buf = (struct buf_t *)stream;
	buf->data = realloc(buf->data, buf->size + total + 1);
	if (buf->data == NULL)
		err(1, NULL);
	memcpy(&(buf->data[buf->size]), contents, total);
	buf->size += total;
	buf->data[buf->size] = '\0';

	return total;
}

static void
drawbody(void)
{
	WINDOW *win;

	switch (state) {
	case SEARCH:
		win = win_search;
		break;
	case SELECT:
		win = win_select;
		break;
	default:
		win = win_body;
		break;
	}
	(void) prefresh(win, 0, 0, HEADER_HEIGHT+3, 2, LINES-4-FOOTER_HEIGHT,
			COLS-4);
}

static void
drawplaylistinit(void)
{
	(void) wclear(win_body);
	(void) mvwprintw(win_body, 0, 0, "Playlist");
	(void) mvwprintw(win_body, 1, 0, "--------");
	win_y = 3;
	drawbody();
}

static void
drawplaylistupdate(void)
{
	(void) mvwprintw(win_body, win_y, 0, "%d. %s\n",
			 track_number, track_name);
	win_y = getcury(win_body);
	drawbody();
}

static void
drawborders(void)
{
	WINDOW *win = win_main;
	int x, y;

	(void) wclear(win);
	for (y = 0; y < LINES; y++) {
		if (y == 0 || y == HEADER_HEIGHT+1) {
			for (x = 1; x < COLS-1; x++)
				(void) mvwaddch(win, y, x, ACS_HLINE);
		} else if (LINES> HEADER_HEIGHT+2 &&
			   (y == LINES-FOOTER_HEIGHT-2 || y == LINES-1)) {
			for (x = 1; x < COLS-1; x++)
				(void) mvwaddch(win, y, x, ACS_HLINE);
		}
		if (y == 0) {
			(void) mvwaddch(win, y, 0, ACS_ULCORNER);
			(void) mvwaddstr(win, y, 3, " 8p ");
			(void) mvwaddch(win, y, COLS-1, ACS_URCORNER);
		} else if ((y == HEADER_HEIGHT+1 ||
			    y == LINES-FOOTER_HEIGHT-2) &&
			   LINES > HEADER_HEIGHT+2) {
			(void) mvwaddch(win, y, 0, ACS_LTEE);
			(void) mvwaddch(win, y, COLS-1, ACS_RTEE);
		} else if (y == LINES-1) {
			(void) mvwaddch(win, y, 0, ACS_LLCORNER);
			(void) mvwaddch(win, y, COLS-1, ACS_LRCORNER);
		} else {
			(void) mvwaddch(win, y, 0, ACS_VLINE);
			(void) mvwaddch(win, y, COLS-1, ACS_VLINE);
		}
	}
	(void) wrefresh(win);
}

static void
drawerror(char *err)
{
	if (err == NULL)
		return;
	(void) wclear(win_footer);
	(void) mvwprintw(win_footer, 0, 0, "Warning: %s", err);
	(void) curs_set(0);
	(void) prefresh(win_footer, 0, 0, LINES-FOOTER_HEIGHT-1, 2, LINES-1,
			COLS-4);
	(void) sleep(2);
	drawfooter();
}

static void
drawfooter(void)
{
	struct searchstr_node *it;
	char playing[] = "q Quit  s Search  p Play/Pause  n Next track"
		"  N Next mix";
	char search[] = "ESC Exit |  Search: ";
	char select[] = "ESC Exit  Enter Select";
	char start[] = "q Quit  s Search";
	int cursor_pos, i;

	(void) wclear(win_footer);
	switch (state) {
	case PLAYING:
		(void) mvwprintw(win_footer, 0, 0, "%s", playing);
		(void) curs_set(0);
		break;
	case START:
		(void) mvwprintw(win_footer, 0, 0, "%s", start);
		(void) curs_set(0);
		break;
	case SEARCH:
		(void) mvwprintw(win_footer, 0, 0, "%s", search);
		for (it = searchstr_head; it != NULL; it = it->next)
			(void) wprintw(win_footer, "%lc", it->c);
		(void) curs_set(1);
		cursor_pos = strlen(search);
		it = searchstr_head;
		for (i = 0; i < searchstr_pos; i++) {
			cursor_pos += wcwidth(it->c);
			it = it->next;
		}
		(void) wmove(win_footer, 0, cursor_pos);
		break;
	case SELECT:
		(void) mvwprintw(win_footer, 0, 0, "%s", select);
		(void) curs_set(0);
		break;
	default:
		(void) curs_set(0);
		break;
	}
	(void) prefresh(win_footer, 0, 0, LINES-FOOTER_HEIGHT-1, 2,
	    LINES-1, COLS-4);
}

static void
drawheader(void)
{
	(void) wclear(win_header);
	if (mix_name != NULL)
		(void) mvwprintw(win_header, 0, 0, "Mix:   %s", mix_name);
	else
		(void) mvwprintw(win_header, 0, 0, "Mix:");
	if (track_name != NULL)
		(void) mvwprintw(win_header, 1, 0, "Track: %s", track_name);
	else
		(void) mvwprintw(win_header, 1, 0, "Track:");
	(void) prefresh(win_header, 0, 0, 1, 2, HEADER_HEIGHT, COLS-4);
}

static void
drawsearch(void)
{
	char *txt[] = {
	     "Search Help",
	     "-----------",
	     "",
	     "Search by using Smart ID, for example:",
	     "",
	     "Smart ID                    Result",
	     "all:popular                 all mixes",
	     "tags:chill                  mixes tagged \"chill\"",
	     "tags:chill+hip_hop:recent   new mixes tagged \"chill\" & "
		 "\"hip hop\"",
	     "artist:Radiohead            mixes including songs by Radiohead",
	     "keyword:ocarina             mixes including the text \"ocarina\"",
	     "dj:1                        mixes published by the user with "
		 "id=1",
	     "liked:1                     mixes liked by the user with id=1",
	     "similar:14                  mixes similar to mix with id=14",
	     "",
	     "Note: no spaces allowed."};
	int i;

	(void) wclear(win_search);
	for (i = 0; i < (int)(sizeof(txt)/sizeof(txt[0])); i++)
		(void) mvwprintw(win_search, i, 0, "%s", txt[i]);
}

static void
drawsearch_wait(void)
{
	(void) wclear(win_select);
	(void) mvwprintw(win_select, 0, 0, "Searching...");
	drawbody();
}

static void
drawselect(void)
{
	int count, i;
	json_t *data, *descr, *mix_set, *mixes, *name, *tags, *plays, *likes;
	size_t len;

	mix_set = json_object_get(select_root, "mix_set");
	if (mix_set == NULL) {
		json_decref(select_root);
		drawerror("Search returned no results.");
		state = pstate;
		drawbody();
		drawfooter();
		return;
	}
	mixes = json_object_get(mix_set, "mixes");
	if (!json_is_array(mixes)) {
		json_decref(select_root);
		drawerror("Search returned no results.");
		state = pstate;
		drawbody();
		drawfooter();
		return;
	}
	len = json_array_size(mixes);
	(void) wclear(win_select);
	(void) wmove(win_select, 0,0);
	count = 0;
	i = select_pos;
	while (count < (int) len) {
		i = mod(i, len);
		data = json_array_get(mixes, i);
		name = json_object_get(data, "name");
		tags = json_object_get(data, "tag_list_cache");
		descr = json_object_get(data, "description");
		plays = json_object_get(data, "plays_count");
		likes = json_object_get(data, "likes_count");
		if (i == select_pos)
			(void) wattron(win_select, A_REVERSE);
		(void) wattron(win_select, A_BOLD);
		(void) wprintw(win_select, "%d. %s\n", i+1,
		    json_string_value(name));
		(void) wattroff(win_select, A_BOLD);
		if (i == select_pos)
			(void) wattroff(win_select, A_REVERSE);
		(void) wattron(win_select, A_BOLD);
		(void) wprintw(win_select, "Description:\n");
		(void) wattroff(win_select, A_BOLD);
		(void) wprintw(win_select, "%s\n", json_string_value(descr));
		(void) wattron(win_select, A_BOLD);
		(void) wprintw(win_select, "Tags:\n");
		(void) wattroff(win_select, A_BOLD);
		(void) wprintw(win_select, "%s\n", json_string_value(tags));
		(void) wattron(win_select, A_BOLD);
		(void) wprintw(win_select, "Number of plays:\n");
		(void) wattroff(win_select, A_BOLD);
		(void) wprintw(win_select, "%d\n",
		    (int) json_integer_value(plays));
		(void) wattron(win_select, A_BOLD);
		(void) wprintw(win_select, "Number of likes:\n");
		(void) wattroff(win_select, A_BOLD);
		(void) wprintw(win_select, "%d\n",
		    (int) json_integer_value(likes));
		(void) wprintw(win_select, "\n---\n\n");
		i++;
		count++;
	}
	drawbody();
}

static void
drawwelcome(void)
{
	(void) wclear(win_body);
	(void) mvwprintw(win_body, 0, 0, "Welcome to 8p.");
	(void) mvwprintw(win_body, 1, 0,
	    "Press \"s\" to start searching for 8tracks.com mixes.");
	drawbody();
}

/* Return TRUE (1) if user wants to quit, else FALSE (0) */
static int
handlekey(int err, wint_t c)
{
	int quit = FALSE;

	if (err == ERR)
		return quit;
	if (c == KEY_RESIZE) {
		handleresize();
		return quit;
	}
	switch (state) {
	case PLAYING:
		switch (err) {
		case OK:
			switch (c) {
			case 'q':	quit = TRUE;  break;
			case 's':	searchinit(); break;
			case 'n':	trackskip();  break;
			case 'p':	playpause();  break;
			case 'N':	mixnext();    break;
			default:	break;
			}
			break;
		default:	break;
		}
		break;
	case SEARCH:
		switch (err) {
		case KEY_CODE_YES:
			switch (c) {
			case KEY_BACKSPACE:	searchbackspace(); break;
			case KEY_DC:		searchdelete(); break;
			case KEY_ENTER:		search(); break;
			case KEY_LEFT:		/* Fallthrough */
			case KEY_RIGHT:		searchchangepos(c); break;
			default:		break;
			}
			break;
		case OK:
			/*  10: Enter
			 *  27: Escape
			 *  32: Spacebar
			 * 127: Backspace
			 */
			switch (c) {
			case  10:	search(); break;
			case  27:	searchescape(); break;
			case  32:	break; /* Ignore */
			case 127:	searchbackspace(); break;
			default:	searchaddchar(c); break;
			}
			break;
		default:
			break;
		}
		break;
	case SELECT:
		switch (err) {
		case KEY_CODE_YES:
			switch (c) {
			case KEY_UP:	/* Fallthrough */
			case KEY_DOWN:	selectchangepos(c); break;
			case KEY_ENTER:	selectmix(); break;
			default:	break;
			}
		case OK:
			/* 10: Enter
			 * 27: Escape
			 */
			switch (c) {
			case 10:	selectmix(); break;
			case 27:	selectescape(); break;
			default:	break;
			}
		default:	break;
		}
		break;
	case START:
		switch (err) {
		case OK:
			switch (c) {
			case 'q':	quit = TRUE; break;
			case 's':	searchinit(); break;
			default:	break;
			}
			break;
		default:	break;
		}
		break;
	default:	break;
	}

	return quit;
}

static void
handleresize(void)
{
	drawborders();
	drawheader();
	if (LINES > HEADER_HEIGHT+2) {
		drawbody();
		drawfooter();
	}
}

static void
initglobals(void)
{
	mix_name       = NULL;
	playtoken      = NULL;
	searchstr_head = NULL;
	searchstr_last = NULL;
	track_name     = NULL;
}

static void
initncurses(void)
{
	win_main = initscr();
	if (win_main == NULL)
		errx(1, "failed to initialize ncurses");
	if ((meta(win_main, true) == ERR) ||
	    (keypad(win_main, true) == ERR) ||
	    (noecho() == ERR) ||
	    (halfdelay(DELAY) == ERR) ||
	    (curs_set(0) == ERR) ||
	    (set_escdelay(ESCDELAY) == ERR)) {
		(void) endwin();
		errx(1, "failed to initialize ncurses");
	}
	win_header = newpad(HEADER_HEIGHT, 200);
	win_body = newpad(BODY_HEIGHT, 200);
	win_search = newpad(BODY_HEIGHT, 200);
	win_select = newpad(BODY_HEIGHT, 200);
	win_footer = newpad(FOOTER_HEIGHT, 200);
	win_y = 0;
	win_pos = 0;

	drawborders();
	drawsearch();
	drawwelcome();
	drawfooter();
	drawheader();
}

static void
initvlc(void)
{
	libvlc_callback_t callback;

	vlc_inst = libvlc_new(0, NULL);
	vlc_mp = libvlc_media_player_new(vlc_inst);
	vlc_em = libvlc_media_player_event_manager(vlc_mp);
	callback = vlc_event_trackend;
	libvlc_event_attach(vlc_em, libvlc_MediaPlayerEndReached, callback,
			    NULL);
	callback = vlc_event_error;
	libvlc_event_attach(vlc_em, libvlc_MediaPlayerEncounteredError,
			    callback, NULL);
}

static size_t
intlen(int i)
{
	size_t len;

	if (i < 0)
		i *= -1;
	len = 1;
	while (i > 9) {
		len++;
		i /= 10;
	}

	return len;
}

static char *
fetch(char *url)
{
	CURL *curl;
	CURLcode curl_err;
	char *key, *ret;
	char prefix_key[] = "X-Api-Key: ";
	size_t len;
	struct buf_t buf;
	struct curl_slist *headers;

	if (url == NULL)
		return NULL;
	curl_err = curl_global_init(CURL_GLOBAL_ALL);
	if (curl_err != 0)
		return NULL;
	if ((buf.data = malloc(1)) == NULL)
		err(1, NULL);
	buf.size = 0;
	len = strlen(prefix_key) + strlen(APIKEY) + 1;
	key = malloc(len * sizeof(char));
	if (key == NULL)
		err(1, NULL);
	(void) snprintf(key, len, "%s%s", prefix_key, APIKEY);
	headers = curl_slist_append(NULL, key);
	headers = curl_slist_append(headers, "X-Api-Version: 3");
	headers = curl_slist_append(headers, "User-Agent: 8p");
	headers = curl_slist_append(headers, "Accept: application/json");
	curl = curl_easy_init();
	(void) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	(void) curl_easy_setopt(curl, CURLOPT_URL, url);
	(void) curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlwrite);
	(void) curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&buf);
	curl_err = curl_easy_perform(curl);
	if (curl_err != CURLE_OK)
		ret = NULL;
	else if (buf.size > 0) {
		len = buf.size + 1;
		ret = malloc(len*sizeof(char));
		if (ret == NULL)
			err(1, NULL);
		(void) strlcpy(ret, buf.data, len);
	} else
		ret = NULL;

	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);
	curl_global_cleanup();
	free(buf.data);
	free(key);

	return ret;
}

static char *
getplaytoken(void) {
	char *js, *ret;
	char url[] = "http://8tracks.com/sets/new";
	json_t *playtoken, *root;
	size_t len;

	js = fetch(url);
	if (js == NULL)
		return NULL;
	root = json_loads(js, 0, NULL);
	free(js);
	if (root == NULL)
		return NULL;
	playtoken = json_object_get(root, "play_token");
	if (playtoken == NULL) {
		json_decref(root);
		return NULL;
	}
	if (json_string_value(playtoken) == NULL) {
		json_decref(root);
		return NULL;
	}
	len = strlen(json_string_value(playtoken)) + 1;
	ret = malloc(len * sizeof(char));
	if (ret == NULL)
		err(1, NULL);
	strlcpy(ret, json_string_value(playtoken), len); 
	json_decref(root);

	return ret;
}

static int
needtoreport(void)
{
	int ret = FALSE;

	if (libvlc_media_player_get_time(vlc_mp) >= (30 * 1000))
		ret = TRUE;

	return ret;
}

static int
mod(int a, int b)
{
	int ret;

	ret = a % b;
	if (ret < 0)
		ret += b;
	
	return ret;
}

static void
trackfirst(void)
{
	char *js, *url;
	char prefix[] = "http://8tracks.com/sets/";
	char interfix[] = "/play?mix_id=";
	size_t len;

	if (playtoken == NULL)
		playtoken = getplaytoken();
	if (playtoken == NULL) {
		drawerror("Could not get playtoken. Stopped playback.");
		state = START;
		return;
	}

	len = strlen(prefix) + strlen(playtoken) + strlen(interfix) +
		intlen(mix_id) + 1;
	url = malloc(len * sizeof(char));
	if (url == NULL)
		err(1, NULL);
	(void) snprintf(url, len, "%s%s%s%d", prefix, playtoken, interfix,
			mix_id);
	js = fetch(url);
	free(url);
	if (js == NULL) {
		drawerror("Failed to play the first track.");
		return;
	}
	select_root = json_loads(js, 0, NULL);
	free(js);
	if (select_root == NULL) {
		drawerror("Failed to play the first track.");
		return;
	}
	track_number = 0;
	trackplay();
	drawplaylistupdate();
	drawheader();
}

static void
tracknext(void)
{
	char *js, *url;
	char prefix[] = "http://8tracks.com/sets/";
	char interfix[] = "/next?mix_id=";
	size_t len;

	len = strlen(prefix) + strlen(playtoken) + strlen(interfix) +
		intlen(mix_id) + 1;
	url = malloc(len * sizeof(char));
	if (url == NULL)
		err(1, NULL);
	(void) snprintf(url, len, "%s%s%s%d", prefix, playtoken, interfix,
			mix_id);
	js = fetch(url);
	free(url);
	if (js == NULL) {
		drawerror("Failed to play next track.");
		return;
	}
	select_root = json_loads(js, 0, NULL);
	free(js);
	trackplay();
	drawplaylistupdate();
	drawheader();
}

/* For some reason requesting a similar mix often returns the same mix.
 * Which is a bit too similar for me.  Therefore we request the next mix
 * based on the used search string.  Hopefully this will fix the problem.
 */
static void
mixnext(void)
{
	char *js, *url;
	char prefix[] = "http://8tracks.com/sets/";
	char interfixone[] = "/next_mix?mix_id=";
	char interfixtwo[] = "&smart_id=";
	size_t len;
	json_t *next_mix, *id, *name;

	len = strlen(prefix) + strlen(playtoken) + strlen(interfixone) +
		intlen(mix_id) + strlen(interfixtwo) + strlen(searchstr_last) +
		1;
	url = malloc(len * sizeof(char));
	if (url == NULL)
		err(1, NULL);
	(void) snprintf(url, len, "%s%s%s%d%s%s", prefix, playtoken,
			interfixone, mix_id, interfixtwo, searchstr_last);
	js = fetch(url);
	free(url);
	if (js == NULL) {
		drawerror("Could not retrieve the next mix.");
		return;
	}
	select_root = json_loads(js, 0, NULL);
	free(js);
	next_mix = json_object_get(select_root, "next_mix");
	id = json_object_get(next_mix, "id");
	if (id == NULL) {
		drawerror("Could not retrieve the next mix.");
		return;
	}
	mix_id = (int) json_integer_value(id);
	name = json_object_get(next_mix, "name");
	if (mix_name != NULL)
		free(mix_name);
	len = strlen(json_string_value(name)) + 1;
	mix_name = malloc(len * sizeof(char));
	if (mix_name == NULL)
		err(1, NULL);
	(void) snprintf(mix_name, len, "%s", json_string_value(name));
	json_decref(select_root);

	drawplaylistinit();
	drawfooter();
	trackfirst();
}

static void
playpause(void)
{
	libvlc_media_player_pause(vlc_mp);
}

/* Less checking is needed here, we already know that the json object
 * is usable.
 */
static void
selectmix(void)
{
	json_t *mix_set, *mixes, *data, *name, *id;
	size_t len;

	mix_set = json_object_get(select_root, "mix_set");
	mixes = json_object_get(mix_set, "mixes");
	data = json_array_get(mixes, select_pos);
	name = json_object_get(data, "name");
	id = json_object_get(data, "id");
	if (mix_name != NULL)
		free(mix_name);
	len = strlen(json_string_value(name)) + 1;
	mix_name = malloc(len * sizeof(char));
	(void) snprintf(mix_name, len, "%s", json_string_value(name));
	mix_id = (int) json_integer_value(id);
	json_decref(select_root);
	    
	state = PLAYING;
	drawplaylistinit();
	drawfooter();
	trackfirst();
}

static void
selectchangepos(wint_t c)
{
	json_t *mix_set, *mixes;
	size_t len;

	mix_set = json_object_get(select_root, "mix_set");
	if (mix_set == NULL)
		return;
	mixes = json_object_get(mix_set, "mixes");
	if (mixes == NULL)
		return;
	len = json_array_size(mixes);

	switch (c) {
	case KEY_UP:	select_pos = mod(select_pos-1, len); break;
	case KEY_DOWN:	select_pos = mod(select_pos+1, len); break;
	default:	break;
	}
	drawselect();
}

static void
selectescape(void)
{
	json_decref(select_root);
	state = pstate;
	drawbody();
	drawfooter();
}

static void
trackskip(void)
{
	char *js, *url;
	char prefix[] = "http://8tracks.com/sets/";
	char interfix[] = "/skip?mix_id=";
	size_t len;

	if (track_skip_allowed == FALSE) {
		drawerror("Skip not allowed.");
		return;
	}
	if (track_last == TRUE) {
		drawerror("Playing the last track.  Skipping to next mix.");
		mixnext();
		return;
	}

	len = strlen(prefix) + strlen(playtoken) + strlen(interfix) +
		intlen(mix_id) + 1;
	url = malloc(len * sizeof(char));
	if (url == NULL)
		err(1, NULL);
	(void) snprintf(url, len, "%s%s%s%d", prefix, playtoken, interfix,
			mix_id);
	js = fetch(url);
	free(url);
	if (js == NULL) {
		drawerror("Failed to skip track.");
		return;
	}
	select_root = json_loads(js, 0, NULL);
	free(js);
	if (select_root == NULL) {
		drawerror("Failed to skip track.");
		return;
	}

	trackplay();
	drawplaylistupdate();
	drawheader();
}

static void
trackplay(void)
{
	json_t *set, *at_last_track, *skip_allowed, *track_file_stream_url,
	       *name, *performer, *track, *id;
	libvlc_media_t *m;
	size_t len;

	set = json_object_get(select_root, "set");
	at_last_track = json_object_get(set, "at_last_track");
	skip_allowed = json_object_get(set, "skip_allowed");
	track = json_object_get(set, "track");
	id = json_object_get(track, "id");
	name = json_object_get(track, "name");
	performer = json_object_get(track, "performer");
	track_file_stream_url = json_object_get(track, "track_file_stream_url");
	if (name != NULL && performer != NULL) {
		if (track_name != NULL)
			free(track_name);
		len = strlen(json_string_value(performer)) +
			strlen(json_string_value(name)) + strlen(" - ") + 1;
		track_name = malloc(len * sizeof(char));
		if (track_name == NULL)
			err(1, NULL);
		(void) snprintf(track_name, len, "%s - %s",
				json_string_value(performer),
				json_string_value(name));
	}
	if (id != NULL)
		track_id = (int) json_integer_value(id);
	track_reported = FALSE;
	if (skip_allowed != NULL) {
		if (json_is_true(skip_allowed))
			track_skip_allowed = TRUE;
		else
			track_skip_allowed = FALSE;
	}
	if (at_last_track != NULL) {
		if (json_is_true(at_last_track))
			track_last = TRUE;
		else
			track_last = FALSE;
	}
	if (track_file_stream_url == NULL) {
		json_decref(select_root);
		drawerror("Error retrieving song.");
		return;
	}
	track_ended = FALSE;
	track_number++;
	m = libvlc_media_new_location(vlc_inst,
				      json_string_value(track_file_stream_url));
	libvlc_media_player_set_media(vlc_mp, m);
	libvlc_media_release(m);
	libvlc_media_player_play(vlc_mp);
	json_decref(select_root);
}

static void
trackreport(void)
{
	char *js, *url;
	char prefix[] = "http://8tracks.com/sets/";
	char interfixone[] = "/report?track_id=";
	char interfixtwo[] = "&mix_id=";
	size_t len;

	len = strlen(prefix) + strlen(playtoken) + strlen(interfixone) +
	      intlen(track_id) + strlen(interfixtwo) + intlen(mix_id) + 1;
	url = malloc(len * sizeof(char));
	if (url == NULL)
		err(1, NULL);
	(void) snprintf(url, len, "%s%s%s%d%s%d", prefix, playtoken,
			interfixone, track_id, interfixtwo, mix_id);
	js = fetch(url);
	free(js);
	free(url);
	track_reported = TRUE;
}

static void
search(void)
{
	char *js, *tmp, *url;
	char basic[] = "all"; /* default smart id */
	char prefix[] = "http://8tracks.com/mix_sets/";
	char suffix[] = "?include=mixes";
	int tmp_len;
	size_t len;
	struct searchstr_node *it;

	/* Build the url string.
	 * If no search string is entered, default to smart id "all" */
	if (searchstr_last != NULL)
		free(searchstr_last);
	if (searchstr_length() == 0) {
		len = strlen(prefix) + strlen(basic) + strlen(suffix) + 1;
		url = malloc(len * sizeof(char));
		if (url == NULL)
			err(1, NULL);
		(void) snprintf(url, len, "%s%s%s", prefix, basic, suffix);

		len = strlen(basic) + 1;
		searchstr_last = malloc(len * sizeof(char));
		if (searchstr_last == NULL)
			err(1, NULL);
		(void) snprintf(searchstr_last, len, "%s", basic);
	} else {
		/* Convert wchar* search string to char*.
		 * Store the result in searchstr_last.  We need it later if
		 * the user wants to play the next mix.
		 */
		len = searchstr_length() * MB_CUR_MAX + 1;
		searchstr_last = malloc(len * sizeof(char));
		if (searchstr_last == NULL)
			err(1, NULL);
		searchstr_last[0] = '\0';
		tmp = malloc(MB_CUR_MAX * sizeof(char));
		if (tmp == NULL)
			err(1, NULL);
		for (it = searchstr_head; it != NULL; it = it->next) {
			tmp_len = wctomb(tmp, it->c);
			tmp[tmp_len] = '\0';
			(void) strlcat(searchstr_last, tmp, len);
		}
		free(tmp);

		len = strlen(prefix) + strlen(searchstr_last) + strlen(suffix) +
			1;
		url = malloc(len * sizeof(char));
		if (url == NULL)
			err(1, NULL);
		(void) snprintf(url, len, "%s%s%s", prefix, searchstr_last,
			suffix);
	}
	searchstr_clear();

	/* Start the search */
	drawsearch_wait();
	js = fetch(url);
	free(url);
	if (js == NULL) {
		state = pstate;
		drawerror("Search failed.");
		drawbody();
		drawfooter();
		return;
	}
	/* Don't forget to decrement to reference count of
	 * the json object when we are done selecting. */
	select_root = json_loads(js, 0, NULL);
	free(js);
	if (select_root == NULL) {
		state = pstate;
		drawerror("Search failed.");
		drawbody();
		drawfooter();
		return;
	}
	select_pos = 0;
	state = SELECT;
	drawselect();
	drawfooter();
}

static void
searchaddchar(wint_t c)
{
	searchstr_push(c);
	searchstr_pos++;
	drawfooter();
}

static void
searchbackspace(void)
{
	if (searchstr_pos > 0) {
		searchstr_pos--;
		searchstr_pop();
	}
}

static void
searchchangepos(wint_t c)
{
	switch (c) {
	case KEY_LEFT:
		if (searchstr_pos > 0)
			searchstr_pos--;
		break;
	case KEY_RIGHT:
		if (searchstr_pos < (int)searchstr_length())
			searchstr_pos++;
		break;
	default:
		break;
	}
	drawfooter();
}

static void
searchdelete(void)
{
	searchstr_pop();
}

static void
searchescape(void)
{
	searchstr_clear();
	state = pstate;
	drawbody();
	drawfooter();
}

static void
searchinit(void)
{
	pstate = state;
	state = SEARCH;
	searchstr_clear();
	drawbody();
	drawfooter();
}

static void
searchstr_clear(void)
{
	struct searchstr_node *it, *tmp;

	it = searchstr_head;
	while (it != NULL) {
		tmp = it->next;
		free(it);
		it = tmp;
	}
	searchstr_pos = 0;
	searchstr_head = NULL;
}

static size_t
searchstr_length(void)
{
	struct searchstr_node *it;
	size_t len;

	len = 0;
	for (it = searchstr_head; it != NULL; it = it->next)
		len++;

	return len;
}

/* Delete wchar from the list at cursor position (searchstr_pos) */
static void
searchstr_pop(void)
{
	struct searchstr_node *it, *tmp;
	int i;

	if (searchstr_head == NULL)
		return;
	if (searchstr_pos >= (int)searchstr_length())
		return;
	it = searchstr_head;
	if (searchstr_pos == 0) {
		searchstr_head = it->next;
		free(it);
	} else {
		for (i = 1; i < searchstr_pos; i++)
			it = it->next;
		tmp = it->next;
		it->next = it->next->next;
		free(tmp);
	}
	drawfooter();
}

/* Push wchar on the list at cursor position (searchstr_pos) */
static void
searchstr_push(wint_t c)
{
	struct searchstr_node *it, *new;
	int i;

	new = malloc(sizeof(struct searchstr_node));
	if (new == NULL)
		err(1, NULL);
	new->c = c;
	new->next = NULL;

	if (searchstr_pos == 0) {
		new->next = searchstr_head;
		searchstr_head = new;
	} else {
		it = searchstr_head;
		for (i = 1; i < searchstr_pos; i++)
			it = it->next;
		new->next = it->next;
		it->next = new;
	}
}

void
vlc_event_trackend(const struct libvlc_event_t *event, void *data)
{
	track_ended = TRUE;
}

void
vlc_event_error(const struct libvlc_event_t *event, void *data)
{
	/* TODO:
	 * - Handle error
	 */
}

int
main(void)
{
	int err, quit;
	wint_t c;

	checklocale();
	initncurses();
	initvlc();
	initglobals();

	state = START;
	quit = FALSE;
	while (quit == FALSE) {
		if (state == PLAYING) {
			if (track_ended == TRUE) {
				if (track_last == TRUE)
					mixnext();
				else
					tracknext();
			} else if (track_reported == FALSE &&
				   needtoreport() == TRUE)
					trackreport();
		}
		err = get_wch(&c);
		quit = handlekey(err, c);
	}

	cleanupvlc();
	cleanupncurses();
	return 0;
}

