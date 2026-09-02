// ppu_server.h -- API for the part of libppu that runs on the PPU
//
// The PPU side: code loaded and started by the CPU (see ppu_client.h)
// runs here, in PPU memory starting at address 1000.
//
// A PPU program's own entry point is plain `void ppu_main(void)` --
// not `start`/START, and no asm-name tricks needed. The real START
// entry symbol PRUN's convention and ppuc_load_code() both require
// lives in ppus_start.c, built right into libppu.a itself (see
// ../../libs/libppu/Makefile); its shim calls ppu_main(), then calls
// ppus_exit() automatically once ppu_main returns.
//
// Build and link with pdp11-uknc-rt11-ld-ppu -- a dedicated linker for
// PPU programs (a thin wrapper around pdp11-uknc-rt11-ld installed
// alongside it -- see ../../libs/libppu/pdp11-uknc-rt11-ld-ppu):
//
//   pdp11-uknc-rt11-gcc -c myprogram.c -o myprogram.o
//   pdp11-uknc-rt11-ld-ppu -o myprogram.ppu myprogram.o
//
// What that wrapper actually runs, and why each piece is there:
//
//   pdp11-uknc-rt11-ld -r -m pdp11rt11rel -T $SYSROOT/lib/ppu.ld \
//       -u start myprogram.o -L$SYSROOT/lib -lppu -o myprogram.ppu
//
// `-u start` is required: nothing in a plain PPU program actually
// references start() by name (the RT-11 loader finds it by its fixed
// position, not by symbol resolution), so without it a normal
// archive link would never pull ppus_start.o's member out of
// libppu.a at all. `-u start` forces exactly that one member in --
// none of libppu.a's other, CPU-side members reference or get
// referenced by anything in a PPU program, so nothing else comes
// along with it.
//
// -T ppu.ld guarantees start() -- and so START's address -- lands at
// .text offset 0, regardless of myprogram.o's own size or content, or
// which order myprogram.o and libppu.a are given in: unlike the
// default pdp11rt11rel script (which just concatenates same-named
// input sections in whatever order the command line gives them),
// ppu.ld pins ppus_start.o's own .text first explicitly -- by name,
// which works the same whether that file is standalone or (as here)
// an archive member. See ppu.ld's own comment for why that guarantee
// matters here.
//
// (Merging 2+ object files like this through `ld -r` used to corrupt
// the output's symbol table on this toolchain's a.out-pdp11 backend --
// fixed at the binutils level; a PPU program is no longer limited to
// a single translation unit.)

#ifndef PPU_SERVER_H
#define PPU_SERVER_H

// Frees this program's own PPU memory block and returns control to
// the PPU's resident monitor. Never returns -- matches PRUN's own
// payload convention (pptest.mac/ppcp.mac): "MOV #START,R1 ; FREE
// MEMORY AND EXIT" / "JMP @#176300", just callable from C instead of
// hand-written in assembly.
//
// Takes no argument: every PPU program is loaded as one module whose
// own base address is, by this project's own convention (see ppu.ld),
// always exactly the module's own entry point -- there's no other
// address a well-behaved PPU program could ever need to free here.
// Defined as a real function in ppus_start.c, alongside the entry
// point itself: the real, asm-named "start" symbol this needs
// internally is a private implementation detail of that file, not
// something this header exposes. No longer needs to be forced inline
// into every caller now that `ld -r` correctly merges multiple
// object files (see the note on this further down).
void ppus_exit(void) __attribute__((noreturn));

// Prints text (a plain C string literal -- see below) through the
// PPU-resident debug monitor's EMT 056 call (see pptest.mac/PRUN:
// "EMT 56" followed inline by ".WORD" holding the string's address).
//
// This is a macro, not a function, because the ROM's EMT 056 handler
// reads its argument from the word immediately following the trap
// instruction in the code stream, then skips over that word before
// resuming -- there is no register/stack argument passing to piggyback
// on, so the address has to be embedded directly at the call site.
// The natural way to do that -- pass text's address as an "i"
// (immediate/constant) asm operand -- runs into a real backend quirk:
// GCC represents "the address of a named object" for an "i" operand
// via its own internal alias symbol (something like "$*name"), not
// the object's own symbol, and this toolchain's pdp11rt11rel emitter
// (ld/emultempl/pdp11rt11rel.em) only emits GSD definitions for
// *global* symbols -- so that alias comes out as an unresolvable
// external reference in the linked module, even though the real
// string is right there in .data. Tested and confirmed with GCC
// 15.2.0 on this target; a plain `const char *` parameter has the same
// problem regardless (every caller's literal ends up going through
// that same alias path).
//
// Sidestepping it: text is spliced (via ordinary adjacent string
// literal concatenation -- text must be a string literal, not a
// variable) directly into the asm template as inline .asciz data,
// with a branch over it so control resumes right after the argument
// word, matching the EMT convention exactly. Local numeric labels
// 1/2 are GNU as's usual reusable kind (like 1f/1b elsewhere in this
// project) -- safe to call this macro more than once per function.
#define ppus_debug_print(text)      \
  asm volatile("emt 0056\n\t"        \
               ".word 1f\n\t"         \
               "br 2f\n\t"             \
               "1: .asciz \"" text "\"\n\t" \
               ".even\n"                \
               "2:" ::: "memory")

#endif  // PPU_SERVER_H
