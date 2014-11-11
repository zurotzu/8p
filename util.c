/* See LICENSE file for copyright and license details. */

#include "util.h"

int
setplaytoken(struct info *data)
{
	char *js;
	char url[] = "http://8tracks.com/sets/new";
	int errn;
	size_t len;
	json_t *root, *playtoken, *status;

	if (data->playtoken)
		return SUCCESS;

	js = NULL;
	root = NULL;

	/* Fetch url */
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
	playtoken = json_object_get(root, "play_token");
	if (playtoken == NULL)
		goto error;

	/* Set playtoken */
	len = strlen(json_string_value(playtoken)) + 1;
	data->playtoken = malloc(len * sizeof(char));
	if (data->playtoken == NULL)
		err(1, NULL);
	(void)strlcpy(data->playtoken, json_string_value(playtoken), len);

	/* Cleanup */
	free(js);
	json_decref(root);

	return SUCCESS;

error:
	if (js)
		free(js);
	if (root)
		json_decref(root);

	return ERROR;
}

int
mod(int a, int b)
{
	int ret;

	ret = a % b;
	if (ret < 0)
		ret += b;

	return ret;
}
