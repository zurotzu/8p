/* See LICENSE file for copyright and license details. */

#include "fetch.h"

static size_t	write(void *, size_t, size_t, void *);

int
fetch(char **js, const char *url)
{
	CURL *curl;
	CURLcode curl_err;
	struct buffer buf;
	struct curl_slist *headers;
	
	if (url == NULL || *js == NULL)
		return ERROR;

	/* Initialize */
	headers = NULL;
	curl = NULL;

	/* Set up curl */
	curl_err = curl_global_init(CURL_GLOBAL_ALL);
	if (curl_err != 0)
		return ERROR;
	curl = curl_easy_init();
	if (curl == NULL)
		goto error;

	/* Set up the buffer */
	buf.data = *js;
	buf.pos = 0;

	/* Set up the curl headers */
	headers = curl_slist_append(NULL, "X-Api-Key: " APIKEY);
	headers = curl_slist_append(headers, "X-Api-Version: 3");
	headers = curl_slist_append(headers, "User-Agent: 8p");
	headers = curl_slist_append(headers, "Accept: application/json");
	if (headers == NULL)
		goto error;

	/* Set up curl request */
	curl_err = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	if (curl_err != 0)
		goto error;
	curl_err = curl_easy_setopt(curl, CURLOPT_URL, url);
	if (curl_err != 0)
		goto error;
	curl_err = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write);
	if (curl_err != 0)
		goto error;
	curl_err = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&buf);
	if (curl_err != 0)
		goto error;

	/* Perform request */
	curl_err = curl_easy_perform(curl);
	if (curl_err != 0)
		goto error;

	/* If the buffer ran out of memory, a realloc in the write
	 * function could have changed the pointer.  This makes sure
	 * that js still points to the right memory address.
	 */
	*js = buf.data;

	/* Cleanup */
	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);
	curl_global_cleanup();

	return SUCCESS;

error:
	if (curl)
		curl_easy_cleanup(curl);
	if (headers)
		curl_slist_free_all(headers);
	curl_global_cleanup();
	
	return ERROR;
}

static size_t
write(void *contents, size_t size, size_t nmemb, void *stream)
{
	struct buffer *buf;

	buf = (struct buffer *)stream;
	buf->data = realloc(buf->data, buf->pos + size * nmemb + 1);
	if (buf->data == NULL)
		err(1, NULL);
	memcpy(&(buf->data[buf->pos]), contents, size * nmemb);
	buf->pos += size * nmemb;
	buf->data[buf->pos] = '\0';
	
	return size * nmemb;
}
