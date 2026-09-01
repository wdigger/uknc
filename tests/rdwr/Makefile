OBJS:=	rdwr.o
SIMH:= pdp11
CC:=pdp11-uknc-rt11-gcc
OPT_FLAG:=-O2
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer $(OPT_FLAG)
LD_FLAGS:=-Wl,--gc-sections -Wl,-Map=rdwr.sav.map

all:	rdwr.dsk

%.o:	%.c
	${CC} ${GCC_FLAGS} -c -o $@ $<

rdwr.sav:	$(OBJS)
	${CC} ${GCC_FLAGS} ${LD_FLAGS} -o $@ $(OBJS)

rdwr.dsk:	rdwr.sav
	rm -f rdwr.dsk
	cp ../../resources/rt11os.dsk rdwr.dsk
	rt11dsk a rdwr.dsk rdwr.sav

clean:
	@-rm *.o *.dsk *.sav.map *.sav
