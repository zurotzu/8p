/* See LICENSE file for copyright and license details. */

#include "select.h"

void
select_init(struct info *data)
{
	data->state = SELECT;
	data->select_pos = 0;
	data->scroll = 0;
}

void
select_exit(struct info *data)
{
	int i;

	data->state = data->pstate;
	data->scroll = 0;

	for (i = 0; i < (int)data->mlist_size;  i++)
		mix_free(data->mlist[i]);
	free(data->mlist);
	data->mlist = NULL;
}

void
select_changepos(struct info *data, wint_t c)
{
	if (c == KEY_UP) {
		data->select_pos = mod(data->select_pos-1, data->mlist_size);
		data->scroll = 0;
	} else if (c == KEY_DOWN) {
		data->select_pos = mod(data->select_pos+1, data->mlist_size);
		data->scroll = 0;
	}
}

void
select_select(struct info *data)
{
	int i;

	if (data->m)
		mix_free(data->m);
	data->m = data->mlist[data->select_pos];
	for (i = 0; i < (int)data->mlist_size; i++) {
		if (i != data->select_pos)
			mix_free(data->mlist[i]);
	}
	free(data->mlist);
	data->mlist = NULL;

	data->state = PLAY;
	data->scroll = 0;
	draw_redraw(data);
	play_next(data);
}
