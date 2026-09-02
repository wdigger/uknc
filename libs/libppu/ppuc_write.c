// ppuc_write.c -- copy a buffer into already-allocated PPU memory
//
// Shared by ppuc_load.c and ppuc_load_reloc.c -- see ppuc_internal.h.

#include "ppuc_internal.h"

int ppuc_write_buf(unsigned short ppu_addr, const void *buf,
                    unsigned int size) {
  struct ppu_desc desc;

  desc.stat = 0;
  desc.func = PPU_F_WRITE;
  desc.dev = PPU_DEV;
  desc.addr = ppu_addr;
  desc.buff = (unsigned short)buf;
  desc.wcnt = (unsigned short)(size >> 1);
  return ppuc_request(&desc);
}
