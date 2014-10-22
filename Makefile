VERSION = 1.1
PREFIX = /usr/local
CFLAGS+=-pedantic -Wall -Werror
LIBS=-lcurl -ljansson -lncursesw -lvlc -lbsd
LDFLAGS+=-s ${LIBS}

all: 8p

%.o: %.c 8p.c
	@echo CC -c -o $@ $< ${CFLAGS}
	@${CC} -c -o $@ $< ${CFLAGS}

8p: 8p.o
	@echo CC -o $@ $^ ${LDFLAGS}
	@${CC} -o $@ $^ ${LDFLAGS}

clean:
	@echo cleaning
	@echo rm -f 8p 8p.o
	@rm -f 8p 8p.o

dist:
	@echo creating tarball
	@mkdir -p 8p-${VERSION}
	@cp 8p.c Makefile README.md 8p-${VERSION}
	@tar -cf 8p-${VERSION}.tar 8p-${VERSION}
	@gzip 8p-${VERSION}.tar
	@rm -rf 8p-${VERSION}

install: all
	@echo installing executables to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f 8p ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/8p

uninstall:
	@echo removing executables from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/8p

.PHONY: all clean dist install uninstall
