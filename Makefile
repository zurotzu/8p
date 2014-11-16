VERSION=	1.2

PREFIX?=	/usr/local

CFLAGS+=	-std=c99 -O2 -pedantic -Wall -Werror -Wextra \
		-D_XOPEN_SOURCE_EXTENDED=1 -D_XOPEN_SOURCE=700
LIBS=		-lcurl -ljansson -lncursesw -lvlc -lbsd
LDFLAGS+=	-s ${LIBS}

SRCS=	draw.c fetch.c key.c main.c mix.c play.c report.c search.c \
	select.c string.c track.c util.c
OBJS=	${SRCS:.c=.o}

all: 8p

8p: ${OBJS}
	${CC} ${CFLAGS} -o $@ $^ ${LDFLAGS}

%.o: %.c
	${CC} ${CFLAGS} -c -o $@ $<

clean:
	rm -f 8p ${OBJS}

dist:
	@echo creating tarball
	mkdir -p 8p-${VERSION}
	cp *.c *.h Makefile README.md TODO.md LICENSE Screenshot.png \
		8p-${VERSION}
	tar -cf 8p-${VERSION}.tar 8p-${VERSION}
	gzip 8p-${VERSION}.tar
	rm -rf 8p-${VERSION}

install: all
	@echo installing executables to ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f 8p ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/8p

uninstall:
	@echo removing executables from ${DESTDIR}${PREFIX}/bin
	rm -f ${DESTDIR}${PREFIX}/bin/8p

.PHONY: all clean dist install uninstall
