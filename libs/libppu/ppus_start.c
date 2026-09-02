// ppus_start.c -- PPU-side startup shim.
//
// The actual RT-11 REL entry point (a global symbol named exactly
// START, per PRUN's own convention and ppuc_load_code()'s lookup --
// see ppu_client.h) lives here, not in the developer's own program.
// A PPU program instead defines plain `void ppu_main(void) { ... }`;
// this shim calls it, then frees the program's own memory and returns
// to the PPU's resident monitor via ppus_exit() once ppu_main
// returns, matching PRUN's own payload convention (pptest.mac/
// ppcp.mac) automatically -- a program that wants to exit early can
// still call ppus_exit() itself instead of returning.
//
// This file's own .o lives inside libppu.a itself (built into it --
// see ../../libs/libppu/Makefile -- alongside the CPU-side ppuc_*
// members), not shipped as a separate file: nothing in a plain PPU
// program references start()/ppus_exit() by name (the RT-11 loader
// finds start() by its fixed position, not by the linker's own
// symbol resolution -- see ppu.ld), so a normal archive link would
// never pull this member in on its own -- pdp11-uknc-rt11-ld-ppu (see
// ../../libs/libppu/pdp11-uknc-rt11-ld-ppu), the linker every PPU
// program should actually be built with, forces it in with `-u
// start` and applies libppu's ppu.ld linker script, which -- not
// command-line order -- is what actually pins start() at .text
// offset 0; see ppu_server.h/ppu.ld for the full story.

#include "ppu_server.h"

extern void ppu_main(void);

// The entry point must be a global symbol named exactly START (see
// ppuc_load_code()) -- plain `void start(void) { ... }` compiles to
// the symbol `_start` on this target (C's usual leading underscore),
// which folds to `.START` once RADIX-50-packed (`_` has no RADIX-50
// representation) and won't be found. Declare it with an explicit asm
// name to defeat the mangling -- a private detail of this file;
// ppu_server.h's own public API has no need to know start's real
// name.
void start(void) asm("start");

void start(void) {
  ppu_main();
  ppus_exit();
}

// ppus_exit()'s contract is documented in ppu_server.h -- this is its
// one definition. A real function, not forced inline (as it used to
// be, back when this toolchain's `ld -r` couldn't yet correctly merge
// more than one object file -- see ppu_server.h's note on that): it
// only ever needs to run once per program, right before that program
// exits for good, so there's nothing to gain from duplicating it into
// every caller's own .text.
void ppus_exit(void) {
  register unsigned short r1 asm("r1") = (unsigned short)(unsigned int)start;

  asm volatile("jmp @$0176300" ::"r"(r1));
  __builtin_unreachable();
}
