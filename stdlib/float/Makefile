LIB_OBJS:=	pdp11-fis-asm.o pdp11-floatsisf.o
TEST_OBJS:=	test.o
SIMH:= pdp11
CC:=pdp11-aout-gcc
OPT_FLAG:=-O2
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer -msoft-float -fcprop-registers -fPIC -nostartfiles -nodefaultlibs -nostdlib -mbm2 $(OPT_FLAG)
AS_FLAGS:=-mno-fpu -meis -pic

all:	libfloat.a test.sav test.dsk

%.o:	%.c
	pdp11-aout-gcc ${GCC_FLAGS} -c -o $@ $<

%.o:	%.s
	pdp11-aout-as ${AS_FLAGS} -o $@ $<

libfloat.a:	$(LIB_OBJS)
	pdp11-aout-ar r $@ $(LIB_OBJS)

%.lda:	%.out
	bin2load -a -f $< -o $@

test.sav:	test.lda
	lda2sav -o $@ $<

test.out: $(TEST_OBJS) crt0rt.o pdp11-aout.ld
	pdp11-aout-ld --gc-sections -T pdp11-aout.ld -o $@ $(TEST_OBJS) libfloat.a

test.dsk:	test.sav
	rm -f test.dsk
	cp rt11os.dsk test.dsk
	rt11dsk a test.dsk test.sav

clean:
	@-rm *.lda *.out *.o test.dsk *.map *.sav *.a
