// ppuc_load_reloc.c -- allocate PPU memory, relocate a buffer against its
// newly-allocated base address, and load it
//
// See ppuc_load.c for the plain version with no relocation.

#include <errno.h>
#include <stddef.h>

#include "ppu_client.h"
#include "ppuc_internal.h"

long ppuc_load_reloc(const void *buf, unsigned int size,
                      const struct ppu_reloc *reloc,
                      unsigned int reloc_count) {
  long alloc_result;
  unsigned short ppu_addr;
  unsigned int i;

  // reloc == NULL means nothing to relocate (e.g. position-independent
  // code needs no patching), not an error -- reloc_count is ignored in
  // that case rather than checked, see the loop below.

  alloc_result = ppuc_alloc(size);
  if (alloc_result < 0) {
    return -1;  // errno already set by ppuc_alloc
  }
  ppu_addr = (unsigned short)alloc_result;

  // Patch the buffer in place: each reloc entry names a word (by byte
  // offset into buf) that needs the newly-allocated PPU base address
  // added to, or subtracted from, its current value. Unlike PRUN's own
  // RELOC, there's no "restore original value first" step -- that
  // exists there to let relocation be redone from scratch against a
  // different base; ppuc_load_reloc() only ever relocates once, directly
  // against the caller's own buffer, so the word's current value is
  // taken as already correct for a base of 0.
  for (i = 0; reloc != NULL && i < reloc_count; i++) {
    unsigned short *word;

    if (reloc[i].offset + 2 > size || (reloc[i].offset & 1) != 0) {
      ppuc_free(ppu_addr);
      errno = EINVAL;
      return -1;
    }
    word = (unsigned short *)((const char *)buf + reloc[i].offset);
    if (reloc[i].subtract) {
      *word -= ppu_addr;
    } else {
      *word += ppu_addr;
    }
  }

  if (!ppuc_write_buf(ppu_addr, buf, size)) {
    ppuc_free(ppu_addr);
    errno = EIO;
    return -1;
  }

  return ppu_addr;
}
