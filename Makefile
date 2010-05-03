# Makefile
CC=gcc
CFLAGS += -g -Wall
LIBS += $(shell pkg-config --cflags --libs glib-2.0)

.PHONY: all

all: main 
main: main.c 
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
clean:
	rm -f main 

