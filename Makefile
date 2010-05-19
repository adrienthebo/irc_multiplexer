# Makefile
CC=gcc
CFLAGS += -g -Wall -std=gnu99
#LIBS += $(shell pkg-config --cflags --libs glib-2.0)

.PHONY: all

all: bot
debug: 
	CFLAGS="-DDEBUG" $(MAKE)
bot: main.o irc_multiplexer.o irc_message.o utilities.o
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^ 
main.o: main.c irc_multiplexer.h
	$(CC) $(CFLAGS) $(LIBS) -c $<
irc_multiplexer.o: irc_multiplexer.c irc_multiplexer.h irc_message.h utilities.h
	$(CC) $(CFLAGS) $(LIBS) -c $<
irc_message.o: irc_message.c irc_message.h
	$(CC) $(CFLAGS) $(LIBS) -c $<
utilities.o: utilities.c utilities.h
	$(CC) $(CFLAGS) $(LIBS) -c $<
clean:
	rm -f bot main.o irc_multiplexer.o irc_message.o utilities.o

