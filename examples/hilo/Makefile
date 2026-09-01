OBJS:=	hilo.o
SIMH:= pdp11
CC:=pdp11-uknc-rt11-gcc
OPT_FLAG:=-O2
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer $(OPT_FLAG)
LD_FLAGS:=-Wl,--gc-sections -Wl,-Map=hilo.sav.map

all:	hilo.dsk

%.o:	%.c
	${CC} ${GCC_FLAGS} -c -o $@ $<

hilo.sav:	$(OBJS)
	${CC} ${GCC_FLAGS} ${LD_FLAGS} -o $@ $(OBJS)

hilo.dsk:	hilo.sav
	rm -f hilo.dsk
	cp ../../resources/rt11os.dsk hilo.dsk
	rt11dsk a hilo.dsk hilo.sav

clean:
	@-rm *.o *.dsk *.sav.map *.sav
