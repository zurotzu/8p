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


`sudo make clean debian install`

It will in draw.h, key.h, search.h, and select.h, change `#include <ncurses.h>` into `#include <ncursesw/curses.h>`.

This change can be reverted with `make arch`.

For convenience, here is a one-liner with the debian-dependencies:

`sudo apt-get install libbsd-dev libncursesw5-dev libvlc-dev libjansson-dev libcurl4-openssl-dev`
