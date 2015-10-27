/* See LICENSE file for copyright and license details. */

#include "report.h"

void
report(struct info *data)
{
	struct track *t;
	int index;
	char *js, *url;
	size_t len;

	index = data->m->track_count - 1;
	t = data->m->track[index];

	/* Report only once */
	if (t->reported == TRUE)
		return;

	/* Set up url */
	len = strlen("http://8tracks.com/sets/") + strlen(data->playtoken) +
	    strlen("/report?track_id=") + intlen(t->id) +
	    strlen("&mix_id=") + intlen(data->m->id) + 1;
	url = malloc(len * sizeof(char));
	if (url == NULL)
		err(1, NULL);
	(void)snprintf(url, len,
	    "http://8tracks.com/sets/%s/report?track_id=%d&mix_id=%d",
	    data->playtoken, t->id, data->m->id);

	/* Report track */
	js = malloc(1);
	if (js == NULL)
		err(1, NULL);
	(void)fetch(&js, url); /* Just reporting, result doesn't matter. */
	t->reported = TRUE;

	/* Cleanup */
	free(js);
	free(url);
}
