// ppuc_load_code.c -- load an RT-11 native relocatable object module
// ("REL": GSD/TXT/RLD/ENDMOD blocks) into PPU memory. The entry point
// is always the returned load address itself -- see below.
//
// Produced directly by this toolchain's own linker, no extra tooling:
//
//   pdp11-uknc-rt11-as foo.s -o foo.o
//   pdp11-uknc-rt11-ld -r -m pdp11rt11rel foo.o -o foo.ppu
//
// This is a small single-module linker, not a generic reader of every
// RT-11 REL file anyone could produce: it only understands the fixed
// .text/.data/.bss/.ABS. p-sect model and the relocation subset that
// ld/emultempl/pdp11rt11rel.em's own emitter ever writes for a single
// module with no unresolved external references -- exactly what a
// self-contained PPU program (now possibly linked from more than one
// object file -- see ppu_server.h/ppus_start.c) compiles to.
//
// PC-relative ("_DISP") entries -- which a cross-function call between
// two merged object files produces, unlike a same-file call the
// assembler already resolves without any relocation at all -- need no
// patch: the operand word already holds the correct, self-relative
// distance between the reference and its same-module target, and
// loading the whole module at a different base shifts both by the
// same amount, which cancels out. So these are only ever parsed far
// enough to skip their operand correctly, never written to. A
// PC-relative reference to a true external symbol (REL_GSD_GLOBAL
// under .ABS., not one of this module's own p-sects -- GLOBAL_DISP/
// GLOBAL_ADD_DISP) is a different, still-unsupported case (EINVAL):
// this loader has no way to resolve an address outside its own
// module.
//
// The object-module format has no "transfer address" field of its own
// (that's only meaningful for a fully linked image); the entry point
// is, by this project's own convention, always .text offset 0 --
// which is why libppu's startup shim (ppus_start.c/ppu_server.h) must
// always be the *first* object file named on the `ld -r` command
// line that produces the module, so its own start() lands there
// regardless of what's linked in after it. No per-module symbol
// lookup is needed to find it.
//
// The reading of the module and the understanding of it are separate:
// everything about the REL format itself lives in ppuc_rel.c, and this
// file is only the part that needs a file -- open, one heap buffer for
// the whole .PPU, read, hand it to ppuc_rel_load(), free. That split is
// what lets a program which embeds its PPU module in its own image call
// ppuc_load_code_buf() and link no C library at all; a program that
// loads from a file pays for open/read/malloc, as it must.
//
// Only one CPU-side heap buffer is ever held: buf, the whole .PPU file,
// read verbatim. It is not written to -- ppuc_rel.c copies each TXT
// chunk out before patching relocations into it.
//
// This avoids a real constraint the previous two-buffer design had,
// found the hard way while testing ppus_send()/ppuc_recv() (see
// ppu_client.h): holding both buf and a same-order-of-magnitude
// content buffer at once meant combined peak size scaled with the
// *PPU* program being loaded, not the calling CPU program -- a CPU
// program that comfortably loaded a small PPU module could get
// ENOMEM loading a larger one, even with the PPU itself having plenty
// of free memory (confirmed directly: ppuc_alloc() for the same size,
// called on its own, succeeded -- the failure was the second
// malloc() on the CPU side). Halving the CPU-side buffer requirement
// removes that scaling entirely.

#include <errno.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ppu_client.h"
#include "ppuc_internal.h"

// Reads name into a fresh heap buffer. Returns it (and its size in
// *size_out) on success, NULL on any failure, errno already set.
static unsigned char *read_module(const char *name, unsigned int *size_out) {
  int fd, n, file_pos;
  struct stat st;
  unsigned char *buf;
  unsigned int size;

  fd = open(name, O_RDONLY, 0);
  if (fd < 0) {
    return NULL;
  }
  if (fstat(fd, &st) < 0) {
    close(fd);
    return NULL;
  }
  size = (unsigned int)st.st_size;
  buf = malloc(size > 0 ? size : 1);
  if (buf == NULL) {
    close(fd);
    errno = ENOMEM;
    return NULL;
  }
  file_pos = 0;
  while ((unsigned int)file_pos < size) {
    n = read(fd, buf + file_pos, size - (unsigned int)file_pos);
    if (n <= 0) {
      break;
    }
    file_pos += n;
  }
  close(fd);
  if ((unsigned int)file_pos != size) {
    free(buf);
    errno = EIO;
    return NULL;
  }
  *size_out = size;
  return buf;
}

// The loader proper: with fixed == 0 the module goes into a block
// ppuc_alloc() hands out -- which means asking ppuc_rel.c how big the
// laid-out image will be before loading it -- and with fixed != 0 it
// goes to `at`, an address the caller vouches for (see
// ppuc_load_code_at()).
static long load_code(const char *name, unsigned short at, int fixed) {
  unsigned char *buf;
  unsigned int size, content_size;
  long alloc_result;
  unsigned short ppu_addr;

  buf = read_module(name, &size);
  if (buf == NULL) {
    return -1;
  }

  if (fixed) {
    ppu_addr = at;
  } else {
    content_size = ppuc_rel_content_size(buf, size);
    if (content_size == 0) {
      free(buf);
      errno = EINVAL;
      return -1;
    }
    alloc_result = ppuc_alloc(content_size);
    if (alloc_result < 0) {
      free(buf);
      return -1;  // errno already set by ppuc_alloc
    }
    ppu_addr = (unsigned short)alloc_result;
  }

  if (!ppuc_rel_load(buf, size, ppu_addr)) {
    if (!fixed) {
      ppuc_free(ppu_addr);
    }
    free(buf);
    errno = EINVAL;  // a malformed module, or a PPU write that failed
    return -1;
  }

  free(buf);
  return ppu_addr;
}

long ppuc_load_code(const char *name) {
  return load_code(name, 0, 0);
}

long ppuc_load_code_at(const char *name, unsigned short at) {
  if ((at & 1) != 0) {
    errno = EINVAL;
    return -1;
  }
  return load_code(name, at, 1);
}
