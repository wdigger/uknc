OBJS:=	spigot.o
SIMH:= pdp11
CC:=pdp11-uknc-rt11-gcc
OPT_FLAG:=-O2
# -ffunction-sections/-fdata-sections put every function and every
# variable in a section of its own, so that -Wl,--gc-sections below
# can drop the ones nothing reaches.  Only the ELF toolchain can do
# this; the older a.out one warns that it cannot and carries on.
SECTION_FLAGS:=-ffunction-sections -fdata-sections
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer $(OPT_FLAG) $(SECTION_FLAGS)
LD_FLAGS:=-Wl,--gc-sections -Wl,-Map=spigot.sav.map

all:	spigot.dsk

%.o:	%.c
	${CC} ${GCC_FLAGS} -c -o $@ $<

spigot.sav:	$(OBJS)
	${CC} ${GCC_FLAGS} ${LD_FLAGS} -o $@ $(OBJS)

spigot.dsk:	spigot.sav
	rm -f spigot.dsk
	cp ../../resources/rt11os.dsk spigot.dsk
	rt11dsk a spigot.dsk spigot.sav

clean:
	@-rm *.o *.dsk *.sav.map *.sav
