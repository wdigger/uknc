OBJS:=	ppupong.o
CC:=pdp11-uknc-rt11-gcc
LDPPU:=pdp11-uknc-rt11-ld-ppu
OPT_FLAG:=-O2
# -ffunction-sections/-fdata-sections put every function and every
# variable in a section of its own, so that -Wl,--gc-sections below
# can drop the ones nothing reaches.  Only the ELF toolchain can do
# this; the older a.out one warns that it cannot and carries on.
SECTION_FLAGS:=-ffunction-sections -fdata-sections
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer $(OPT_FLAG) $(SECTION_FLAGS) -g
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
	${CC} -std=gnu23 -fomit-frame-pointer -Os -g -c -o $@ $<

# PING.SAV/PONG.PPU, not ppupong.sav/ppupongppu.ppu: RT-11 filenames
# are limited to 6.3 characters -- "ppupong" itself is already one
# character too long -- so the on-disk names (what ppupong.c passes to
# ppuc_load_code(), and what a user types at the KMON prompt to run
# the CPU side) are shortened, not the host-side source filenames.
ping.sav:	$(OBJS)
	${CC} ${LD_FLAGS} -Wl,-Map=$@.map -o $@ $(OBJS) -lppu

# The same program linked as ELF instead of a flat image: same
# objects, same addresses, but with the symbols and DWARF that a
# .sav has nowhere to keep.  This is what gdb reads.
#
# pong.ppu.elf comes with it: debugging the CPU side is when the PPU
# side's symbols are wanted, and that is the only moment anything asks
# for this target by name (the editor's own F5 builds <name>.elf).
#
# The disk image with it: F5 in the editor builds this target, and a
# program whose PPU module is read from a file needs that file on the
# disk it boots from (the program itself is put into memory by gdb).
ping.elf:	$(OBJS) pong.ppu.elf ppupong.dsk
	${CC} ${LD_FLAGS} -Wl,-m,pdp11rt11 -o $@ $(OBJS) -lppu

pong.ppu:	ppupongppu.o
	${LDPPU} -Map=$@.map -o $@ ppupongppu.o

# The PPU side as ELF: the same objects and the same layout, but with
# the symbols and DWARF an RT-11 object module has nowhere to keep.
# gdb reads this one to show PPU code -- see ../../libs/libppu/ppu.gdb,
# which places it over the running module.  Linked at address 0, since
# where the module ends up is decided when it is loaded.
pong.ppu.elf:	ppupongppu.o
	${LDPPU} --elf -o $@ ppupongppu.o

ppupong.dsk:	ping.sav pong.ppu
	rm -f ppupong.dsk
	cp ../../resources/rt11os.dsk ppupong.dsk
	rt11dsk a ppupong.dsk ping.sav
	rt11dsk a ppupong.dsk pong.ppu

clean:
	@-rm -f *.o *.dsk *.sav.map *.sav *.ppu *.ppu.map *.ppu.elf
