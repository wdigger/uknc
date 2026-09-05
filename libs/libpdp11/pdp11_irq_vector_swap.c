// pdp11_irq_vector_swap.c -- see pdp11_irq.h

#include "pdp11_irq.h"

struct pdp11_vector pdp11_irq_vector_swap(unsigned int vec,
                                           struct pdp11_vector v) {
  struct pdp11_vector old;

  pdp11_irq_disable();
  old.pc = *(volatile unsigned short *)vec;
  old.psw = *(volatile unsigned short *)(vec + 2);
  *(volatile unsigned short *)vec = v.pc;
  *(volatile unsigned short *)(vec + 2) = v.psw;
  pdp11_irq_enable();

  return old;
}
