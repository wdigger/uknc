OBJS:=	hilo.o
SIMH:= pdp11
CC:=pdp11-uknc-rt11-gcc
OPT_FLAG:=-O2
# -ffunction-sections/-fdata-sections put every function and every
# variable in a section of its own, so that -Wl,--gc-sections below
# can drop the ones nothing reaches.  Only the ELF toolchain can do
# this; the older a.out one warns that it cannot and carries on.
SECTION_FLAGS:=-ffunction-sections -fdata-sections
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer $(OPT_FLAG) $(SECTION_FLAGS) -g
LD_FLAGS:=-Wl,--gc-sections -Wl,-Map=hilo.sav.map

all:	hilo.dsk

%.o:	%.c
	${CC} ${GCC_FLAGS} -c -o $@ $<

hilo.sav:	$(OBJS)
	${CC} ${GCC_FLAGS} ${LD_FLAGS} -o $@ $(OBJS)

# The same program linked as ELF instead of a flat image: same
# objects, same addresses, but with the symbols and DWARF that a
# .sav has nowhere to keep.  This is what gdb reads.
hilo.elf:	$(OBJS)
	${CC} ${GCC_FLAGS} ${LD_FLAGS} -Wl,-m,pdp11rt11 -o $@ $(OBJS)

hilo.dsk:	hilo.sav
	rm -f hilo.dsk
	cp ../../resources/rt11os.dsk hilo.dsk
	rt11dsk a hilo.dsk hilo.sav

clean:
	@-rm *.o *.dsk *.sav.map *.sav *.elf
