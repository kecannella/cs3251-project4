#################################################################
##
## FILE:	Makefile
## PROJECT:	CS 3251 Project 1 - Professor Traynor
## DESCRIPTION: Compile Project 1
##
#################################################################

CC=gcc -g

OS := $(shell uname -s)

# Extra LDFLAGS if Solaris
ifeq ($(OS), SunOS)
	LDFLAGS=-lsocket -lnsl
    endif

all: server 

server: server.c util.c util.h
	$(CC) server.c util.c -o server -lcrypto -pthread
	
clean:
	    rm -f server *.o

