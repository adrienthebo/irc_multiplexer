# Makefile
CC=gcc
CFLAGS += -g -Wall -std=gnu99

.PHONY: all

all: bot client
debug: 
	CFLAGS="-DDEBUG" $(MAKE)
bot: server.o irc_multiplexer.o irc_message.o buffered_socket.o utilities.o
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^ 
client: client.o
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^ 

server.o: server.c irc_multiplexer.h
	$(CC) $(CFLAGS) $(LIBS) -c $<
client.o: client.c
	$(CC) $(CFLAGS) $(LIBS) -c $<
irc_multiplexer.o: irc_multiplexer.c irc_multiplexer.h irc_message.h utilities.h
	$(CC) $(CFLAGS) $(LIBS) -c $<
irc_message.o: irc_message.c irc_message.h
	$(CC) $(CFLAGS) $(LIBS) -c $<
buffered_socket.o: buffered_socket.c buffered_socket.h utilities.h
	$(CC) $(CFLAGS) $(LIBS) -c $<
utilities.o: utilities.c utilities.h
	$(CC) $(CFLAGS) $(LIBS) -c $<
clean:
	rm -f bot client client.o buffered_socket.o \
	    server.o irc_multiplexer.o irc_message.o utilities.o

