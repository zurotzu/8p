/* See LICENSE file for copyright and license details. */

#ifndef PLAY_H
#define PLAY_H

#include <stdio.h>
#include <vlc/vlc.h>
#include <wchar.h>
#include "defs.h"
#include "mix.h"
#include "search.h"

int	play_init(struct info *);
void	play_exit(struct info *);
int	play_ended(struct info *);
int	play_isoverthirtymark(struct info *);
void	play_togglepause(struct info *);
void	play_next(struct info *);
void	play_nextmix(struct info *);
void	play_skip(struct info *);


#endif
