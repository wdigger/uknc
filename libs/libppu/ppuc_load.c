// ppuc_load.c -- allocate PPU memory and load a buffer into it unmodified
//
// See ppuc_load_reloc.c for the version that patches the buffer against
// its newly-allocated base address first.

#include <errno.h>

#include "ppu_client.h"
#include "ppuc_internal.h"

long ppuc_load(const void *buf, unsigned int size) {
  long alloc_result;
  unsigned short ppu_addr;

  alloc_result = ppuc_alloc(size);
  if (alloc_result < 0) {
    return -1;  // errno already set by ppuc_alloc
  }
  ppu_addr = (unsigned short)alloc_result;

  if (!ppuc_write_buf(ppu_addr, buf, size)) {
    ppuc_free(ppu_addr);
    errno = EIO;
    return -1;
  }

  return ppu_addr;
}
