OBJS:=	lseek.o
SIMH:= pdp11
CC:=pdp11-uknc-rt11-gcc
OPT_FLAG:=-O2
# -ffunction-sections/-fdata-sections put every function and every
# variable in a section of its own, so that -Wl,--gc-sections below
# can drop the ones nothing reaches.  Only the ELF toolchain can do
# this; the older a.out one warns that it cannot and carries on.
SECTION_FLAGS:=-ffunction-sections -fdata-sections
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer $(OPT_FLAG) $(SECTION_FLAGS)
LD_FLAGS:=-Wl,--gc-sections -Wl,-Map=lseek.sav.map

all:	lseek.dsk

%.o:	%.c
	${CC} ${GCC_FLAGS} -c -o $@ $<

lseek.sav:	$(OBJS)
	${CC} ${GCC_FLAGS} ${LD_FLAGS} -o $@ $(OBJS)

lseek.dsk:	lseek.sav
	rm -f lseek.dsk
	cp ../../resources/rt11os.dsk lseek.dsk
	rt11dsk a lseek.dsk lseek.sav

clean:
	@-rm *.o *.dsk *.sav.map *.sav
