CC	= gcc
INSTALL = install
CFLAGS  ?= -march=native -O2 -Wall -pipe -ansi -pedantic

PREFIX = /usr/local

LUA_DIR = $(PREFIX)
LUA_LIBDIR=$(LUA_DIR)/lib/lua/5.1
LUA_SHAREDIR=$(LUA_DIR)/share/lua/5.1

programs = voorhees.so

.PHONY: all test strip indent install uninstall clean

all: $(programs)

voorhees.so: CFLAGS+=-fpic -nostartfiles
voorhees.so: LDFLAGS+=-shared
voorhees.so: voorhees.c
	$(CC) $(CFLAGS) $^ -llua $(LDFLAGS) $(LIBS) -o $@

test:
	lua test.lua

strip: $(programs)
	@for i in $(programs); do echo strip $$i; strip "$$i"; done

indent:
	indent -kr -i8 *.c *.h

install: strip
	$(INSTALL) -m755 -D voorhees.so $(DESTDIR)$(LUA_LIBDIR)/voorhees.so

uninstall:
	rm -f $(DESTDIR)$(LUA_LIBDIR)/voorhees.so

clean:
	rm -f $(programs) *.o *.c~ *.h~
