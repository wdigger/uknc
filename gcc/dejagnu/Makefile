# The DejaGnu board links uknc_exit.o into every test it runs; see
# uknc_exit.c for what it is for.  Built here rather than committed.
CC:=pdp11-uknc-rt11-gcc
CFLAGS:=-std=gnu23 -Os -fomit-frame-pointer

all:	uknc_exit.o

uknc_exit.o:	uknc_exit.c
	${CC} ${CFLAGS} -c -o $@ $<

clean:
	@-rm -f uknc_exit.o
