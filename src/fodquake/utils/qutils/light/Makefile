CC=gcc
STRIP=strip

CFLAGS=-g -Wall -O3 -fno-strict-aliasing -I$(VPATH)../common
STRIPFLAGS=--strip-unneeded --remove-section=.comment

OBJS = \
	entities.o \
	light.o \
	ltface.o \
	threads.o \
	trace.o

all:
	mkdir -p objects
	(cd objects && $(MAKE) -f ../Makefile light VPATH=../)

light: $(OBJS)
	$(CC) $(CFLAGS) -L$(VPATH)../common/objects $(OBJS) -o $@.db -lcommon -lm
	$(STRIP) $(STRIPFLAGS) $@.db -o $@

clean:
	rm -rf objects

