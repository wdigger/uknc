OBJS:=	hello.o
SIMH:= pdp11
CC:=pdp11-uknc-rt11-gcc
OPT_FLAG:=-O2
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer $(OPT_FLAG)
LD_FLAGS:=-Wl,--gc-sections -Wl,-Map=hello.sav.map

all:	hello.dsk

%.o:	%.c
	${CC} ${GCC_FLAGS} -c -o $@ $<

hello.sav:	$(OBJS)
	${CC} ${GCC_FLAGS} ${LD_FLAGS} -o $@ $(OBJS)

hello.dsk:	hello.sav
	rm -f hello.dsk
	cp ../../resources/rt11os.dsk hello.dsk
	rt11dsk a hello.dsk hello.sav

clean:
	@-rm *.o *.dsk *.sav.map *.sav
