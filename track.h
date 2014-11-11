/* See LICENSE file for copyright and license details. */

#ifndef TRACK_H
#define TRACK_H

#include <bsd/string.h>
#include <err.h>
#include <jansson.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"

struct track	*track_create(json_t *);
void		 track_free(struct track *);

#endif
