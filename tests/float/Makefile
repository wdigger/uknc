OBJS:=	float.o
SIMH:= pdp11
CC:=pdp11-uknc-rt11-gcc
OPT_FLAG:=-O2
# -ffunction-sections/-fdata-sections put every function and every
# variable in a section of its own, so that -Wl,--gc-sections below
# can drop the ones nothing reaches.  Only the ELF toolchain can do
# this; the older a.out one warns that it cannot and carries on.
SECTION_FLAGS:=-ffunction-sections -fdata-sections
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer $(OPT_FLAG) $(SECTION_FLAGS)
LD_FLAGS:=-Wl,--gc-sections -Wl,-Map=float.sav.map

# The FIS instructions are on by default for this vendor, so the plain
# build uses them; make NOFIS=1 to check the library routines instead.
# Both have to pass, which is the point -- see float.c.
ifdef NOFIS
GCC_FLAGS+=-mno-fis
endif

all:	float.dsk

%.o:	%.c
	${CC} ${GCC_FLAGS} -c -o $@ $<

float.sav:	$(OBJS)
	${CC} ${GCC_FLAGS} ${LD_FLAGS} -o $@ $(OBJS)

float.dsk:	float.sav
	rm -f float.dsk
	cp ../../resources/rt11os.dsk float.dsk
	rt11dsk a float.dsk float.sav

clean:
	@-rm *.o *.dsk *.sav.map *.sav
