/* See LICENSE file for copyright and license details. */

#ifndef MIX_H
#define MIX_H

#include <bsd/string.h>
#include <err.h>
#include <jansson.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "fetch.h"
#include "string.h"
#include "track.h"
#include "util.h"

struct mix	*mix_create(json_t *);
void		 mix_free(struct mix *);
struct track	*mix_nexttrack(struct info *);

#endif
