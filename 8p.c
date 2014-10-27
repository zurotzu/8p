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

#define _XOPEN_SOURCE_EXTENDED

#include <bsd/string.h>
#include <err.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include <curl/curl.h>
#include <jansson.h>
#include <ncurses.h>
#include <vlc/vlc.h>

#define APIKEY "e233c13d38d96e3a3a0474723f6b3fcd21904979"
#define DELAY 15
#define ESCDELAY 10
#define BODY_HEIGHT 800
#define FOOTER_HEIGHT 1
#define HEADER_HEIGHT 2

enum states {START, SEARCH, SELECT, PLAYING};

struct buf_t {
	char	*data;
	size_t	 size;
};
struct node_t {
	struct node_t *next;
	wchar_t c;
};
struct screen_t {
	WINDOW	*body;
	WINDOW	*footer;
	WINDOW	*header;
	WINDOW	*mainwindow;
	WINDOW	*search;
	WINDOW	*select;
	int	 pos;
	int	 y;
};
struct select_t {
	json_t	*root;
	int	 pos;
};
struct play_t {
	bool	 last_track;
	bool	 skip_allowed;
	bool	 reported;
	bool	 songended;
	char	 mix_name[100];
	char	 track_name[100];
	int	 mix_id;
	int	 track_id;
	int	 i;
};

static	void	 checklocale(void);
static	size_t	 curlwrite(void *, size_t, size_t, void *);
static	void	 drawbody(void);
static	void	 drawborders(void);
static	void	 drawfooter(void);
static	void	 drawheader(void);
static	void	 drawplaylist(void);
static	void	 drawsearch(void);
static	void	 drawselect(void);
static	void	 drawwelcome(void);
static	void	 exitsearch(void);
static	void	 exitselect(void);
static	char	*fetch(char *);
static	char	*getplaytoken(void);
static	void	 handleresize(void);
static	void	 handlesearchkey(wint_t);
static	void	 handlesearchpos(wint_t);
static	void	 initncurses(void);
static	void	 initsearch(void);
static	void	 listclean(void);
static	int	 listlength(void);
static	void	 listpop(void);
static	void	 listprint(void);
static	void	 listpush(wint_t);
static	int	 mod(int, int);
static	void	 playfirst(void);
static	void	 playnext(void);
static	void	 playselect(void);
static	void	 playskip(void);
static	void	 playsong(void);
static	void	 report(void);
static	void	 search(void);
void		 songend(const struct libvlc_event_t *, void *);
static	void	 updateplaylist(void);

static char *playtoken;
static int pstate, state;
static libvlc_instance_t *vlc_inst;
static libvlc_media_player_t *vlc_mp;
static struct node_t *head;
static struct play_t play;
static struct screen_t screen;
static struct select_t selection;

static void
checklocale(void)
{
	char *locale;

	locale = setlocale(LC_ALL, "");
	if (locale == NULL || 
	    (strstr(locale, "UTF-8") == NULL &&
	    strstr(locale, "utf-8") == NULL &&
	    strstr(locale, "UTF8") == NULL &&
	    strstr(locale, "utf8") == NULL))
		errx(1, "UTF-8 locale expected");
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

	switch (state)
	{
	case SEARCH:
		win = screen.search;
		break;
	case SELECT:
		win = screen.select;
		break;
	default:
		win = screen.body;
		break;
	}
	(void) prefresh(win, 0, 0, HEADER_HEIGHT+3, 2, LINES-4-FOOTER_HEIGHT,
	    COLS-4);
}

static void
drawplaylist(void)
{
	(void) wclear(screen.body);
	(void) mvwprintw(screen.body, 0, 0, "Playlist");
	(void) mvwprintw(screen.body, 1, 0, "--------");
	screen.y = 3;
	drawbody();
}

static void
updateplaylist(void)
{
	(void) mvwprintw(screen.body, screen.y, 0, "%d. %s\n",
	    play.i, play.track_name);
	screen.y = getcury(screen.body);
	drawbody();
}

static void
drawborders(void)
{
	WINDOW *win = screen.mainwindow;
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
		    y == LINES-FOOTER_HEIGHT-2) && LINES > HEADER_HEIGHT+2) {
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
	(void) wclear(screen.footer);
	(void) mvwprintw(screen.footer, 0, 0, "Warning: %s", err);
	(void) curs_set(0);
	(void) prefresh(screen.footer, 0, 0, LINES-FOOTER_HEIGHT-1, 2,
	    LINES-1, COLS-4);
	(void) sleep(2);
	drawfooter();
}

static void
drawfooter(void)
{
	char playing[] = "q Quit  s Search  p Play/Pause  n Next track"
	    "  N Next mix";
	char search[] = "ESC Exit |  Search: ";
	char select[] = "ESC Exit  Enter Select";
	char start[] = "q Quit  s Search";
	
	(void) wclear(screen.footer);
	switch (state) {
	case PLAYING:
		(void) mvwprintw(screen.footer, 0, 0, "%s", playing);
		(void) curs_set(0);
		break;
	case START:
		(void) mvwprintw(screen.footer, 0, 0, "%s", start);
		(void) curs_set(0);
		break;
	case SEARCH:
		(void) mvwprintw(screen.footer, 0, 0, "%s", search);
		listprint();
		(void) curs_set(1);
		(void) wmove(screen.footer, 0,
		    ((int) strlen(search))+screen.pos);
		break;
	case SELECT:
		(void) mvwprintw(screen.footer, 0, 0, "%s", select);
		(void) curs_set(0);
		break;
	default:
		(void) curs_set(0);
		break;
	}
	(void) prefresh(screen.footer, 0, 0, LINES-FOOTER_HEIGHT-1, 2,
	    LINES-1, COLS-4);
}

static void
drawheader(void)
{
	(void) wclear(screen.header);
	(void) mvwprintw(screen.header, 0, 0, "Mix:   %s", play.mix_name);
	(void) mvwprintw(screen.header, 1, 0, "Track: %s", play.track_name);
	(void) prefresh(screen.header, 0, 0, 1, 2, HEADER_HEIGHT, COLS-4);
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

	(void) wclear(screen.search);
	for (i = 0; i < sizeof(txt)/sizeof(txt[0]); i++)
		(void) mvwprintw(screen.search, i, 0, "%s", txt[i]);
}

static void
drawselect(void)
{
	int count, i;
	json_t *data, *descr, *mix_set, *mixes, *name, *tags, *plays, *likes;
	size_t len;

	mix_set = json_object_get(selection.root, "mix_set");
	if (mix_set == NULL) {
		json_decref(selection.root);
		drawerror("Search returned no results.");
		return;
	}
	mixes = json_object_get(mix_set, "mixes");
	if (!json_is_array(mixes)) {
		json_decref(selection.root);
		drawerror("Search returned no results.");
		return;
	}
	len = json_array_size(mixes);
	(void) wclear(screen.select);
	(void) wmove(screen.select, 0,0);
	count = 0;
	i = selection.pos;
	while (count < len) {
		i = mod(i, len);
		data = json_array_get(mixes, i);
		name = json_object_get(data, "name");
		tags = json_object_get(data, "tag_list_cache");
		descr = json_object_get(data, "description");
		plays = json_object_get(data, "plays_count");
		likes = json_object_get(data, "likes_count");
		if (i == mod(selection.pos, len))
			(void) wattron(screen.select, A_REVERSE);
		(void) wattron(screen.select, A_BOLD);
		(void) wprintw(screen.select, "%d. %s\n", i+1,
		    json_string_value(name));
		(void) wattroff(screen.select, A_BOLD);
		if (i == mod(selection.pos, len))
			(void) wattroff(screen.select, A_REVERSE);
		(void) wattron(screen.select, A_BOLD);
		(void) wprintw(screen.select, "Description:\n");
		(void) wattroff(screen.select, A_BOLD);
		(void) wprintw(screen.select, "%s\n", json_string_value(descr));
		(void) wattron(screen.select, A_BOLD);
		(void) wprintw(screen.select, "Tags:\n");
		(void) wattroff(screen.select, A_BOLD);
		(void) wprintw(screen.select, "%s\n", json_string_value(tags));
		(void) wattron(screen.select, A_BOLD);
		(void) wprintw(screen.select, "Number of plays:\n");
		(void) wattroff(screen.select, A_BOLD);
		(void) wprintw(screen.select, "%d\n",
		    (int) json_integer_value(plays));
		(void) wattron(screen.select, A_BOLD);
		(void) wprintw(screen.select, "Number of likes:\n");
		(void) wattroff(screen.select, A_BOLD);
		(void) wprintw(screen.select, "%d\n",
		    (int) json_integer_value(likes));
		(void) wprintw(screen.select, "\n---\n\n");
		i++;
		count++;
	}
	drawbody();
}

static void
drawwelcome(void)
{
	(void) wclear(screen.body);
	(void) mvwprintw(screen.body, 0, 0, "Welcome to 8p.");
	(void) mvwprintw(screen.body, 1, 0,
	    "Press \"s\" to start searching for 8tracks.com mixes.");
	drawbody();
}

static void
exitsearch(void)
{
	listclean();
	state = pstate;
	drawfooter();
	drawbody();
}

static void
exitselect(void)
{
	json_decref(selection.root);
	state = pstate;
	drawfooter();
	drawbody();
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
handlesearchkey(wint_t c)
{
	listpush(c);
	screen.pos++;
	drawfooter();
}

static void
handlesearchpos(wint_t c)
{
	if (c == KEY_LEFT) {
		if (screen.pos > 0)
			screen.pos--;
	} else if (c == KEY_RIGHT) {
		if (screen.pos < listlength())
			screen.pos++;
	}
	drawfooter();
}

static void
initncurses(void)
{
	screen.mainwindow = initscr();
	if (screen.mainwindow == NULL)
		errx(1, "failed to initialize ncurses");
	if ((meta(screen.mainwindow, true) == ERR) ||
	    (keypad(screen.mainwindow, true) == ERR) ||
	    (noecho() == ERR) ||
	    (halfdelay(DELAY) == ERR) ||
	    (curs_set(0) == ERR) ||
	    (set_escdelay(ESCDELAY) == ERR)) {
		(void) endwin();
		errx(1, "failed to initialize ncurses");
	}
	screen.header = newpad(HEADER_HEIGHT, 200);
	screen.body = newpad(BODY_HEIGHT, 200);
	screen.search = newpad(BODY_HEIGHT, 200);
	screen.select = newpad(BODY_HEIGHT, 200);
	screen.footer = newpad(FOOTER_HEIGHT, 200);
	screen.y = 0;
	screen.pos = 0;

	drawborders();
	drawsearch();
	drawwelcome();
	drawfooter();
	drawheader();
}

static void
initsearch(void)
{
	pstate = state;
	state = SEARCH;
	head = NULL;
	screen.pos = 0;
	drawbody();
	drawfooter();
}

static char *
fetch(char *url)
{
	CURL *curl;
	CURLcode curl_err;
	char *ret;
	char key[60];
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
	(void) snprintf(key, sizeof(key), "X-Api-Key: %s", APIKEY);
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
		strlcpy(ret, buf.data, len);
	} else
		ret = NULL;

	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);
	curl_global_cleanup();
	free(buf.data);

	return ret;
}

static char *
getplaytoken(void) {
	char *js, *ret;
	char url[] = "http://8tracks.com/sets/new";
	json_error_t error;
	json_t *playtoken, *root;
	size_t len;

	js = fetch(url);
	if (js == NULL)
		return NULL;
	root = json_loads(js, 0, &error);
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
	ret = malloc(len*sizeof(char));
	strlcpy(ret, json_string_value(playtoken), len); 
	json_decref(root);

	return ret;
}

static void
listclean(void)
{
	struct node_t *it, *tmp;

	it = head;
	while (it != NULL) {
		tmp = it->next;
		free(it);
		it = tmp;
	}
	screen.pos = 0;
	head = NULL;
}

static int
listlength(void)
{
	struct node_t *it;
	int i;

	i = 0;
	it = head;
	while (it != NULL) {
		it = it->next;
		i++;
	}
	return i;
}

static void
listpop(void)
{
	struct node_t *it, *tmp;
	int i;

	if (head == NULL)
		return;
	if (screen.pos > listlength()-1)
		return;
	it = head;
	if (screen.pos == 0) {
		head = it->next;
		free(it);
	} else {
		for (i = 1; i < screen.pos; i++)
			it = it->next;
		tmp = it->next;
		it->next = it->next->next;
		free(tmp);
	}
	drawfooter();
}

static void
listprint(void)
{
	struct node_t *it;

	it = head;
	while (it != NULL) {
		wprintw(screen.footer, "%lc", it->c);
		it = it->next;
	}
}

static void
listpush(wint_t c)
{
	struct node_t *it, *new;
	int i;

	new = malloc(sizeof(struct node_t));
	if (new == NULL)
		err(1, NULL);
	new->c = c;
	new->next = NULL;

	if (screen.pos == 0) {
		new->next = head;
		head = new;
	} else {
		it = head;
		for (i = 1; i < screen.pos; i++)
			it = it->next;
		new->next = it->next;
		it->next = new;
	}
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
playfirst(void)
{
	char *js, *url;
	size_t len;
	json_error_t error;

	if (playtoken == NULL)
		playtoken = getplaytoken();
	if (playtoken == NULL) {
		drawerror("Could not get playtoken. Stopped playback.");
		state = START;
		return;
	}

	len = snprintf(NULL, 0, "%d", play.mix_id) + strlen(playtoken) + 37 + 1;
	url = malloc(len*sizeof(char));
	if (url == NULL)
		err(1, NULL);
	snprintf(url, len, "http://8tracks.com/sets/%s/play?mix_id=%d",
	    playtoken, play.mix_id);
	js = fetch(url);
	free(url);
	if (js == NULL) {
		drawerror("Failed to play the first track.");
		return;
	}
	selection.root = json_loads(js, 0, &error);
	free(js);
	play.i = 0;
	playsong();
}

static void
playnext(void)
{
	char *js, *url;
	size_t len;
	json_error_t error;

	if (playtoken == NULL)
		return;
	len = snprintf(NULL, 0, "%d", play.mix_id) + strlen(playtoken) + 37 + 1;
	url = malloc(len*sizeof(char));
	if (url == NULL)
		err(1, NULL);
	snprintf(url, len, "http://8tracks.com/sets/%s/next?mix_id=%d",
	    playtoken, play.mix_id);
	js = fetch(url);
	free(url);
	if (js == NULL) {
		drawerror("Failed to play next track.");
		return;
	}
	selection.root = json_loads(js, 0, &error);
	free(js);
	playsong();
	updateplaylist();
	drawheader();
}

static void
playnextmix(void)
{
	char *js, *url;
	size_t len;
	json_error_t error;
	json_t *next_mix, *id, *name;

	if (playtoken == NULL)
		return;

	len = snprintf(NULL, 0, "%d", play.mix_id)*2 + strlen(playtoken) +
	    snprintf(NULL, 0, "http://8tracks.com/sets//next_mix.json?mix_id="
	    "&smart_id=similar:") + 1;
	url = malloc(len*sizeof(char));
	if (url == NULL)
		err(1, NULL);
	snprintf(url, len, "http://8tracks.com/sets/%s/next_mix.json?mix_id=%d"
	    "&smart_id=similar:%d", playtoken, play.mix_id, play.mix_id);
	js = fetch(url);
	free(url);
	if (js == NULL) {
		drawerror("Could not retrieve the next mix.");
		return;
	}
	selection.root = json_loads(js, 0, &error);
	free(js);
	next_mix = json_object_get(selection.root, "next_mix");
	id = json_object_get(next_mix, "id");
	if (id == NULL) {
		drawerror("Could not retrieve the next mix.");
		return;
	}
	play.mix_id = (int) json_integer_value(id);
	name = json_object_get(next_mix, "name");
	(void) snprintf(play.mix_name, sizeof(play.mix_name), "%s",
	    json_string_value(name));
	json_decref(selection.root);

	drawplaylist();
	playfirst();
	drawheader();
	updateplaylist();
	drawfooter();
}

static void
playpause(void)
{
	libvlc_media_player_pause(vlc_mp);
}

static void
playselect(void)
{
	json_t *mix_set, *mixes, *data, *name, *id;

	mix_set = json_object_get(selection.root, "mix_set");
	mixes = json_object_get(mix_set, "mixes");
	data = json_array_get(mixes,
	    mod(selection.pos, json_array_size(mixes)));
	name = json_object_get(data, "name");
	id = json_object_get(data, "id");
	(void) snprintf(play.mix_name, sizeof(play.mix_name), "%s",
	    json_string_value(name));
	play.mix_id = (int) json_integer_value(id);
	json_decref(selection.root);
	    
	state = PLAYING;
	drawplaylist();
	playfirst();
	drawheader();
	updateplaylist();
	drawfooter();
}

static void
playskip(void)
{
	char *js, *url;
	size_t len;
	json_error_t error;

	if (playtoken == NULL)
		return;
	if (!play.skip_allowed) {
		drawerror("Skip not allowed.");
		return;
	}
	if (play.last_track) {
		playnextmix();
		return;
	}

	len = snprintf(NULL, 0, "%d", play.mix_id) + strlen(playtoken) + 37 + 1;
	url = malloc(len*sizeof(char));
	if (url == NULL)
		err(1, NULL);
	snprintf(url, len, "http://8tracks.com/sets/%s/skip?mix_id=%d",
	    playtoken, play.mix_id);
	js = fetch(url);
	free(url);
	if (js == NULL) {
		drawerror("Failed to skip track.");
		return;
	}
	selection.root = json_loads(js, 0, &error);
	free(js);

	playsong();
	updateplaylist();
	drawheader();
}

static void
playsong(void)
{
	json_t *set, *at_last_track, *skip_allowed, *track_file_stream_url,
	       *name, *performer, *track, *id;
	libvlc_media_t *m;

	set = json_object_get(selection.root, "set");
	at_last_track = json_object_get(set, "at_last_track");
	skip_allowed = json_object_get(set, "skip_allowed");
	track = json_object_get(set, "track");
	id = json_object_get(track, "id");
	name = json_object_get(track, "name");
	performer = json_object_get(track, "performer");
	track_file_stream_url = json_object_get(track, "track_file_stream_url");
	if (name != NULL && performer != NULL) {
		snprintf(play.track_name, sizeof(play.track_name), "%s - %s",
		    json_string_value(performer), json_string_value(name));
	}
	if (id != NULL)
		play.track_id = (int) json_integer_value(id);
	play.reported = false;
	if (skip_allowed != NULL) {
		if (json_is_true(skip_allowed))
			play.skip_allowed = true;
		else
			play.skip_allowed = false;
	}
	if (at_last_track != NULL) {
		if (json_is_true(at_last_track))
			play.last_track = true;
		else
			play.last_track = false;
	}
	if (track_file_stream_url == NULL) {
		json_decref(selection.root);
		return;
	}
	play.songended = false;
	play.i++;
	m = libvlc_media_new_location(vlc_inst,
	    json_string_value(track_file_stream_url));
	libvlc_media_player_set_media(vlc_mp, m);
	libvlc_media_release(m);
	libvlc_media_player_play(vlc_mp);
	json_decref(selection.root);
}

static void
report(void)
{
	char *url;
	char *js;
	size_t len;

	len = 55 + strlen(playtoken) + snprintf(NULL, 0, "%d", play.track_id) +
	    snprintf(NULL, 0, "%d", play.mix_id) + 1;
	url = malloc(len*sizeof(char));
	if (url == NULL)
		err(1, NULL);
	snprintf(url, len,
	    "http://8tracks.com/sets/%s/report?track_id=%d&mix_id=%d",
	    playtoken, play.track_id, play.mix_id);
	js = fetch(url);
	free(js);
	free(url);
	play.reported = true;
}

static void
search(void)
{
	char *js, *tmp, *url;
	char basic[] = "all"; /* default smart id */
	char prefix[] = "http://8tracks.com/mix_sets/";
	char suffix[] = "?include=mixes";
	int tmp_len;
	json_error_t error;
	size_t len;
	struct node_t *it;

	if (listlength() == 0) {
		len = strlen(prefix) + strlen(basic) + strlen(suffix) + 1;
		url = malloc(len * sizeof(char));
		if (url == NULL)
			err(1, NULL);
		(void) snprintf(url, len, "%s%s%s", prefix, basic, suffix);
	} else {
		len = strlen(prefix) + listlength() * MB_CUR_MAX +
		    strlen(suffix) + 1;
		url = malloc(len * sizeof(char));
		if (url == NULL)
			err(1, NULL);
		(void) snprintf(url, len, "%s", prefix);

		tmp = malloc(MB_CUR_MAX * sizeof(char));
		if (tmp == NULL)
			err(1, NULL);
		it = head;
		while (it != NULL) {
			tmp_len = wctomb(tmp, it->c);
			tmp[tmp_len] = '\0';
			(void) strlcat(url, tmp, len);
			it = it->next;
		}
		free(tmp);

		(void) strlcat(url, suffix, len);

		listclean();
	}
	js = fetch(url);
	free(url);
	if (js == NULL) {
		state = pstate;
		drawerror("Search failed.");
		return;
	}
	selection.root = json_loads(js, 0, &error);
	free(js);
	selection.pos = 0;
	state = SELECT;
	drawselect();
	drawfooter();
}

void
songend(const struct libvlc_event_t *event, void *data)
{
	play.songended = true;
}

int
main(void)
{
	libvlc_event_manager_t *vlc_em;
	libvlc_callback_t callback = songend;
	bool quit;
	int cerr;
	wint_t c;

	checklocale();
	initncurses();

	/* initialize vlc */
	vlc_inst = libvlc_new(0, NULL);
	vlc_mp = libvlc_media_player_new(vlc_inst);
	vlc_em = libvlc_media_player_event_manager(vlc_mp);
	libvlc_event_attach(vlc_em, libvlc_MediaPlayerEndReached, callback,
	    NULL);

	play.mix_id = 0;
	play.mix_name[0] = '\0';
	play.track_name[0] = '\0';
	play.songended = false;
	playtoken = NULL;
	quit = false;
	state = START;
	while (!quit) {
		cerr = get_wch(&c);

		if (play.songended && state == PLAYING) {
			if (play.last_track)
				playnextmix();
			else
				playnext();
		}
		if (!play.reported && state == PLAYING) {
			if (libvlc_media_player_get_time(vlc_mp) >= (30 * 1000))
				report();
		}

		if (cerr == ERR)
			continue;
		if (c == KEY_RESIZE) {
			handleresize();
			continue;
		}
		switch (state) {
		case PLAYING:
			switch (cerr) {
			case OK:
				switch (c) {
				case 'q':
					quit = true;
					break;
				case 's':
					initsearch();
					break;
				case 'n':
					playskip();
					break;
				case 'p':
					playpause();
					break;
				case 'N':
					playnextmix();
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
			break;
		case SEARCH:
			switch (cerr) {
			case KEY_CODE_YES:
				switch (c) {
				case KEY_BACKSPACE:
					if (screen.pos > 0) {
						screen.pos--;
						listpop();
					}
					break;
				case KEY_DC:
					listpop();
					break;
				case KEY_ENTER:
					search();
					break;
				case KEY_LEFT:
				case KEY_RIGHT:
					handlesearchpos(c);
				default:
					break;
				}
				break;
			case OK:
				switch (c) {
				case 27: /* Escape */
					exitsearch();
					break;
				case 10: /* Enter */
					search();
					break;
				case 32: /* Spacebar */
					break;
				case 127: /* Backspace */
					if (screen.pos > 0) {
						screen.pos--;
						listpop();
					}
					break;
				default:
					handlesearchkey(c);
					break;
				}
				break;
			default:
				break;
			}
			break;
		case SELECT:
			switch (cerr) {
			case KEY_CODE_YES:
				switch (c) {
				case KEY_UP:
					selection.pos--;
					drawselect();
					break;
				case KEY_DOWN:
					selection.pos++;
					drawselect();
					break;
				case KEY_ENTER:
					playselect();
					break;
				default:
					break;
				}
			case OK:
				switch (c) {
				case 27: /* Escape */
					exitselect();
					break;
				case 10: /* Enter */
					playselect();
					break;
				default:
					break;
				}
			default:
				break;
			}
			break;
		case START:
			switch (cerr) {
			case OK:
				switch (c) {
				case 'q':
					quit = true;
					break;
				case 's':
					initsearch();
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}

	(void) endwin();
	if (vlc_mp != NULL)
		libvlc_media_player_release(vlc_mp);
	if (vlc_inst != NULL)
		libvlc_release(vlc_inst);
	return 0;
}

