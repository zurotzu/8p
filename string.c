/* See LICENSE file for copyright and license details. */

#include "string.h"

size_t
intlen(int i)
{
	size_t len;

	if (i < 0)
		i *= -1;
	len = 1; /* Smallest possible size */
	while (i > 9) {
		len++;
		i /= 10;
	}

	return len;
}


int
wwrap(wchar_t **s, const int maxwidth)
{
	int i, j;
	int lw; /* Last wrap location */
	int found;
	int width;
	size_t len;
	wchar_t *tmp;

	if (*s == NULL)
		return ERROR;
	if (maxwidth < 3) /* Not worth it */
		return ERROR;

	/* Follow the string until we reach further than the maxwidth,
	 * than track back to the last space and insert a newline.
	 * If no space is found insert a newline at maxwidth.
	 */
	for (i = 0, width = 0, lw = 0; (*s)[i] != L'\0'; i++) {
		if ((*s)[i] == L'\t') /* Replace tabs with spaces */
			(*s)[i] = ' ';
		if ((*s)[i] == L'\n') {
			lw = i;
			width = 0;
		} else
			width+= wcwidth((*s)[i]);
		if (width >= maxwidth) {
			/* Replace last space with newline */
			for (j = i, found = FALSE; j > lw && found == FALSE;
			    j--) {
				if ((*s)[j] == L' ') {
					found = TRUE;
					(*s)[j] = L'\n';
					lw = j;
					i = j;
				}
			}
			/* If no space was found for a clean newline, insert
			 * a newline at position maxwidth - 1.
			 */
			if (found == FALSE) {
				tmp = *s;
				len = wcslen(tmp) + 1 + 1;
				*s = malloc(len * sizeof(wchar_t));
				if (*s == NULL)
					err(1, NULL);
				for (j = 0; j < i; j++)
					(*s)[j] = tmp[j];
				(*s)[i] = L'\n';
				for (j = i+1; j < (int)len; j++)
					(*s)[j] = tmp[j-1];
				free(tmp);
			}
			width = 0;
		}
	}

	return SUCCESS;
}
