/* See LICENSE file for copyright and license details. */

#include "track.h"

struct track *
track_create(json_t *root)
{
	json_t *last, *skip_allowed, *track;
	json_t *id, *name, *performer, *url;
	struct track *t;
	size_t len;

	if (root == NULL)
		return NULL;

	/* Create new track */
	t = malloc(sizeof(struct track));
	if (t == NULL)
		err(1, NULL);

	/* Parse json */
	last = json_object_get(root, "at_last_track");
	if (last == NULL)
		goto error;
	skip_allowed = json_object_get(root, "skip_allowed");
	if (skip_allowed == NULL)
		goto error;
	track = json_object_get(root, "track");
	if (track == NULL)
		goto error;
	id = json_object_get(track, "id");
	if (id == NULL)
		goto error;
	name = json_object_get(track, "name");
	if (name == NULL)
		goto error;
	performer = json_object_get(track, "performer");
	if (performer == NULL)
		goto error;
	url = json_object_get(track, "track_file_stream_url");
	if (url == NULL)
		goto error;

	/* Set track information */
	t->id = json_integer_value(id);

	len = strlen(json_string_value(name)) + 1;
	t->name = malloc(len * sizeof(char));
	if (t->name == NULL)
		err(1, NULL);
	(void)strlcpy(t->name, json_string_value(name), len);

	len = strlen(json_string_value(performer)) + 1;
	t->performer = malloc(len * sizeof(char));
	if (t->performer == NULL)
		err(1, NULL);
	(void)strlcpy(t->performer, json_string_value(performer), len);

	len = strlen(json_string_value(url)) + 1;
	t->url = malloc(len * sizeof(char));
	if (t->url == NULL)
		err(1, NULL);
	(void)strlcpy(t->url, json_string_value(url), len);

	t->last = json_is_true(last);

	t->reported = FALSE;

	t->skip_allowed = json_is_true(skip_allowed);

	return t;

error:
	free(t);
	return NULL;
}

void
track_free(struct track *t)
{
	free(t->name);
	free(t->performer);
	free(t->url);
}
