// ppurun.c -- load a PPU-side program from a .ppu file and run it.
//
// Demonstrates libppu's ppuc_load_code()/ppuc_run(): the PPU payload
// (pptest.c's ppu_main(), linked with libppu's startup shim
// (ppus_start.o) into a REL module by pdp11-uknc-rt11-ld -r -m
// pdp11rt11rel) proves it's really executing on the second CPU by
// printing through the PPU-resident debug monitor's own EMT 056 call
// -- see libs/libppu/ppu_client.h/ppu_server.h and Oleg Safiullin's
// PRUN pptest.mac, which this is modeled on.
//
// Console messages go through write() rather than printf()/puts():
// mixing buffered stdio with real file I/O in one program has a
// known, unresolved GCC pdp11-backend codegen bug on this toolchain.

#include <fcntl.h>
#include <unistd.h>

#include "ppu_client.h"

static void msg(const char *s) {
  unsigned int len = 0;

  while (s[len] != 0) {
    len++;
  }
  write(STDOUT_FILENO, s, len);
}

int main(void) {
  long ppu_addr;

  msg("ppurun: loading PPTEST.PPU into the PPU...\r\n");
  ppu_addr = ppuc_load_code("PPTEST.PPU");
  if (ppu_addr < 0) {
    msg("ppurun: ppuc_load_code failed\r\n");
    return 1;
  }

  msg("ppurun: starting it...\r\n");
  if (ppuc_run((unsigned short)ppu_addr) < 0) {
    msg("ppurun: ppuc_run failed\r\n");
    return 1;
  }

  msg("ppurun: done -- check the PPU debug screen\r\n");
  return 0;
}
