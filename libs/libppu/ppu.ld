/* ppu.ld -- linker script for building a PPU-side .ppu program.
 *
 * Guarantees libppu's startup shim (ppus_start.o -- see
 * ppus_start.c/ppu_server.h) lands at .text offset 0, and so at PPU
 * load address 0 relative to the module -- the fixed convention
 * ppuc_load_code() relies on to find START without any per-module
 * symbol lookup (see ../../libs/libppu/ppuc_load_code.c) -- no matter
 * what order object files are listed on the `ld` command line, or how
 * large they are.
 *
 * Without this, whichever file happens to come first on the command
 * line determines where start() lands, and there is nothing to catch
 * a mistake: the REL format has no separate "entry point" field of
 * its own for ppuc_load_code() to cross-check against, so listing
 * ppus_start.o anywhere but first would silently produce a module
 * that loads fine but runs whatever wrong code happens to sit at
 * offset 0.
 *
 * Use in place of the default pdp11rt11rel script, in any file order:
 *
 *   pdp11-uknc-rt11-ld -r -m pdp11rt11rel -T ppu.ld \
 *       myprogram.o $(SYSROOT)/lib/ppus_start.o -o myprogram.ppu
 *
 * *ppus_start.o below (a leading glob, not a bare filename) matches
 * regardless of the path ppus_start.o is actually given at -- e.g.
 * both "ppus_start.o" and ".../lib/ppus_start.o" end in that name.
 */

OUTPUT_FORMAT("a.out-pdp11", "a.out-pdp11", "a.out-pdp11")
OUTPUT_ARCH(pdp11)

SECTIONS
{
  .text :
  {
    *ppus_start.o(.text)
    *(.text)
  }
  .data :
  {
    *(.rodata)
    *(.rodata.*)
    *(.data)
  }
  .bss :
  {
    *(.bss)
    *(COMMON)
  }
}
