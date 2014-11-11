/* See LICENSE file for copyright and license details. */

#include "draw.h"

static void	drawheader(struct info *);
static void	drawbody(struct info *);
static void	drawstart(struct info *);
static void	drawsearch(struct info *);
static void	drawsearching(void);
static void	drawselect(struct info *);
static void	drawplay(struct info *);
static void	drawfooter(struct info *);
static void	drawbodyfill(int);
static int	nlprintw(int, int, int *, const char*, ...);

void
draw_exit(void)
{
	(void)endwin();
}

void
draw_init(void)
{
	int ret;

	if (initscr() == NULL)
		goto error;

	/* Set up ncurses environment */
	/* Hide cursor */
	ret = curs_set(0);
	if (ret == ERR)
		goto error;

	/* Set half-blocking mode and time */
	ret = halfdelay(HALFDELAY);
	if (ret == ERR)
		goto error;

	/* Make function keys available */
	ret = keypad(stdscr, true);
	if (ret == ERR)
		goto error;

	/* Disable echoing keys on screen */
	ret = noecho();
	if (ret == ERR)
		goto error;

	/* Prevent enter from creating a newline */
	ret = nonl();
	if (ret == ERR)
		goto error;

	/* Set short timeout for ESC */
	ret = set_escdelay(DELAYESC);
	if (ret == ERR)
		goto error;

	/* Clear the screen */
	(void)clear();

	return;

error:
	draw_exit();
	errx(1, "Failed to initialize ncurses.");
}

void
draw_redraw(struct info *data)
{
	(void)erase();
	drawheader(data);
	drawbody(data);
	drawfooter(data);
	(void)refresh();
}

static void
drawheader(struct info *data)
{
	char *mix, *track;
	int index;
	size_t len;

	if (data == NULL)
		return;

	if (LINES < 4)
		return;

	/* Get mix and track name */
	mix = NULL;
	track = NULL;
	if (data->m) {
		if (data->m->name)
			mix = data->m->name;
		index = data->m->track_count;
		if (index > 0) {
			len = strlen(data->m->track[index-1]->name) + 
			    strlen(" - ") +
			    strlen(data->m->track[index-1]->performer) + 1;
			track = malloc(len * sizeof(char));
			if (track == NULL)
				err(1, NULL);
			track[0] = '\0';
			(void)snprintf(track, len, "%s - %s",
			    data->m->track[index-1]->performer,
			    data->m->track[index-1]->name);
		}
	}

	/* First line */
	(void)mvaddch(0, 0, ACS_ULCORNER);
	(void)mvhline(0, 1, ACS_HLINE, COLS-2);
	(void)mvaddstr(0, 3, " 8p ");
	(void)mvaddch(0, COLS-1, ACS_URCORNER);

	/* Second line */
	(void)mvaddch(1, 0, ACS_VLINE);
	(void)mvaddch(1, COLS-1, ACS_VLINE);
	if (mix)
		(void)mvprintw(1, 2, "Mix:   %.*s", COLS-4-7, mix);
	else
		(void)mvprintw(1, 2, "Mix:");

	/* Third line */
	(void)mvaddch(2, 0, ACS_VLINE);
	(void)mvaddch(2, COLS-1, ACS_VLINE);
	if (track)
		(void)mvprintw(2, 2, "Track: %.*s", COLS-4-7, track);
	else
		(void)mvprintw(2, 2, "Track:");

	/* Fourth line */
	(void)mvaddch(3, 0, ACS_LLCORNER);
	(void)mvhline(3, 1, ACS_HLINE, COLS-2);
	(void)mvaddch(3, COLS-1, ACS_LRCORNER);

	if (track)
		free(track);
}

static void
drawbody(struct info *data)
{
	if (data == NULL)
		return;

	/* Header and footer get priority */
	if (LINES <= 6)
		return;

	(void)mvaddch(3, 0, ACS_LTEE);
	(void)mvaddch(3, COLS-1, ACS_RTEE);

	switch (data->state) {
	case START:	drawstart(data); break;
	case SEARCH:	drawsearch(data); break;
	case SEARCHING:	drawsearching(); break;
	case SELECT:	drawselect(data); break;
	case PLAY:	drawplay(data); break;
	default:	break;
	}
}

static void
drawbodyfill(int ln)
{
	for (; ln < LINES-3; ln++) {
		(void)mvaddch(ln, 0, ACS_VLINE);
		(void)mvaddch(ln, COLS-1, ACS_VLINE);
	}
}

static void
drawstart(struct info *data)
{
	int y;
	int scroll;

	scroll = data->scroll;
	y = nlprintw(4, FALSE, &scroll, "Welcome to 8p.");
	y = nlprintw(y, FALSE, &scroll,
	    "Press \"s\" to start searching for 8tracks.com mixes.");
	drawbodyfill(y);
}

static void
drawsearch(struct info *data)
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
	    "dj:1                        mixes published by the user with id=1",
	    "liked:1                     mixes liked by the user with id=1",
	    "similar:14                  mixes similar to mix with id=14",
	    "",
	    "Note: no spaces allowed."};
        int i;
	int y;
	int scroll;

	scroll = data->scroll;
	y = nlprintw(4, FALSE, &scroll, "%s", txt[0]);
        for (i = 1; i < (int)(sizeof(txt)/sizeof(txt[0])); i++)
                y = nlprintw(y, FALSE, &scroll, "%s", txt[i]);
	drawbodyfill(y);
}

static void
drawsearching(void)
{
	int scroll, y;

	scroll = 0;
	y = nlprintw(4, FALSE, &scroll, "Searching...");
	drawbodyfill(y);
}

static void
drawselect(struct info *data)
{
	int scroll, y;
	int i, j;

	scroll = data->scroll;

	if (data->mlist == NULL)
		goto error;
	if (data->mlist_size == 0)
		goto error;

	y = 4;
	for (j = 0, i = data->select_pos; j < (int)data->mlist_size; j++, i++) {
		i = mod(i, data->mlist_size);
		if (data->mlist[i] == NULL) {
			y = nlprintw(y, FALSE, &scroll, "-. Error");
			continue;
		}
		if (i == data->select_pos) {
			y = nlprintw(y, TRUE, &scroll, "%d. %s", i+1,
			    data->mlist[i]->name);
			y = nlprintw(y, FALSE, &scroll, "");
		} else {
			y = nlprintw(y, FALSE, &scroll, "%d. %s", i+1,
			    data->mlist[i]->name);
		}
		y = nlprintw(y, FALSE, &scroll, "\nDescription:\n%s",
		    data->mlist[i]->description);
		y = nlprintw(y, FALSE, &scroll, "Tags: %s",
		    data->mlist[i]->tags);
		y = nlprintw(y, FALSE, &scroll, "Number of plays: %d",
		    data->mlist[i]->plays_count);
		y = nlprintw(y, FALSE, &scroll, "Number of likes: %d",
		    data->mlist[i]->likes_count);
		y = nlprintw(y, FALSE, &scroll, "\n---\n\n");
	} 
	drawbodyfill(y);

	return;

error:
	y = nlprintw(4, FALSE, &scroll, "Search returned no results.");
	drawbodyfill(y);
}

static void
drawplay(struct info *data)
{
	int scroll, y, i;

	scroll = data->scroll;
	y = nlprintw(4, FALSE, &scroll, "Playlist");
	y = nlprintw(y, FALSE, &scroll, "--------");
	for (i = 0; i < data->m->track_count; i++) {
		y = nlprintw(y, FALSE, &scroll, "%d. %s - %s", i+1,
		    data->m->track[i]->performer,
		    data->m->track[i]->name);
	}
	drawbodyfill(y);
}

static void
drawfooter(struct info *data)
{
	char playing[] = "q Quit  s Search  p Play/Pause  n Next track"
	    "  N Next mix";
	char search[] = "ESC Exit |  Search: ";
	char select[] = "ESC Exit  Enter Select";
	char start[] = "q Quit  s Search";
	struct search_node *it;
	int cp, i;

	if (data == NULL)
		return;
	if (LINES < 6)
		return;

	(void)mvaddch(LINES-3, 0, ACS_LTEE);
	(void)mvhline(LINES-3, 1, ACS_HLINE, COLS-2);
	(void)mvaddch(LINES-3, COLS-1, ACS_RTEE);
	
	(void)mvaddch(LINES-2, 0, ACS_VLINE);
	(void)mvaddch(LINES-2, COLS-1, ACS_VLINE);

	(void)mvaddch(LINES-1, 0, ACS_LLCORNER);
	(void)mvhline(LINES-1, 1, ACS_HLINE, COLS-2);
	(void)mvaddch(LINES-1, COLS-1, ACS_LRCORNER);

	switch (data->state) {
	case PLAY:
		(void)mvprintw(LINES-2, 2, "%.*s", COLS-4, playing);
		(void)curs_set(0);
		break;
	case START:
		(void)mvprintw(LINES-2, 2, "%.*s", COLS-4, start);
		(void)curs_set(0);
		break;
	case SEARCH:
		(void)mvprintw(LINES-2, 2, "%.*s", COLS-4, search);
		for (it = data->slist_head; it != NULL; it = it->next)
			(void)printw("%lc", it->c);
		(void)curs_set(1);
		cp = strlen(search) + 2;
		it = data->slist_head;
		for (i = 0; i < data->cursor_pos; i++, it = it->next)
			cp += wcwidth(it->c);
		(void)move(LINES-2, cp);
		break;
	case SELECT:
		(void)mvprintw(LINES-2, 2, "%.*s", COLS-4, select);
		(void)curs_set(0);
		break;
	default:
		(void)curs_set(0);
		break;
	}
}

/* Print a line only if it is visible and wrap if the line
 * length is too long to fit.
 * NOTE: only usable for drawing in the body section.
 */
static int
nlprintw(int ln, int sel, int *scroll, const char *fmt, ...)
{
	va_list ap;
	char *str;
	wchar_t *wstr;
	int errn, i, slen;
	size_t wlen;

	/* Check if start line is visible */
	if (ln > LINES - 4) {
		return ln;
	}

	str = NULL;
	wstr = NULL;

	/* Get string length */
	va_start(ap, fmt);
	slen = vsnprintf(NULL, 0, fmt, ap);
	if (slen < 0)
		return ln;
	slen++;
	va_end(ap);

	/* Set string */
	str = malloc(slen * sizeof(char));
	if (str == NULL)
		err(1, NULL);
	va_start(ap, fmt);
	errn = vsnprintf(str, slen, fmt, ap);
	if (errn < 0)
		goto error;
	va_end(ap);

	/* Convert string to a wide-character string */
	wstr = malloc(slen * sizeof(wchar_t));
	if (wstr == NULL)
		err(1, NULL);
	wlen = mbstowcs(wstr, str, slen);
	if (wlen == (size_t)-1)
		goto error;

	/* Apply strict width rules to the string */
	errn = wwrap(&wstr, COLS-4);
	if (errn == ERROR)
		goto error;

	/* Print string */
	i = 0;
	if (*scroll > 0) {
		while(wstr[i] != L'\0' && *scroll != 0) {
			if (wstr[i] == L'\n')
				(*scroll)--;
			i++;
		}
		if (*scroll > 0) {
			(*scroll)--;
			goto error;
		}
	}
	while (ln < LINES - 3 && wstr[i] != L'\0') {
		(void)mvaddch(ln, 0, ACS_VLINE);
		(void)move(ln, 2);
		if (sel == TRUE)
			(void)attron(A_REVERSE);
		while(wstr[i] != L'\0' && wstr[i] != L'\n') {
			(void)printw("%lc", wstr[i]);
			i++;
		}
		if (sel == TRUE)
			(void)attroff(A_REVERSE);
		if (wstr[i] == L'\n') {
			(void)addch(wstr[i]);
			i++;
		}
		(void)mvaddch(ln, COLS-1, ACS_VLINE);
		ln++;
	}

	/* Cleanup */
	free(str);
	free(wstr);

	return ln;

error:
	if (str)
		free(str);
	if (wstr)
		free(wstr);

	return ln;
}
