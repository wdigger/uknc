OBJS:=	fileio.o
SIMH:= pdp11
CC:=pdp11-uknc-rt11-gcc
OPT_FLAG:=-O2
# -ffunction-sections/-fdata-sections put every function and every
# variable in a section of its own, so that -Wl,--gc-sections below
# can drop the ones nothing reaches.  Only the ELF toolchain can do
# this; the older a.out one warns that it cannot and carries on.
SECTION_FLAGS:=-ffunction-sections -fdata-sections
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer $(OPT_FLAG) $(SECTION_FLAGS)
LD_FLAGS:=-Wl,--gc-sections -Wl,-Map=fileio.sav.map

all:	fileio.dsk

%.o:	%.c
	${CC} ${GCC_FLAGS} -c -o $@ $<

fileio.sav:	$(OBJS)
	${CC} ${GCC_FLAGS} ${LD_FLAGS} -o $@ $(OBJS)

fileio.dsk:	fileio.sav
	rm -f fileio.dsk
	cp ../../resources/rt11os.dsk fileio.dsk
	rt11dsk a fileio.dsk fileio.sav

clean:
	@-rm *.o *.dsk *.sav.map *.sav
