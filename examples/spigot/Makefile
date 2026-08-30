OBJS:=	spigot.o stdio.o
SIMH:= pdp11
CC:=pdp11-uknc-rt11-gcc
OPT_FLAG:=-O2
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer -msoft-float $(OPT_FLAG)
LD_FLAGS:=-Wl,--gc-sections -Wl,-Map=spigot.sav.map

all:	spigot.dsk

%.o:	%.c
	${CC} ${GCC_FLAGS} -c -o $@ $<

spigot.sav:	$(OBJS)
	${CC} ${GCC_FLAGS} -nodefaultlibs ${LD_FLAGS} -o $@ $(OBJS) -lgcc

spigot.dsk:	spigot.sav
	rm -f spigot.dsk
	cp ../../resources/rt11os.dsk spigot.dsk
	rt11dsk a spigot.dsk spigot.sav

clean:
	@-rm *.o *.dsk *.sav.map *.sav
