/* See LICENSE file for copyright and license details. */

#ifndef FETCH_H
#define FETCH_H

#include <curl/curl.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"

struct buffer {
	char	*data;
	size_t	 pos;
};

int	fetch(char **, const char *);

#endif
