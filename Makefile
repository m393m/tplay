CC = gcc
CFLAGS = -O3 -march=native -fPIC -shared -pthread -Wall -Wextra
PREFIX = /usr
LIBDIR = $(PREFIX)/lib64
ifeq ($(wildcard $(LIBDIR)),)
	LIBDIR = $(PREFIX)/lib
endif

all: libtplay_turbo.so

libtplay_turbo.so: libtplay_turbo.c
	$(CC) $(CFLAGS) -o $@ $<

install: all
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(LIBDIR) $(DESTDIR)$(PREFIX)/share/fonts/tplay
	install -m 0755 tplay-turbo $(DESTDIR)$(PREFIX)/bin/tplay-turbo
	sed -i "s|/usr/lib64/libtplay_turbo.so|$(LIBDIR)/libtplay_turbo.so|g" $(DESTDIR)$(PREFIX)/bin/tplay-turbo
	install -m 0755 libtplay_turbo.so $(DESTDIR)$(LIBDIR)/libtplay_turbo.so
	install -m 0644 TPlayPixel.ttf $(DESTDIR)$(PREFIX)/share/fonts/tplay/TPlayPixel.ttf

clean:
	rm -f libtplay_turbo.so

.PHONY: all install clean
