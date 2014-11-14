/* See LICENSE file for copyright and license details. */

#include "mix.h"

struct mix *
mix_create(json_t *root)
{
	struct mix *m;
	json_t *id, *name, *user_id, *description, *likes_count, *plays_count,
	     *tags, *liked;
	size_t len;

	if (root == NULL)
		return NULL;

	m = malloc(sizeof(struct mix));
	if (m == NULL)
		err(1, NULL);

	/* Initialize */
	m->name = NULL;
	m->description = NULL;
	m->tags = NULL;
	m->track = NULL;

	/* Set information from json object */
	id = json_object_get(root, "id");
	if (id != NULL)
		m->id = json_integer_value(id);

	name = json_object_get(root, "name");
	if (name != NULL) {
		len = strlen(json_string_value(name)) + 1;
		m->name = malloc(len * sizeof(char));
		if (m->name == NULL)
			err(1, NULL);
		(void)strlcpy(m->name, json_string_value(name), len);
	}

	user_id = json_object_get(root, "user_id");
	if (user_id != NULL)
		m->user_id = json_integer_value(user_id);

	description = json_object_get(root, "description");
	if (description != NULL) {
		len = strlen(json_string_value(description)) + 1;
		m->description = malloc(len * sizeof(char));
		if (m->description == NULL)
			err(1, NULL);
		(void)strlcpy(m->description, json_string_value(description),
		    len);
	}

	likes_count = json_object_get(root, "likes_count");
	if (likes_count != NULL)
		m->likes_count = json_integer_value(likes_count);

	plays_count = json_object_get(root, "plays_count");
	if (plays_count != NULL)
		m->plays_count = json_integer_value(plays_count);

	tags = json_object_get(root, "tag_list_cache");
	if (tags != NULL) {
		len = strlen(json_string_value(tags)) + 1;
		m->tags = malloc(len * sizeof(char));
		if (m->tags == NULL)
			err(1, NULL);
		(void)strlcpy(m->tags, json_string_value(tags), len);
	}

	liked = json_object_get(root, "liked_by_current_user");
	if (liked != NULL)
		m->liked = json_is_true(liked);

	/* Set default values */
	m->finished = FALSE;
	m->track_count = 0;
	m->track = NULL;

	return m;
}

void
mix_free(struct mix *m)
{
	int i;

	if (m == NULL)
		return;

	free(m->name);
	free(m->description);
	free(m->tags);
	for (i = 0; i < m->track_count; i++)
		track_free(m->track[i]);
	free(m->track);
	free(m);
}

struct track *
mix_nexttrack(struct info *data)
{
	char *js, *url;
	size_t len;
	int errn;
	json_t *root, *set, *status;
	struct track *t;

	if (data == NULL)
		return NULL;
	if (data->m == NULL)
		return NULL;

	/* Initialize variables */
	js = NULL;
	url = NULL;
	root = NULL;
	set = NULL;
	status = NULL;
	errn = setplaytoken(data);
	if (errn == ERROR)
		goto error;

	/* Set up url
	 * A different url is needed for the first track to get
	 * the mix started.
	 */
	if (data->m->track_count == 0) {
		len = strlen("http://8tracks.com/sets/") +
		    strlen(data->playtoken) + strlen("/play?mix_id=") +
		    intlen(data->m->id) + 1;
		url = malloc(len * sizeof(char));
		if (url == NULL)
			err(1, NULL);
		(void)snprintf(url, len,
		    "http://8tracks.com/sets/%s/play?mix_id=%d",
		    data->playtoken, data->m->id);
	} else {
		len = strlen("http://8tracks.com/sets/") +
		    strlen(data->playtoken) + strlen("/next?mix_id=") +
		    intlen(data->m->id) + 1;
		url = malloc(len * sizeof(char));
		if (url == NULL)
			err(1, NULL);
		(void)snprintf(url, len,
		    "http://8tracks.com/sets/%s/next?mix_id=%d",
		    data->playtoken, data->m->id);
	}

	/* Retrieve json string */
	js = malloc(1);
	if (js == NULL)
		err(1, NULL);
	errn = fetch(&js, url);
	if (errn == ERROR)
		goto error;

	/* Parse json string */
	root = json_loads(js, 0, NULL);
	if (root == NULL)
		goto error;
	status = json_object_get(root, "status");
	if (strcmp(json_string_value(status), "200 OK") != 0)
		goto error;
	set = json_object_get(root, "set");
	if (set == NULL)
		goto error;

	/* Set up track */
	t = track_create(set);
	if (t == NULL)
		goto error;
	data->m->track_count++;
	data->m->track = realloc(data->m->track,
	    data->m->track_count * sizeof(struct track *));
	if (data->m->track == NULL)
		goto error;
	data->m->track[data->m->track_count-1] = t;

	/* Cleanup */
	free(url);
	free(js);
	json_decref(root);

	return t;

error:
	if (js)
		free(js);
	if (url)
		free(url);
	if (root)
		json_decref(root);

	return NULL;
}
