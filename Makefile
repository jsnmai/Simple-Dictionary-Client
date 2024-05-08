CC=gcc
CFLAGS=-g -O2 -Wall -Wno-unused-variable
LDLIBS=-lpthread

all: dictclient

dictclient: dictclient.c

clean:
	rm -f *~ dictclient

