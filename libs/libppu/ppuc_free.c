// ppuc_free.c -- release a block of PPU memory

#include <errno.h>

#include "ppu_client.h"
#include "ppuc_internal.h"

// Releases a PPU memory block previously returned by a successful
// ppuc_alloc() (or ppuc_load()).
int ppuc_free(unsigned short ppu_addr) {
  struct ppu_desc desc;

  desc.stat = 0;
  desc.func = PPU_F_DEALLOC;
  desc.dev = PPU_DEV;
  desc.addr = ppu_addr;
  if (!ppuc_request(&desc)) {
    errno = EIO;
    return -1;
  }
  return 0;
}
