# 8p

An ncurses-based command-line player for 8tracks.com.

## Screenshot
![Screenshot](Screenshot.png?raw=true)

## Dependencies
curl, jansson, libbsd, ncurses(w), vlc, utf8 locale

## Installation

To install run (as root)  

`make clean install`

Note: 8p is installed by default into the /usr/local namespace.  
You can change this by specifying `PREFIX`, e.g.  
`make PREFIX=/usr install`  
will install the 8p binary into /usr/bin/8p.

### Arch Linux

[AUR Package](https://aur.archlinux.org/packages/8p)

### Debian/Ubuntu

In draw.h, key.h, search.h, and select.h, change `#include <ncurses.h>` into `#include <ncursesw/curses.h>`.

