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

// Largest buffer the callback passed to ppus_recv_init() will ever be
// called with. ppuc_send() on the CPU side doesn't enforce this -- it
// just sends whatever size it's given -- so a message longer than
// this is silently truncated to this many bytes on arrival, not
// rejected; keep messages within this on the CPU side if that
// matters. A plain compile-time constant, not configurable per
// program: ppus_recv.o's own receive buffer is sized to it statically
// (see ppus_recv.c), so changing it means rebuilding libppu.a, not
// just the calling program.
#define PPUS_RECV_MAX 64

// The signature ppus_recv_init()'s callback argument must have. Called
// once for every buffer ppuc_send() delivers, once ppus_recv_init()
// has armed the receiver -- buf points at an internal buffer private
// to ppus_recv.c (valid only until the callback returns; copy anything
// you need to keep), and size is however many bytes of it are
// actually valid (at most PPUS_RECV_MAX, see above). Runs inside the
// interrupt handler ppus_recv_init() installs, with interrupts at that
// channel's own priority level masked (ordinary PDP-11
// vectored-interrupt behavior) until it returns -- keep it short, the
// same way any interrupt handler should be, and don't call
// ppuc_*-family functions from it (those are for the CPU side; this
// runs on the PPU).
typedef void (*ppus_receive_fn)(const void *buf, unsigned int size);

// Arms this program to receive buffers sent from the CPU via
// ppuc_send() (see ppu_client.h): installs an interrupt handler for
// the CPU<->PPU exchange channel and unmasks interrupts on this
// processor (nothing before this point in a PPU program's life needs
// them, so nothing has lowered its priority before now). callback is
// called for every buffer that arrives afterward -- see
// ppus_receive_fn above for its contract; it must stay valid (a real
// function, not something on a stack that later goes out of scope)
// for as long as the receiver stays armed, which in practice means for
// the rest of the program's life, unless disarmed with
// ppus_recv_shutdown() below.
// Call this once from ppu_main(), before doing anything that would
// race with a command arriving -- there is no other setup needed.
//
// Implemented in ppus_recv.c, a separate, ordinary libppu.a archive
// member: unlike ppus_start.c's shim, this one is never forced into a
// PPU program's link (no `-u` for it) -- it only comes along if a
// program actually calls this function, the same ordinary
// demand-driven archive linking libppu's CPU-side functions already
// rely on.
void ppus_recv_init(ppus_receive_fn callback);

// Disarms the receiver: puts vector 0340/0342 back to whatever the
// PPU-resident monitor had there before ppus_recv_init() installed
// its own handler, and disables channel 2's RX interrupt again. Any
// program that calls ppus_recv_init() MUST call this before returning
// from ppu_main() (before ppus_start.c's shim runs ppus_exit(), which
// frees this program's own PPU memory block) -- otherwise vector 0340
// is left pointing at memory that's no longer guaranteed to hold
// ppus_recv_isr, and a later channel-2 interrupt (e.g. from the
// resident monitor's own use of ppuc_request() on behalf of a
// subsequent ppuc_load_code()/ppuc_alloc() call) jumps into whatever's
// actually there by then. Confirmed directly: omitting this call
// reproduces a monitor halt shortly after an otherwise-correct
// ppuc_send()/ppuc_recv() exchange completes and the CPU side reaches
// its own .EXIT.
void ppus_recv_shutdown(void);

// Sends buf (size bytes) to the CPU, for a program to pick up with
// ppuc_recv() (see ppu_client.h) -- the PPU-side counterpart to
// ppuc_send()/ppus_recv_init(), in the opposite direction. Blocks
// until every byte is out; no reply, and nothing to report failure
// with -- this either lands as the next call to ppuc_recv() on the
// CPU side, or, if nothing there is calling that, just sits in
// channel 1's own hardware latch until something does.
//
// If this program also uses ppus_recv_init() (the CPU->PPU
// direction), the CPU side must make its first ppuc_send() call
// before calling ppuc_recv() to read anything sent here -- see
// ppuc_recv.c's header comment for why; that's a CPU-side
// precondition, nothing this function itself needs to worry about.
//
// Implemented in ppus_send.c, a separate, ordinary libppu.a archive
// member -- only linked into a PPU program's module if that program
// actually calls this function, same as ppus_recv_init() above.
void ppus_send(const void *buf, unsigned int size);

#endif  // PPU_SERVER_H
