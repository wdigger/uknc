OBJS:=	example.o
SIMH:= pdp11
CC:=pdp11-aout-gcc
OPT_FLAG:=-O2
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer -msoft-float -fcprop-registers -fPIC -nostartfiles -nodefaultlibs -nostdlib -mbm2 $(OPT_FLAG)
AS_FLAGS:=-mno-fpu -mlimited-eis -pic

all:	example.dsk

%.o:	%.c
	pdp11-aout-gcc ${GCC_FLAGS} -c -o $@ $<

%.o:	%.s
	pdp11-aout-as ${AS_FLAGS} -o $@ $<

%.lda:	%.out
	bin2load -a -f $< -o $@

exam.sav:	example.lda
	lda2sav -o $@ $<

example.out: $(OBJS) crt0rt.o pdp11-aout.ld
	pdp11-aout-ld --gc-sections -T pdp11-aout.ld -o $@ $(OBJS) -Map=example.out.map

example.dsk:	exam.sav
	rm -f example.dsk
	cp rt11os.dsk example.dsk
	rt11dsk a example.dsk exam.sav

clean:
	@-rm *.lda *.out *.o example.dsk *.out.map *.sav
