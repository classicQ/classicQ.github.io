AR=ar
CC=gcc
RANLIB=ranlib

ARFLAGS=rc
CFLAGS=-g -Wall -O3 -fno-strict-aliasing

OBJS = \
	bspfile.o \
	cmdlib.o \
	lbmlib.o \
	mathlib.o \
	polylib.o \
	scriplib.o \
	threads.o \
	trilib.o \
	wadlib.o

all:
	mkdir -p objects
	(cd objects && $(MAKE) -f ../Makefile libcommon.a VPATH=../)

libcommon.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	$(RANLIB) $@

clean:
	rm -rf objects

