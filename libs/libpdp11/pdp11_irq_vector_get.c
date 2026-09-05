// pdp11_irq_vector_get.c -- see pdp11_irq.h

#include "pdp11_irq.h"

struct pdp11_vector pdp11_irq_vector_get(unsigned int vec) {
  struct pdp11_vector v;

  v.pc = *(volatile unsigned short *)vec;
  v.psw = *(volatile unsigned short *)(vec + 2);
  return v;
}
