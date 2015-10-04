CC=gcc
STRIP=strip

CFLAGS=-g -Wall -O3 -fno-strict-aliasing -I$(VPATH)../common
STRIPFLAGS=--strip-unneeded --remove-section=.comment

OBJS = \
	flow.o \
	soundpvs.o \
	vis.o

all:
	mkdir -p objects
	(cd objects && $(MAKE) -f ../Makefile vis VPATH=../)

vis: $(OBJS)
	$(CC) $(CFLAGS) -L$(VPATH)../common/objects $(OBJS) -o $@.db -lcommon -lm
	$(STRIP) $(STRIPFLAGS) $@.db -o $@

clean:
	rm -rf objects

