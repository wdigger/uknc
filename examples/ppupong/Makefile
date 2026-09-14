OBJS:=	ppupong.o
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

all:	ppupong.dsk

%.o:	%.c
	${CC} ${GCC_FLAGS} -c -o $@ $<

# -Os, not -O2, for the PPU side specifically: ppuc_load_code() holds
# the whole loaded file plus its laid-out .text/.data/.bss image in
# CPU memory at once, so a PPU program's own compiled size has a real
# ceiling independent of the PPU's own (much roomier) free memory --
# see the note in ../../libs/libppu/ppuc_load_code.c. -Os measurably
# shrinks this one.
ppupongppu.o:	ppupongppu.c
	${CC} -std=gnu23 -fomit-frame-pointer -Os -c -o $@ $<

# PING.SAV/PONG.PPU, not ppupong.sav/ppupongppu.ppu: RT-11 filenames
# are limited to 6.3 characters -- "ppupong" itself is already one
# character too long -- so the on-disk names (what ppupong.c passes to
# ppuc_load_code(), and what a user types at the KMON prompt to run
# the CPU side) are shortened, not the host-side source filenames.
ping.sav:	$(OBJS)
	${CC} ${LD_FLAGS} -Wl,-Map=$@.map -o $@ $(OBJS) -lppu

pong.ppu:	ppupongppu.o
	${LDPPU} -Map=$@.map -o $@ ppupongppu.o

ppupong.dsk:	ping.sav pong.ppu
	rm -f ppupong.dsk
	cp ../../resources/rt11os.dsk ppupong.dsk
	rt11dsk a ppupong.dsk ping.sav
	rt11dsk a ppupong.dsk pong.ppu

clean:
	@-rm -f *.o *.dsk *.sav.map *.sav *.ppu *.ppu.map
