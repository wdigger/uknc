OBJS:=	math.o
SIMH:= pdp11
CC:=pdp11-uknc-rt11-gcc
OPT_FLAG:=-Os
# -ffunction-sections/-fdata-sections put every function and every
# variable in a section of its own, so that -Wl,--gc-sections below
# can drop the ones nothing reaches.  Only the ELF toolchain can do
# this; the older a.out one warns that it cannot and carries on.
SECTION_FLAGS:=-ffunction-sections -fdata-sections
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer $(OPT_FLAG) $(SECTION_FLAGS)
LD_FLAGS:=-Wl,--gc-sections -Wl,-Map=math.sav.map
LIBS:=-lm

# Optimized for size rather than speed: libm is large and this program has
# to fit in what RT-11 will load.  make NOFIS=1 builds without the FIS
# instructions; both have to pass.
ifdef NOFIS
GCC_FLAGS+=-mno-fis
endif

# libm is large against a 64KB address space, so the checks come in
# groups, one program at a time: 1 is the cheap format-level functions, 2
# is exp and log, 3 is the inverse trigonometry, 4 is sine, 5 is cosine.
# Sine and cosine are apart because together they do not load.
GROUP?=1
GCC_FLAGS+=-DGROUP=$(GROUP)

all:	math.dsk

%.o:	%.c
	${CC} ${GCC_FLAGS} -c -o $@ $<

math.sav:	$(OBJS)
	${CC} ${GCC_FLAGS} ${LD_FLAGS} -o $@ $(OBJS) ${LIBS}

math.dsk:	math.sav
	rm -f math.dsk
	cp ../../resources/rt11os.dsk math.dsk
	rt11dsk a math.dsk math.sav

clean:
	@-rm *.o *.dsk *.sav.map *.sav
