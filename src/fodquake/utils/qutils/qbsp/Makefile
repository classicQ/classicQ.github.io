CC=gcc
STRIP=strip

CFLAGS=-g -Wall -O3 -fno-strict-aliasing -I$(VPATH)../common
STRIPFLAGS=--strip-unneeded --remove-section=.comment

OBJS = \
	brush.o \
	csg4.o \
	map.o \
	merge.o \
	nodraw.o \
	outside.o \
	portals.o \
	qbsp.o \
	region.o \
	solidbsp.o \
	surfaces.o \
	tjunc.o \
	writebsp.o

all:
	mkdir -p objects
	(cd objects && $(MAKE) -f ../Makefile qbsp VPATH=../)

qbsp: $(OBJS)
	$(CC) $(CFLAGS) -L$(VPATH)../common/objects $(OBJS) -o $@.db -lcommon -lm
	$(STRIP) $(STRIPFLAGS) $@.db -o $@

clean:
	rm -rf objects

