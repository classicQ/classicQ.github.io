CC=gcc
STRIP=strip

CFLAGS=-O2 -g -Wall
STRIPFLAGS=--strip-unneeded

all: genrgbmap pak

genrgbmap: genrgbmap.o
	$(CC) $(CFLAGS) -o $@.db $< -lm
	$(STRIP) $(STRIPFLAGS) -o $@ $@.db

pak: pak.o
	$(CC) $(CFLAGS) -o $@.db $<
	$(STRIP) $(STRIPFLAGS) -o $@ $@.db

clean:
	rm -f genrgbmap genrgbmap.db genrgbmap.o pak pak.db pak.o

.PHONY: all clean

