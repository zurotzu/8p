/* See LICENSE file for copyright and license details. */

#include "main.h"

static void		 checklocale(void);
static void		 dochecks(struct info *);
static struct info	*info_create(void);
static void		 info_free(struct info *);

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
dochecks(struct info *data)
{
	if (data->state == PLAY) {
		/* Tracks need to be reported if they are playing
		 * for more than 30 seconds.
		 */
		if (play_isoverthirtymark(data) == TRUE)
			report(data);
		if (play_ended(data) == TRUE)
			play_next(data);
	}
}

static struct info *
info_create(void)
{
	struct info *data;

	data = malloc(sizeof(struct info));
	if (data == NULL)
		err(1, NULL);

	/* Initialize data */
	data->quit = FALSE;
	data->state = START;

	data->playtoken = NULL;
	data->m = NULL;
	data->mlist = NULL;

	data->search_str = NULL;
	data->slist_head = NULL;

	data->scroll = 0;

	return data;
}

static void
info_free(struct info *data)
{
	free(data->playtoken);
	mix_free(data->m);
	free(data->search_str);
	searchstr_clear(data);
	free(data);
}

int
main(void)
{
	struct info *data;

	/* Initialize */
	checklocale();
	data = info_create();
	draw_init();
	play_init(data);

	while (data->quit != TRUE) {
		dochecks(data);
		draw_redraw(data);
		key_handle(data);
	}

	play_exit(data);
	info_free(data);
	draw_exit();

	return 0;
}
