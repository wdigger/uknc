OBJS:=	pdp11_irq_vector_get.o pdp11_irq_vector_set.o pdp11_irq_vector_swap.o
HDRS:=	pdp11_irq.h pdp11_isr.h
CC:=pdp11-uknc-rt11-gcc
AR:=pdp11-uknc-rt11-ar
OPT_FLAG:=-O2
GCC_FLAGS:=-std=gnu23 -fomit-frame-pointer $(OPT_FLAG)
SYSROOT:=../../gcc/xgcc/pdp11-uknc-rt11

all:	libpdp11.a

%.o:	%.c
	${CC} ${GCC_FLAGS} -c -o $@ $<

libpdp11.a:	$(OBJS)
	${AR} r $@ $(OBJS)

install:	libpdp11.a
	mkdir -p ${SYSROOT}/lib ${SYSROOT}/include
	cp libpdp11.a ${SYSROOT}/lib/
	cp ${HDRS} ${SYSROOT}/include/

clean:
	@-rm -f *.o *.a
