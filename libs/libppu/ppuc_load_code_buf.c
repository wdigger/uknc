// ppuc_load_code_buf.c -- load a PPU program from a REL module the
// caller already has in memory, at an address the caller picks.
//
// The point of it is what it does *not* need: no file, no heap, no
// errno -- see ppuc_rel.c. A CPU program that links its PPU module in
// as a C array (xxd -i on the .ppu, say) and calls this instead of
// ppuc_load_code() links none of the C library on this account, which
// on a machine with tens of kilobytes to its name is worth more than
// the couple of kilobytes the array itself costs.
//
// The module is only read, never written to, so it may be const -- and
// in the case it exists for, it is: a const array in the program's own
// image.

#include <stddef.h> /* NULL only -- a compiler header, no library code */

#include "ppu_client.h"
#include "ppuc_internal.h"

long ppuc_load_code_buf(const void *module, unsigned int size,
                        unsigned short at) {
  if (module == NULL || size == 0 || (at & 1) != 0) {
    return -1;
  }
  if (!ppuc_rel_load(module, size, at)) {
    return -1;
  }
  return at;
}
