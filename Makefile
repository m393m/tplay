CC ?= gcc
CFLAGS ?= -O3 -fPIC -pthread
PREFIX ?= /usr
LIBDIR ?= $(PREFIX)/lib64
BINDIR ?= $(PREFIX)/bin
FONTDIR ?= $(PREFIX)/share/fonts/tplay

all: libtplay_turbo.so

libtplay_turbo.so: libtplay_turbo.c
	$(CC) $(CFLAGS) -shared libtplay_turbo.c -o libtplay_turbo.so

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(FONTDIR)
	install -m 0755 tplay-turbo $(DESTDIR)$(BINDIR)/tplay-turbo
	install -m 0755 libtplay_turbo.so $(DESTDIR)$(LIBDIR)/libtplay_turbo.so
	if [ -f TPlayPixel.ttf ]; then install -m 0644 TPlayPixel.ttf $(DESTDIR)$(FONTDIR)/TPlayPixel.ttf; fi

clean:
	rm -f libtplay_turbo.so
