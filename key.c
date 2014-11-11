/* See LICENSE file for copyright and license details. */

#include "key.h"

static void	key_handleplay(struct info *, int, wint_t);
static void	key_handlesearch(struct info *, int, wint_t);
static void	key_handleselect(struct info *, int, wint_t);
static void	key_handlestart(struct info *, int, wint_t);

void
key_handle(struct info *data)
{
	int errn;
	int step;
	wint_t c;

	if (data == NULL)
		return;

	/* Get key */
	errn = get_wch(&c);

	if (errn == ERR)
		return;
	if (c == KEY_RESIZE) {
		data->scroll = 0;
		draw_redraw(data);
		return;
	}
	/* Always allow scrolling */
	if (errn == KEY_CODE_YES) {
		step = (LINES-7)/3;
		if (step <= 0)
			step = 1;
		if (c == KEY_PPAGE) {
			data->scroll -= step;
			if (data->scroll < 0)
				data->scroll = 0;
			return;
		} else if (c == KEY_NPAGE) {
			data->scroll += step;
			return;
		}
	}

	switch (data->state) {
	case START:	key_handlestart(data, errn, c); break;
	case PLAY:	key_handleplay(data, errn, c); break;
	case SEARCH:	key_handlesearch(data, errn, c); break;
	case SELECT:	key_handleselect(data, errn, c); break;
	default:	break;
	}
}

static void
key_handlestart(struct info *data, int errn, wint_t c)
{
	if (errn != OK)
		return;
	switch (c) {
	case 'q':	data->quit = TRUE; break;
	case 's':	search_init(data); break;
	default:	break;
	}
}

static void
key_handleplay(struct info *data, int errn, wint_t c)
{
	if (errn != OK)
		return;
	switch (c) {
	case 'q':	data->quit = TRUE; break;
	case 's':	search_init(data); break;
	case 'n':	play_skip(data); break;
	case 'p':	play_togglepause(data); break;
	case 'N':	play_nextmix(data); break;
	default:	break;
	}
}

static void
key_handlesearch(struct info *data, int errn, wint_t c)
{
	switch (errn) {
	case KEY_CODE_YES:
		switch(c) {
		case KEY_BACKSPACE:	search_backspace(data); break;
		case KEY_DC:		search_delete(data); break;
		case KEY_ENTER:		search_search(data); break;
		case KEY_LEFT:		/* FALLTHROUGH */
		case KEY_RIGHT:		search_changepos(data, c); break;
		default:		break;
		}
		break;
	case OK:
		switch (c) {
		case L'\n':		/* FALLTHROUGH */
		case L'\r':		search_search(data); break;
		case 0x1b: /* ESC */	search_exit(data); break;
		case 127:		/* FALLTHROUGH */
		case L'\b':		search_backspace(data); break;
		default:		search_addchar(data, c); break;
		}
		break;
	default:
		break;
	}
}

static void
key_handleselect(struct info *data, int errn, wint_t c)
{
	switch (errn) {
	case KEY_CODE_YES:
		switch (c) {
		case KEY_UP:	/* FALLTHROUGH */
		case KEY_DOWN:	select_changepos(data, c); break;
		case KEY_ENTER:	select_select(data); break;
		default:	break;
		}
		break;
	case OK:
		switch (c) {
		case L'\r':		/* FALLTHROUGH */
		case L'\n':		select_select(data); break;
		case 0x1b: /* ESC */	select_exit(data); break;
		default:		break;
		}
		break;
	default:
		break;
	}
}
