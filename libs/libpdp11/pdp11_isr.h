// pdp11_isr.h -- generate full-register-save PDP-11 interrupt
// trampolines that dispatch to a plain C function, instead of
// hand-writing the same save/restore/rti boilerplate at every call
// site
//
// A macro, not a function: each of these needs to emit a bare asm
// label and end on `rti`, not the `rts pc` an ordinary C function
// compiles to (same reasoning as any hand-written ISR trampoline in
// this project -- see e.g. examples/gfour/gfour.c's own gfour_evnt_isr
// comment) -- so the only way to share this across call sites is to
// generate the same text at each one, not call shared code. Nothing
// here is UKNC-specific: which port to read (if any) and which
// callback to dispatch to are always parameters, so these fit any
// PDP-11 program's interrupt handler of the matching shape, not just
// this project's own channel/keyboard/timer ISRs.
//
// PDP11_ISR_RECV_BYTE below: save r0-r5, read one byte from a fixed
// I/O port (also this hardware's own receive acknowledgement -- reading
// it clears the "data ready" latch, same reasoning already documented
// at every existing hand-written version of this), call a C function
// with that byte as its one argument, restore r0-r5, `rti`. Before
// this, libppu's ppuc_recv_isr (ppuc_recv_init.c, channel 1, CPU side)
// and ppus_recv_isr (ppus_recv.c, channel 2, PPU side) were two
// separately hand-written copies of this exact shape, differing only
// in the label name, the port address, and which C function receives
// the byte.
//
// name: the trampoline's own symbol name (bare identifier, no quotes)
//   -- what a vector should point at (see pdp11_irq_vector_set()).
// port: the byte-wide I/O port address to read (bare octal/hex
//   literal, e.g. 0176662).
// callee: the C function to call with the byte as its one argument
//   (bare identifier). Must already be declared with an explicit
//   asm-name matching itself, e.g.
//
//     static void callee(unsigned char b) asm("callee") __attribute__((used));
//
//   -- same reason ppuc_recv_byte_impl/ppus_recv_byte_impl already are:
//   this trampoline's own "jsr pc, callee" text calls that exact,
//   unmangled symbol name, and __attribute__((used)) keeps GCC from
//   discarding a function whose only real caller is inside this raw
//   asm text, invisible to its normal call-graph analysis.
#define PDP11_ISR_RECV_BYTE(name, port, callee)                              \
  asm(#name ":\n\t"                                                          \
      "mov r0, -(sp)\n\t"                                                    \
      "mov r1, -(sp)\n\t"                                                    \
      "mov r2, -(sp)\n\t"                                                    \
      "mov r3, -(sp)\n\t"                                                    \
      "mov r4, -(sp)\n\t"                                                    \
      "mov r5, -(sp)\n\t"                                                    \
      "movb @$" #port ", r0\n\t"                                             \
      "bic $0177400, r0\n\t"                                                 \
      "mov r0, -(sp)\n\t"                                                    \
      "jsr pc, " #callee "\n\t"                                              \
      "add $2, sp\n\t"                                                       \
      "mov (sp)+, r5\n\t"                                                    \
      "mov (sp)+, r4\n\t"                                                    \
      "mov (sp)+, r3\n\t"                                                    \
      "mov (sp)+, r2\n\t"                                                    \
      "mov (sp)+, r1\n\t"                                                    \
      "mov (sp)+, r0\n\t"                                                    \
      "rti\n")

// Generate a full-register-save PDP-11 interrupt trampoline that takes
// no byte off any port at all: save r0-r5, call a plain `void
// callee(void)`, restore r0-r5, `rti`. For an interrupt that carries no
// data of its own -- a line-clock/timer tick being the usual case (see
// examples/gfour/gfour.c's own gfour_evnt_isr) -- as opposed to
// PDP11_ISR_RECV_BYTE above, which always reads one.
//
// name, callee: same meaning as PDP11_ISR_RECV_BYTE's own (bare
// identifiers; callee must already be declared with an explicit
// asm-name matching itself, e.g.
// `static void callee(void) asm("callee") __attribute__((used));`).
#define PDP11_ISR_CALL(name, callee)                                         \
  asm(#name ":\n\t"                                                          \
      "mov r0, -(sp)\n\t"                                                    \
      "mov r1, -(sp)\n\t"                                                    \
      "mov r2, -(sp)\n\t"                                                    \
      "mov r3, -(sp)\n\t"                                                    \
      "mov r4, -(sp)\n\t"                                                    \
      "mov r5, -(sp)\n\t"                                                    \
      "jsr pc, " #callee "\n\t"                                              \
      "mov (sp)+, r5\n\t"                                                    \
      "mov (sp)+, r4\n\t"                                                    \
      "mov (sp)+, r3\n\t"                                                    \
      "mov (sp)+, r2\n\t"                                                    \
      "mov (sp)+, r1\n\t"                                                    \
      "mov (sp)+, r0\n\t"                                                    \
      "rti\n")
