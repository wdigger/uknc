OBJS:=	gfour.o
CC:=pdp11-uknc-rt11-gcc
LDPPU:=pdp11-uknc-rt11-ld-ppu
OPT_FLAG:=-O2
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer $(OPT_FLAG)
LD_FLAGS:=-Wl,--gc-sections
# pdp11-uknc-rt11-ld-ppu (unlike pdp11-uknc-rt11-gcc on the CPU side)
# invokes the raw linker directly, so it never gets GCC's own default
# sysroot library search path -- that's also why it has to add its own
# -L/-lppu for libppu (see the wrapper script itself). libpdp11 isn't
# part of that wrapper (it's not PPU-specific the way libppu is), so
# gfppu.ppu's own link below adds -L/-lpdp11 itself, the same way.
SYSROOT:=../../gcc/xgcc/pdp11-uknc-rt11

all:	gfour.dsk

%.o:	%.c
	${CC} ${GCC_FLAGS} -c -o $@ $<

# -Os, not -O2, for the PPU side specifically: ppuc_load_code() holds
# the whole loaded file plus its laid-out .text/.data/.bss image in
# CPU memory at once, so a PPU program's own compiled size has a real
# ceiling independent of the PPU's own (much roomier) free memory --
# see the note in ../../libs/libppu/ppuc_load_code.c.
gfourppu.o:	gfourppu.c
	${CC} -std=gnu23 -fomit-frame-pointer -Os -c -o $@ $<

# GFOUR.SAV/GFPPU.PPU, not gfour.sav/gfourppu.ppu: RT-11 filenames are
# limited to 6.3 characters -- "gfourppu" is two characters too long --
# so the on-disk names (what gfour.c passes to ppuc_load_code(), and
# what a user types at the KMON prompt to run the CPU side) are
# shortened, not the host-side source filenames.
gfour.sav:	$(OBJS)
	${CC} ${LD_FLAGS} -Wl,-Map=$@.map -o $@ $(OBJS) -lppu -lpdp11

gfppu.ppu:	gfourppu.o
	${LDPPU} -Map=$@.map -o $@ gfourppu.o -L${SYSROOT}/lib -lpdp11

gfour.dsk:	gfour.sav gfppu.ppu
	rm -f gfour.dsk
	cp ../../resources/rt11os.dsk gfour.dsk
	rt11dsk a gfour.dsk gfour.sav
	rt11dsk a gfour.dsk gfppu.ppu

clean:
	@-rm -f *.o *.dsk *.sav.map *.sav *.ppu *.ppu.map
