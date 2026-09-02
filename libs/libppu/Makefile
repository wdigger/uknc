OBJS:=	ppuc_proto.o ppuc_alloc.o ppuc_free.o ppuc_write.o ppuc_run.o ppuc_load.o ppuc_load_reloc.o ppuc_load_file.o ppuc_load_code.o ppus_start.o
HDRS:=	ppu_client.h ppu_server.h
CC:=pdp11-uknc-rt11-gcc
AR:=pdp11-uknc-rt11-ar
OPT_FLAG:=-O2
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer $(OPT_FLAG)
SYSROOT:=../../gcc/xgcc/pdp11-uknc-rt11
BINDIR:=../../gcc/bin

all:	libppu.a

%.o:	%.c
	${CC} ${GCC_FLAGS} -c -o $@ $<

# ppus_start.o's start() must land at .text offset 0 within its own
# object file (see ppu.ld, which pins this whole file's .text first in
# a PPU program's merged module) -- GCC is otherwise free to reorder
# top-level definitions for its own reasons, and does (observed:
# ppus_exit() ending up first instead, once it became a second,
# separately-compiled function rather than inlined into start() alone).
# -fno-toplevel-reorder keeps them in source order, matching this file's
# own real requirement, not just GCC's default heuristic.
ppus_start.o:	ppus_start.c
	${CC} ${GCC_FLAGS} -fno-toplevel-reorder -c -o $@ $<

libppu.a:	$(OBJS)
	${AR} r $@ $(OBJS)

install:	libppu.a
	mkdir -p ${SYSROOT}/lib ${SYSROOT}/include
	cp libppu.a ppu.ld ${SYSROOT}/lib/
	cp ${HDRS} ${SYSROOT}/include/
	cp pdp11-uknc-rt11-ld-ppu ${BINDIR}/
	chmod +x ${BINDIR}/pdp11-uknc-rt11-ld-ppu

clean:
	@-rm -f *.o *.a
