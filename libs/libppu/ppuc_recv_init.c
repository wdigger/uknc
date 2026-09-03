// ppuc_recv_init.c -- interrupt-driven receiver for ppus_send() (see
// ppus_send.c/ppu_server.h), the CPU-side counterpart to
// ppus_recv_init() in the opposite direction
//
// Opt-in, like ppus_recv.c on the PPU side: this file is a normal
// libppu.a archive member, only linked into a CPU program's module if
// that program actually calls ppuc_recv_init().
//
// Hardware background, confirmed directly against ukncbtl's own
// emulator model (emulator/emubase/Board.cpp/Memory.cpp), the same
// way ppus_recv.c's own channel-2 mechanism was: 0176662 is channel
// 1's CPU-side data register (PPU_CHAN1_DATA, see ppuc_internal.h --
// already shared with ppuc_send.c's/ppuc_recv.c's own polling code),
// and bit 6 (0100) of 0176660 (PPU_CHAN1_CSR) is that channel's own
// RX-interrupt-enable bit -- writing to this address dispatches to
// Board.cpp's ChanRxStateSetCPU(1, ...), a direct mirror of
// ChanRxStateSetPPU() on the other side. The vector for "the PPU just
// wrote a byte on channel 1" is a fixed 0460 (Board.cpp's
// ChanWriteByPPU()'s own InterruptVIRQ(3, 0460) call for chan==1).

#include "ppu_client.h"
#include "ppuc_internal.h"

static ppuc_receive_fn ppuc_recv_callback;

static unsigned char ppuc_recv_buf[PPUC_RECV_MAX];
static unsigned int ppuc_recv_want;   // total payload length the sender announced
static unsigned int ppuc_recv_got;    // payload bytes received so far
static unsigned char ppuc_recv_state; // 0=length lo, 1=length hi, 2=payload

// RT-11's own vector 0460/0462 contents, saved by ppuc_recv_init()
// before overwriting them -- see ppuc_recv_shutdown() for why putting
// them back matters.
static unsigned short ppuc_recv_saved_vec_pc;
static unsigned short ppuc_recv_saved_vec_psw;

// Called once per incoming byte, from the raw asm trampoline below --
// same reasoning as ppus_recv_byte()/ppus_recv_byte_impl on the PPU
// side (see ppus_recv.c): asm-named so the trampoline can call it by
// an exact known symbol, __attribute__((used)) since its only real
// caller is inside that trampoline's own raw asm text.
static void ppuc_recv_byte(unsigned char b)
    asm("ppuc_recv_byte_impl") __attribute__((used));
static void ppuc_recv_byte(unsigned char b) {
  if (ppuc_recv_state == 0) {
    ppuc_recv_want = b;
    ppuc_recv_state = 1;
    return;
  }
  if (ppuc_recv_state == 1) {
    ppuc_recv_want |= (unsigned int)b << 8;
    ppuc_recv_got = 0;
    ppuc_recv_state = 2;
    if (ppuc_recv_want == 0) {
      // A zero-length message completes right here -- there's no
      // payload byte left to wait for.
      ppuc_recv_callback(ppuc_recv_buf, 0);
      ppuc_recv_state = 0;
    }
    return;
  }

  // Never write past ppuc_recv_buf's fixed capacity, but keep counting
  // ppuc_recv_got up to the full announced length regardless -- keeps
  // this byte-for-byte in sync with what ppus_send() is actually
  // putting on the wire, same reasoning as ppus_recv_byte_impl's own
  // comment.
  if (ppuc_recv_got < PPUC_RECV_MAX) {
    ppuc_recv_buf[ppuc_recv_got] = b;
  }
  ppuc_recv_got++;
  if (ppuc_recv_got >= ppuc_recv_want) {
    unsigned int n = ppuc_recv_want;

    if (n > PPUC_RECV_MAX) {
      n = PPUC_RECV_MAX;
    }
    ppuc_recv_callback(ppuc_recv_buf, n);
    ppuc_recv_state = 0;
  }
}

// The actual interrupt entry point (vector 0460 -- see
// ppuc_recv_init() below). Hand-written for the same reason
// ppus_recv_isr is (see ppus_recv.c): needs `rti`, not a plain C
// function's `jsr pc`/`rts pc`, and saves every general register
// unconditionally since an interrupt can land mid-function anywhere
// in this program, not just at a compiler-planned call boundary.
asm(
    "ppuc_recv_isr:\n\t"
    "mov r0, -(sp)\n\t"
    "mov r1, -(sp)\n\t"
    "mov r2, -(sp)\n\t"
    "mov r3, -(sp)\n\t"
    "mov r4, -(sp)\n\t"
    "mov r5, -(sp)\n\t"
    "movb @$0176662, r0\n\t"  // read the incoming byte -- also the
                              // hardware's own ack, per ChanReadByCPU()
    "bic $0177400, r0\n\t"    // MOVB sign-extends into the high byte;
                              // clear it so r0 is a plain 0..377 value
    "mov r0, -(sp)\n\t"       // push it as ppuc_recv_byte_impl's one
                              // argument
    "jsr pc, ppuc_recv_byte_impl\n\t"
    "add $2, sp\n\t"
    "mov (sp)+, r5\n\t"
    "mov (sp)+, r4\n\t"
    "mov (sp)+, r3\n\t"
    "mov (sp)+, r2\n\t"
    "mov (sp)+, r1\n\t"
    "mov (sp)+, r0\n\t"
    "rti\n");

// Arms the receiver: installs ppuc_recv_isr at vector 0460 (both
// words -- PC and PSW) and enables channel 1's RX interrupt. The
// vector's own saved PSW is 0200 (bit 7 only), same reasoning as
// ppus_recv_init()'s own choice (see its comment): masks further
// VIRQ/EVNT dispatch for this ISR's own short critical section
// without fully disabling preemption from its very first instruction.
//
// The whole install-vector/drain/enable sequence runs masked (mtps
// $340) until fully done, same reasoning as ppus_recv_init(): without
// it, priority here could already be 0 (unmasked) the whole time,
// letting the enable bit's own immediate-fire path
// (ChanRxStateSetCPU, mirroring ChanRxStateSetPPU) dispatch
// ppuc_recv_isr asynchronously mid-setup.
//
// The drain (reading PPU_CHAN1_DATA once before enabling) guards
// against a stale byte already sitting in the receive latch -- same
// hazard ppus_recv_init() guards against on its own channel, though
// here the likelier source isn't a resident-monitor receiver (RT-11
// itself doesn't use channel 1 for anything) but ppus_recv_init()'s
// own one-shot handshake byte, if the CPU side's first ppuc_send()
// call (which normally consumes it -- see ppuc_recv()'s own
// documented precondition in ppu_client.h) hasn't happened yet.
void ppuc_recv_init(ppuc_receive_fn callback) {
  ppuc_recv_callback = callback;

  asm volatile(
      "mtps $0340\n\t"
      "mov @$0460, %0\n\t"
      "mov @$0462, %1\n\t"
      "mov $ppuc_recv_isr, @$0460\n\t"
      "mov $0200, @$0462\n\t"
      "movb @$0176662, r0\n\t"
      "bisb $0100, @$0176660\n\t"
      "mtps $0\n\t"
      : "=r"(ppuc_recv_saved_vec_pc), "=r"(ppuc_recv_saved_vec_psw)
      :
      : "r0", "cc", "memory");
}

// Puts vector 0460/0462 back to whatever ppuc_recv_init() found there
// and disables channel 1's RX interrupt again. Call this before this
// program exits -- see ppuc_recv_shutdown()'s own comment in
// ppu_client.h for why leaving the vector armed is a real hazard, not
// just untidy: RT-11 doesn't clear memory between programs, so vector
// 0460 pointing at this program's own (soon overwritten) ppuc_recv_isr
// would misdirect a later, unrelated program's channel-1 interrupt
// into whatever happens to be there by then.
void ppuc_recv_shutdown(void) {
  asm volatile(
      "mtps $0340\n\t"
      "bic $0100, @$0176660\n\t"
      "mov %0, @$0460\n\t"
      "mov %1, @$0462\n\t"
      "mtps $0\n\t" ::
          "r"(ppuc_recv_saved_vec_pc),
          "r"(ppuc_recv_saved_vec_psw)
      : "cc", "memory");
}
