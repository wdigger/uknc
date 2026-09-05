// pdp11_irq.h -- interrupt-related primitives shared by CPU- and
// PPU-side programs
//
// Nothing here is UKNC-specific: both the CPU and the PPU are
// PDP-11-compatible cores with their own, separate interrupt state
// (priority in PSW, vector table at the same low addresses), and
// everything below is plain PDP-11 architecture -- just not something
// GCC's own libgcc provides a name for, unlike the instructions it
// compiles from ordinary C.

#ifndef PDP11_IRQ_H
#define PDP11_IRQ_H

#include "pdp11_isr.h"

// Raise/lower this processor's own interrupt priority.
//
// MTPS sets the processor status word's priority field directly.
// Wrapped here as two named, one-instruction primitives instead of the
// bare asm every real call site already duplicated verbatim (libppu's
// ppuc_proto.c/ppuc_send.c, and the masked section of every vector
// install/restore across this project -- see pdp11_irq_vector_set()/
// pdp11_irq_vector_swap() below, the first users of these). static
// inline, not a real .a member: a single instruction has nothing to
// gain from being a real call (return address, stack frame) instead of
// inlining straight to the mtps itself.
//
// Priority 7 (0340) masks every maskable interrupt on this processor;
// 0 unmasks everything. This is the plain, non-nesting raise/lower
// pair every existing call site already used bare -- not a general
// priority-save/restore mechanism (nothing here remembers what the
// priority was before pdp11_irq_disable()); a caller that needs that
// should save PSW itself first.
static inline void pdp11_irq_disable(void) {
  asm volatile("mtps $0340" ::: "cc");
}

static inline void pdp11_irq_enable(void) {
  asm volatile("mtps $0" ::: "cc");
}

// Read/install PDP-11 interrupt vectors.
//
// A vector here is the ordinary PDP-11 kind: two words, memory-mapped
// at a fixed low address (100, 300, 340, 460, ...) -- [PC, PSW] of the
// handler to run when that interrupt fires.
//
// Before this library, every interrupt-driven example/library file in
// this project (libppu's ppuc_recv_init.c/ppus_recv.c, examples/gfour's
// gfour.c/gfourppu.c) hand-rolled its own near-identical asm to save the
// old vector and install a new one. Factored out here once both sides
// of that duplication were the same shape: read the two words, and
// write them back masked so a pending interrupt on that exact vector
// can never fire mid-update and see a half-old/half-new [PC, PSW] pair.
struct pdp11_vector {
  unsigned short pc;
  unsigned short psw;
};

// Reads the vector at address vec (e.g. 0300 for the keyboard, 0100 for
// EVNT) -- a plain, unmasked read: nothing acts on a vector's contents
// just by it being read, so there's no half-old/half-new hazard here
// the way there is for pdp11_irq_vector_set() below.
struct pdp11_vector pdp11_irq_vector_get(unsigned int vec);

// Installs v at address vec, replacing whatever was there. Masks this
// processor's own interrupt priority to 7 for the two-word write and
// drops it back to 0 once done, so an interrupt already pending on
// this exact vector can't fire in between the PC and PSW word landing
// and jump through a half-written pair. Callers that need the previous
// contents (almost always, to put back before exiting -- see
// pdp11_irq_vector_get() above) must read them first: this always
// overwrites unconditionally, there is no built-in swap.
void pdp11_irq_vector_set(unsigned int vec, struct pdp11_vector v);

// Reads the vector at vec and installs v in its place, returning what
// was there before -- what every real call site in this project
// actually wants (install a handler, remember the original to restore
// later), in one call instead of a separate pdp11_irq_vector_get() +
// pdp11_irq_vector_set() pair. Masked the same way
// pdp11_irq_vector_set() is, but as a single critical section covering
// the read *and* the write (not two separate masked sections back to
// back), matching the exact shape every existing hand-rolled
// vector-install already used.
struct pdp11_vector pdp11_irq_vector_swap(unsigned int vec,
                                           struct pdp11_vector v);

// Define an interrupt handler with every asm/register/vector detail
// hidden -- built on top of pdp11_isr.h's own PDP11_ISR_RECV_BYTE/
// PDP11_ISR_CALL trampolines (see there for the exact asm each of
// these generates), spelled as a prefix on the callback's own
// definition instead of a separate declaration-plus-macro-invocation
// pair, so a whole interrupt handler is just:
//
//   PDP11_IRQ_HANDLER(vsync_tick) { vsync_count++; }
//
//   PDP11_IRQ_RECV_BYTE(kbd_recv_byte, 0177702) {
//     kbd_event = b;
//     kbd_pending = 1;
//   }
//
// `b` in the second one is the received byte, always named that. name
// itself -- unmodified, exactly as written -- is the trampoline, what
// a vector should actually be pointed at (see pdp11_irq_vector_set()
// above): both macros declare it themselves (`extern void name(void)
// asm(#name)`), so a caller just writes e.g. `v.pc = (unsigned
// short)(unsigned int)vsync_tick;` directly, no separate declaration
// needed. The braced body the macro is a prefix to still has to
// compile as an ordinary C function, though (this backend has no way
// to make a ordinary-looking function body end in `rti` instead of
// `rts pc`), so that part gets a generated, private name instead --
// `<name>_impl` (e.g. vsync_tick_impl, kbd_recv_byte_impl) -- matching
// the same `_impl`-suffix convention libppu's own
// ppuc_recv_byte_impl/ppus_recv_byte_impl already use for exactly this
// role. Nothing outside this header ever needs to know that second
// name exists.
//
// This can't be spelled as a bare prefix before an entirely untouched
// `static void vsync_tick(void) { ... }` the way a real compiler
// attribute (this backend has none for interrupts) would allow --
// nothing in the C preprocessor can see a name that hasn't been handed
// to it as a macro argument, and generating the private `_impl` name
// needs that name at least once. Taking it as an explicit argument,
// right where the callback's own name would go, is as close as a
// macro can get.
#define PDP11_IRQ_HANDLER(name)                                               \
  extern void name(void) asm(#name);                                         \
  static void name##_impl(void) asm(#name "_impl") __attribute__((used));    \
  PDP11_ISR_CALL(name, name##_impl);                                         \
  static void name##_impl(void)

#define PDP11_IRQ_RECV_BYTE(name, port)                                      \
  extern void name(void) asm(#name);                                         \
  static void name##_impl(unsigned char) asm(#name "_impl")                  \
      __attribute__((used));                                                 \
  PDP11_ISR_RECV_BYTE(name, port, name##_impl);                              \
  static void name##_impl(unsigned char b)

#endif  // PDP11_IRQ_H
