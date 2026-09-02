// ppuc_run.c -- start code running on the PPU

#include <errno.h>

#include "ppu_client.h"
#include "ppuc_internal.h"

int ppuc_run(unsigned short ppu_addr) {
  struct ppu_desc desc;

  desc.stat = 0;
  desc.func = PPU_F_RUN;
  desc.dev = PPU_DEV;
  desc.addr = ppu_addr;
  if (!ppuc_request(&desc)) {
    errno = EIO;
    return -1;
  }
  return 0;
}
