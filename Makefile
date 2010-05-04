# Makefile
CC=gcc
CFLAGS += -g -Wall
LIBS += $(shell pkg-config --cflags --libs glib-2.0)

.PHONY: all

all: bot
bot: main.o irc_multiplexer.o
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^ 
main.o: main.c irc_multiplexer.h
	$(CC) $(CFLAGS) $(LIBS) -c $@ $< $(LIBS)
irc_multiplexer.o: irc_multiplexer.c irc_multiplexer.h
	$(CC) $(CFLAGS) $(LIBS) -c $@ $< $(LIBS)
clean:
	rm -f bot main.o irc_multiplexer.o

