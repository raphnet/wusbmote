CC=gcc
LD=$(CC)

CFLAGS=-Wall -g `pkg-config hidapi-hidraw --cflags`
LDFLAGS=`pkg-config hidapi-hidraw --libs`

PREFIX=/usr/local

PROG=wusbmote_ctl

OBJS=$(patsubst %.c,%.o,$(wildcard *.c))

.PHONY : clean install

all: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(PROG)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(PROG)

install:
	@echo "Install not done yet. Sorry"

