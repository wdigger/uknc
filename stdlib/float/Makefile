LIB_OBJS:=	pdp11-fis.o
TEST_OBJS:=	test.o
SIMH:= pdp11
CC:=pdp11-aout-gcc
AS:=pdp11-aout-as
AR:=pdp11-aout-ar
LD:=pdp11-aout-ld
OPT_FLAG:=-O0
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer -msoft-float -fcprop-registers -fPIC -nostartfiles -nodefaultlibs -nostdlib -mbm2 -fno-builtin $(OPT_FLAG)
AS_FLAGS:=-mno-fpu -meis -mfis -pic

all:	libpdp11-float.a test.sav test.dsk

%.o:	%.c
	${CC} ${GCC_FLAGS} -c -o $@ $<

%.o:	%.s
	${AS} ${AS_FLAGS} -o $@ $<

libpdp11-float.a:	$(LIB_OBJS)
	${AR} r $@ $(LIB_OBJS)

%.lda:	%.out
	bin2load -a -f $< -o $@

test.sav:	test.lda
	lda2sav -o $@ $<

test.out: $(TEST_OBJS) crt0rt.o pdp11-aout.ld
	${LD} --gc-sections -T pdp11-aout.ld -o $@ $(TEST_OBJS) libpdp11-float.a

test.dsk:	test.sav
	rm -f test.dsk
	cp rt11os.dsk test.dsk
	rt11dsk a test.dsk test.sav

clean:
	@-rm *.lda *.out *.o test.dsk *.map *.sav *.a
