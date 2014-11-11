/* See LICENSE file for copyright and license details. */

#ifndef UTIL_H
#define UTIL_H

#include <bsd/string.h>
#include <err.h>
#include <jansson.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "fetch.h"

int	setplaytoken(struct info *);
int	mod(int, int);

#endif
