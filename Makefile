# Makefile
CC=gcc
CFLAGS=-g -Wall

.PHONY: all

all: bot
bot: bot.c
	$(CC) $(CFLAGS) -o $@ $^
clean:
	rm -f bot
