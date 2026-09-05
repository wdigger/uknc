// pdp11_irq_vector_set.c -- see pdp11_irq.h

#include "pdp11_irq.h"

void pdp11_irq_vector_set(unsigned int vec, struct pdp11_vector v) {
  pdp11_irq_disable();
  *(volatile unsigned short *)vec = v.pc;
  *(volatile unsigned short *)(vec + 2) = v.psw;
  pdp11_irq_enable();
}
