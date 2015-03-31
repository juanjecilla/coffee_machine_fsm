CC=gcc
CFLAGS=-g -Wall -O -I.
LDFLAGS= -L.
LDLIBS=-lWiringPi -lrt
all: main
main: main.o fsm.o
clean:
	$(RM) *.o *~ main

libWiringPi.a: wiringPi.o
	ar rcs $@ $^