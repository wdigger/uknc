// ppuc_alloc.c -- allocate a block of PPU memory

#include <errno.h>

#include "ppu_client.h"
#include "ppuc_internal.h"

// Asks the PPU for size bytes of its own memory. The requested size
// (in words) goes in both ppu_desc.buff and ppu_desc.wcnt -- PRUN's
// own PPURQ call ahead of RELOC only sets P.BUFF, but UKNC-soft's
// libgraph.s _RunPPU sets both fields to the same value for its ALLOC
// call (github.com/anpp/UKNC-soft/blob/main/libgraph/libgraph.s), so
// this sets both to match the known-working reference -- harmless if
// only one of the two is actually read.
long ppuc_alloc(unsigned int size) {
  struct ppu_desc desc;

  if (size == 0 || (size & 1) != 0) {
    errno = EINVAL;
    return -1;
  }

  desc.stat = 0;
  desc.func = PPU_F_ALLOC;
  desc.dev = PPU_DEV;
  desc.buff = (unsigned short)(size >> 1);
  desc.wcnt = (unsigned short)(size >> 1);
  if (!ppuc_request(&desc)) {
    errno = ENOMEM;
    return -1;
  }
  return desc.addr;
}
