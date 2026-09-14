OBJS:=	sin.o
CC:=pdp11-uknc-rt11-gcc
LDPPU:=pdp11-uknc-rt11-ld-ppu
OPT_FLAG:=-Os
# -ffunction-sections/-fdata-sections put every function and every
# variable in a section of its own, so that -Wl,--gc-sections below
# can drop the ones nothing reaches.  Only the ELF toolchain can do
# this; the older a.out one warns that it cannot and carries on.
SECTION_FLAGS:=-ffunction-sections -fdata-sections
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer $(OPT_FLAG) $(SECTION_FLAGS)
LD_FLAGS:=-Wl,--gc-sections
SYSROOT:=../../gcc/xgcc/pdp11-uknc-rt11

# -Os rather than -O2 here, and it matters: this program carries libm's
# single precision half, and at -O2 it comes out too big for RT-11 to
# load.  See sin.c's header comment for why it is sinf() and not sin().

all:	sin.dsk

%.o:	%.c
	${CC} ${GCC_FLAGS} -c -o $@ $<

# The PPU side is gfour's, built again under a name RT-11 will take: a
# 320x288 four-colour screen and keypresses forwarded as they happen is
# exactly what this program wants, and there is no reason for a second
# copy of it to drift from the first.  -Os for the reason its own
# Makefile gives: a PPU program's loaded image sits in CPU memory while
# ppuc_load_code() works on it.
sinppu.o:	../gfour/gfourppu.c
	${CC} -std=gnu23 -fomit-frame-pointer -Os -c -o $@ $<

sin.sav:	$(OBJS)
	${CC} ${LD_FLAGS} -Wl,-Map=$@.map -o $@ $(OBJS) -lppu -lpdp11 -lm

sinppu.ppu:	sinppu.o
	${LDPPU} -Map=$@.map -o $@ sinppu.o -L${SYSROOT}/lib -lpdp11

sin.dsk:	sin.sav sinppu.ppu
	rm -f sin.dsk
	cp ../../resources/rt11os.dsk sin.dsk
	rt11dsk a sin.dsk sin.sav
	rt11dsk a sin.dsk sinppu.ppu

clean:
	@-rm -f *.o *.dsk *.sav.map *.sav *.ppu *.ppu.map
