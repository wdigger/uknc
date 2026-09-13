OBJS:=	float.o
SIMH:= pdp11
CC:=pdp11-uknc-rt11-gcc
OPT_FLAG:=-O2
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer $(OPT_FLAG)
LD_FLAGS:=-Wl,--gc-sections -Wl,-Map=float.sav.map

# make FIS=1 adds the FIS instructions, which is what makes multiply and
# divide come out right -- see float.c.  The default build matches the
# toolchain's own default, so it checks the conversions, addition,
# subtraction and comparison only.
ifdef FIS
GCC_FLAGS+=-mfis
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
