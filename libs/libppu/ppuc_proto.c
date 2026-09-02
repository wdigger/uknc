// ppuc_proto.c -- low-level CPU<->PPU request protocol
//
// See ppuc_internal.h for the port/descriptor layout this implements.

#include "ppuc_internal.h"

int ppuc_request(struct ppu_desc *desc) {
  unsigned short addr = (unsigned short)desc;
  unsigned char bytes[4];
  int i;

  bytes[0] = (unsigned char)addr;
  bytes[1] = (unsigned char)(addr >> 8);
  bytes[2] = 0377;
  bytes[3] = 0377;

  asm volatile("mtps $0340" ::: "cc");
  for (i = 0; i < 4; i++) {
    while ((PPU_CSR & 0200) == 0) {
    }
    PPU_DATA = bytes[i];
  }
  // PRUN's PPURQ waits for readiness ONE more time after the last byte
  // (its SOB counter starts at 4+1, not 4) before checking stat -- an
  // extra settle/handshake step this loop was missing.
  while ((PPU_CSR & 0200) == 0) {
  }
  asm volatile("mtps $0" ::: "cc");

  return desc->stat == 0;
}
