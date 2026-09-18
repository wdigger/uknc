OBJS:=	ansi.o
SIMH:= pdp11
CC:=../../gcc/bin/pdp11-uknc-rt11-gcc
OPT_FLAG:=-O2
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer $(OPT_FLAG)
LD_FLAGS:=-Wl,--gc-sections -Wl,-Map=ansi.sav.map

all:	ansi.dsk

%.o:	%.c
	${CC} ${GCC_FLAGS} -c -o $@ $<

ansi.sav:	$(OBJS)
	${CC} ${GCC_FLAGS} ${LD_FLAGS} -o $@ $(OBJS)

ansi.dsk:	ansi.sav
	rm -f ansi.dsk
	cp ../../resources/rt11os.dsk ansi.dsk
	rt11dsk a ansi.dsk ansi.sav

clean:
	@-rm *.o *.dsk *.sav.map *.sav
