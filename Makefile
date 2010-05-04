# Makefile
CC=gcc
CFLAGS += -g -Wall
LIBS += $(shell pkg-config --cflags --libs glib-2.0)

.PHONY: all

all: main 
main: irc_multiplexer.c irc_multiplexer.h
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)
clean:
	rm -f main 

