/* See LICENSE file for copyright and license details. */

#ifndef STRING_H
#define STRING_H

#include <bsd/string.h>
#include <err.h>
#include <stdlib.h>
#include <wchar.h>
#include "defs.h"

size_t	intlen(int);
int	wwrap(wchar_t **, const int);

#endif
