/* See LICENSE file for copyright and license details. */

#ifndef DEFS_H
#define DEFS_H

#include <vlc/vlc.h>

#define APIKEY	"e233c13d38d96e3a3a0474723f6b3fcd21904979"

#define ERROR	0
#define SUCCESS	1
#define FALSE	0
#define	TRUE	1

#define HALFDELAY	50
#define DELAYESC	10

struct info {
	/*
	 * Main section
	 */
	int	 quit;
	int	 state;
	int	 pstate; /* Previous state */

	/*
	 * Play section
	 */
	char			*playtoken;
	libvlc_instance_t	*vlc_inst;
	libvlc_media_player_t	*vlc_mp;
	struct	 		mix *m;
	
	/*
	 * Select section
	 */
	struct	 mix **mlist;
	size_t	 mlist_size;
	int	 select_pos;

	/*
	 * Search section
	 */
	int	 cursor_pos;
	char	*search_str;
	struct	 search_node *slist_head;

	/* 
	 * Drawing section 
	 */
	int	 scroll;
};
struct mix {
	int	 id;
	char	*name;
	int	 user_id;
	char	*description;
	int	 likes_count;
	int	 plays_count;
	char	*tags;
	int	 liked;
	int	 finished;
	int	 track_count;
	struct	 track **track;
};
struct track {
	int	 id;
	char	*name;
	char	*performer;
	char	*url;
	int	 last;
	int	 reported;
	int	 skip_allowed;
};
struct search_node {
	struct search_node	*next;
	wchar_t			 c;
};

enum states {START, SEARCH, SEARCHING, SELECT, PLAY};

#endif
