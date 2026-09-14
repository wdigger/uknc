OBJS:=	ppurun.o
SIMH:=	pdp11
CC:=pdp11-uknc-rt11-gcc
LDPPU:=pdp11-uknc-rt11-ld-ppu
OPT_FLAG:=-O2
# -ffunction-sections/-fdata-sections put every function and every
# variable in a section of its own, so that -Wl,--gc-sections below
# can drop the ones nothing reaches.  Only the ELF toolchain can do
# this; the older a.out one warns that it cannot and carries on.
SECTION_FLAGS:=-ffunction-sections -fdata-sections
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer $(OPT_FLAG) $(SECTION_FLAGS)
LD_FLAGS:=-Wl,--gc-sections

all:	ppurun.dsk

%.o:	%.c
	${CC} ${GCC_FLAGS} -c -o $@ $<

ppurun.sav:	$(OBJS)
	${CC} ${LD_FLAGS} -Wl,-Map=$@.map -o $@ $(OBJS) -lppu

pptest.ppu:	pptest.o
	${LDPPU} -Map=$@.map -o $@ pptest.o

ppurun.dsk:	ppurun.sav pptest.ppu
	rm -f ppurun.dsk
	cp ../../resources/rt11os.dsk ppurun.dsk
	rt11dsk a ppurun.dsk ppurun.sav
	rt11dsk a ppurun.dsk pptest.ppu

clean:
	@-rm -f *.o *.dsk *.sav.map *.sav *.ppu *.ppu.map
