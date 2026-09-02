// pptest.c -- PPU-side payload for the ppurun example
//
// Modeled on Oleg Safiullin's PRUN pptest.mac
// (pdp-11.org.ru/~form/files/pdp-11/uknc/prun/): proves this code is
// really executing on the PPU by printing a fixed string through
// ppus_debug_print() (EMT 056). ppu_main() is this program's own
// entry point -- libppu's startup shim (ppus_start.c) calls it, then
// frees this program's memory and returns to the PPU's resident
// monitor automatically once ppu_main returns; see ppu_server.h.
//
// Built (no gcc driver, no startfiles -- see ../Makefile) as a single
// translation unit, linked with pdp11-uknc-rt11-ld-ppu -- the
// dedicated linker for PPU programs (a thin wrapper around
// pdp11-uknc-rt11-ld that already applies everything a PPU-side link
// needs -- ppu.ld, -u start, -lppu; see ppu_server.h/ppu.ld/
// ppus_start.c for why) -- straight into a REL module:
//
//   pdp11-uknc-rt11-gcc -c pptest.c -o pptest.o
//   pdp11-uknc-rt11-ld-ppu -o pptest.ppu pptest.o

#include "ppu_server.h"

void ppu_main(void) {
  ppus_debug_print(" PPU IS ALIVE");
}
