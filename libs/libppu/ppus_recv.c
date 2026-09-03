// ppus_recv.c -- interrupt-driven receiver for ppuc_send() (see
// ppuc_send.c/ppu_client.h)
//
// Opt-in, unlike ppus_start.c/ppus_exit(): this file is a normal
// libppu.a archive member, not forced in with `-u start` -- it's only
// linked into a PPU program's module at all if that program actually
// calls ppus_recv_init(), which is also what a program must do (from
// ppu_main(), before it can expect any command to arrive) to arm the
// receiver in the first place, passing it the callback (see
// ppus_receive_fn in ppu_server.h) to invoke for each buffer that
// arrives afterward.
//
// Hardware background, confirmed directly against ukncbtl's own
// faithful emulator model (emulator/emubase/Board.cpp/Memory.cpp),
// since nothing about this side of the channel was previously
// documented anywhere in this project: PPU_CSR/PPU_DATA (176674/
// 176676, see ppuc_internal.h) are the CPU's own view of "channel 2",
// the CPU<->PPU exchange channel ppuc_request() already uses -- the
// PPU has its own, differently-addressed view of that same channel:
// 0177064 is channel 2's data register (what ppuc_send() shows up in,
// one byte at a time), and bit 2 of 0177066 is that channel's own
// RX-interrupt-enable bit (bit 5 of the same register mirrors its
// ready/pending state, for a polling design instead -- not used
// here). The vector for "the CPU just wrote a byte on channel 2" is a
// fixed 0340 (traced directly through
// CMotherboard::ChanWriteByCPU/ChanRxStateSetPPU's own
// InterruptVIRQ(9, 0340) call) -- the PDP-11's usual [PC,PSW] pair,
// same convention as the keyboard's own vector (used by, among
// others, a real shipped UKNC game -- blairecas/descent's
// descnt_ppu.mac -- to install its own keyboard handler at 0300).

#include "ppu_server.h"

// Set once by ppus_recv_init(), then invoked from ppus_recv_byte()
// below for every buffer that arrives. NULL until ppus_recv_init()
// runs -- but nothing can call in here before then anyway, since
// installing ppus_recv_isr over vector 0340 is itself part of what
// ppus_recv_init() does.
static ppus_receive_fn ppus_recv_callback;

// The PPU-resident monitor's own vector 0340 contents, saved by
// ppus_recv_init() before overwriting them -- see ppus_recv_shutdown()
// for why putting them back matters.
static unsigned short ppus_recv_saved_vec_pc;
static unsigned short ppus_recv_saved_vec_psw;

static unsigned char ppus_recv_buf[PPUS_RECV_MAX];
static unsigned int ppus_recv_want;   // total payload length the sender announced
static unsigned int ppus_recv_got;    // payload bytes received so far
static unsigned char ppus_recv_state; // 0=length lo, 1=length hi, 2=payload

// TEMP DIAGNOSTIC: non-static so its final address shows up directly
// in the link map (no relocation-offset arithmetic needed to find
// it). Incremented once per ISR invocation.
unsigned int ppus_recv_isr_hit_count;
unsigned char ppus_recv_isr_raw[32];

// Called once per incoming byte, from the raw asm trampoline below --
// asm-named so that trampoline can call it by an exact, known symbol
// regardless of this function's own (irrelevant here) linkage.
// __attribute__((used)) keeps it from being optimized away as
// unreachable: its only real caller is inside that trampoline's own
// raw asm text, which GCC's own call-graph analysis can't see.
static void ppus_recv_byte(unsigned char b)
    asm("ppus_recv_byte_impl") __attribute__((used));
static void ppus_recv_byte(unsigned char b) {
  if (ppus_recv_isr_hit_count < sizeof(ppus_recv_isr_raw)) {  // TEMP DIAGNOSTIC
    ppus_recv_isr_raw[ppus_recv_isr_hit_count] = b;
  }
  ppus_recv_isr_hit_count++;  // TEMP DIAGNOSTIC
  if (ppus_recv_state == 0) {
    ppus_recv_want = b;
    ppus_recv_state = 1;
    return;
  }
  if (ppus_recv_state == 1) {
    ppus_recv_want |= (unsigned int)b << 8;
    ppus_recv_got = 0;
    ppus_recv_state = 2;
    if (ppus_recv_want == 0) {
      // A zero-length message completes right here -- there's no
      // payload byte left to wait for.
      ppus_recv_callback(ppus_recv_buf, 0);
      ppus_recv_state = 0;
    }
    return;
  }

  // Never write past ppus_recv_buf's fixed capacity, but keep counting
  // ppus_recv_got up to the full announced length regardless -- that's
  // what keeps this byte-for-byte in sync with what ppuc_send() is
  // actually putting on the wire, so a message bigger than
  // PPUS_RECV_MAX degrades to a silently truncated payload instead of
  // desynchronizing the framing for whatever message comes next.
  if (ppus_recv_got < PPUS_RECV_MAX) {
    ppus_recv_buf[ppus_recv_got] = b;
  }
  ppus_recv_got++;
  if (ppus_recv_got >= ppus_recv_want) {
    unsigned int n = ppus_recv_want;

    if (n > PPUS_RECV_MAX) {
      n = PPUS_RECV_MAX;
    }
    ppus_recv_callback(ppus_recv_buf, n);
    ppus_recv_state = 0;
  }
}

// The actual interrupt entry point (vector 0340 -- see ppus_recv_init()
// below). Hand-written, not a plain C function: this backend has no
// `interrupt`-style attribute to generate the right prologue/epilogue,
// and an ordinary `call ppus_recv_byte_impl` from C-compiled code
// would use a normal `jsr pc, ...`/`rts pc` pair, not the `rti` this
// needs to correctly restore the interrupted PSW (priority included)
// along with the PC.
//
// Saves every general register unconditionally, not just whatever
// ppus_recv_byte_impl()/the ppus_recv_init() callback happen to
// clobber under the ordinary C calling convention: unlike a normal
// call, an interrupt
// can land in the middle of ppu_main()'s own code at a point the
// compiler never planned for a call boundary at all, so anything left
// un-saved here would corrupt live state in whatever this interrupted
// -- the C ABI's own caller/callee-saved split doesn't apply to an
// asynchronous interrupt the way it does to a real call.
asm(
    "ppus_recv_isr:\n\t"
    "mov r0, -(sp)\n\t"
    "mov r1, -(sp)\n\t"
    "mov r2, -(sp)\n\t"
    "mov r3, -(sp)\n\t"
    "mov r4, -(sp)\n\t"
    "mov r5, -(sp)\n\t"
    "movb @$0177064, r0\n\t"  // read the incoming byte -- also the
                              // hardware's own ack, per ChanReadByPPU()
    "bic $0177400, r0\n\t"    // MOVB sign-extends into the high byte;
                              // clear it so r0 is a plain 0..377 value
    "mov r0, -(sp)\n\t"       // push it as ppus_recv_byte_impl's one
                              // argument (this target's normal C
                              // calling convention: args pushed right
                              // before jsr, caller cleans up after)
    "jsr pc, ppus_recv_byte_impl\n\t"
    "add $2, sp\n\t"
    "mov (sp)+, r5\n\t"
    "mov (sp)+, r4\n\t"
    "mov (sp)+, r3\n\t"
    "mov (sp)+, r2\n\t"
    "mov (sp)+, r1\n\t"
    "mov (sp)+, r0\n\t"
    "rti\n");

// Arms the receiver: installs ppus_recv_isr at vector 0340 (both
// words -- PC and PSW) and enables channel 2's RX interrupt. Also
// unmasks interrupts on the PPU itself (`mtps $0`) -- nothing before
// this point in a PPU program's life (ppus_start.c's shim included)
// has ever needed interrupts enabled, so nothing has ever lowered the
// priority this program actually started at.
//
// The vector's own saved PSW is set to 0200 (bit 7 only -- confirmed
// against ukncbtl's own dispatch loop, Processor.cpp's
// InterruptProcessing(), that this single bit is what gates *both*
// VIRQ and EVNT dispatch), not cleared to 0 as an earlier version of
// this file did (matching descnt_ppu.mac's own choice for its keyboard
// vector). With the vector's PSW at 0, this ISR would be fully
// preemptable from its very first instruction, including by the PPU's
// own periodic hardware timer interrupt (vector 0304, see
// Board.cpp:CMotherboard::AddTimerTick) -- setting bit 7 masks further
// VIRQ/EVNT dispatch (the timer included) for this ISR's own short
// critical section; `rti` restores whatever PSW was interrupted once
// it's done, so nothing stays masked longer than that. A real
// hardening, not just theoretical: confirmed via ukncbtl-debugger
// instrumentation that this ISR's vector can otherwise be re-entered
// before its first instruction runs. (The reliable hang on any message
// needing 5+ total interrupts, initially suspected to be this same
// preemption issue, turned out to have a different, unrelated cause --
// see ppuc_send.c's header comment and the channel-1 handshake below.)
//
// 0177066's bits 0/1/2 are channel 0/1/2's own RX-interrupt-enable,
// all three packed into the one register -- and channel 0 is the
// console/keyboard channel RT-11 itself depends on (confirmed the
// hard way: a first version of this that assigned the register
// outright, `movb $4, @$0177066`, silently killed the CPU's own
// console output the moment this ran, by clearing channel 0's
// already-set enable bit along with setting channel 2's). BISB reads
// the register first, so channel 0 and 1's current bits survive
// untouched -- only channel 2's bit actually changes.
//
// Call this once from ppu_main(), before doing anything that would
// race with a command arriving -- there is no other setup a program
// needs to do to start receiving.
//
// The final two instructions are the other half of the handshake
// documented in ppuc_send.c's header comment: vector 0340 is not
// ours to take over freely -- the PPU-resident monitor (the same one
// ppuc_request()/ppuc_run() talk to) is still listening there when
// this function starts, and stays listening for some number of
// instructions *after* ppuc_run() has already returned to the CPU
// caller, since ppuc_run()'s own request only tells the monitor to
// jump to this program's start(), not to wait until this exact point
// is reached. Confirmed directly (ukncbtl-debugger instrumentation):
// the monitor's own resident channel-2 receiver has a small fixed
// buffer that correctly consumes the first few bytes of anything
// ppuc_send() writes during that window, then mishandles the next one
// once that buffer fills -- which is exactly why a payload needing 5+
// total interrupts reliably hung before this fix, regardless of the
// PSW-masking hardening above: the failing byte was never reaching
// *this* program's receiver at all.
//
// So: once the two instructions above have actually installed
// ppus_recv_isr over vector 0340 and armed channel 2's interrupt,
// send one byte over channel 1 -- a separate CPU<->PPU sub-channel,
// chosen so this can never be confused with channel 2 traffic or with
// channel 0 (console/keyboard) -- to tell the CPU side it is finally
// safe to send real data. ppuc_send()'s first call after ppuc_run()
// waits for exactly this byte before doing anything else. 0177076's
// bit 4 (020) is channel 1's own PPU-side TX-ready bit (see
// Board.cpp's ChanTxStateGetPPU/ChanWriteByPPU); 0177072 is where the
// PPU writes a channel-1 TX byte.
void ppus_recv_init(ppus_receive_fn callback) {
  // Must be set before the asm below arms channel 2's interrupt --
  // this program's priority is already 0 at this point (nothing
  // earlier in its life has ever raised it), so a byte already
  // pending on channel 2 could fire ppus_recv_isr the instant that
  // interrupt is enabled, and it calls through ppus_recv_callback
  // unconditionally.
  ppus_recv_callback = callback;

  // A genuine, confirmed hazard, not just the theoretical one above:
  // ppuc_load_code()'s own ppuc_request() calls (ALLOC/WRITE/RUN, all
  // still handled by the PPU-resident monitor at this point) use this
  // exact same channel 2 -- and the monitor doesn't always leave it
  // drained once it hands off to this program. Confirmed directly
  // (ukncbtl-debugger instrumentation, a hit counter on
  // ppus_recv_byte_impl/on_receive): without this drain, arming the
  // interrupt below (Board.cpp's ChanRxStateSetPPU) finds a stale
  // byte already sitting in the receive latch from the monitor's own
  // traffic and fires ppus_recv_isr for it immediately -- garbage fed
  // into the length-prefix state machine, desynchronizing every real
  // byte that follows (observed as multiple garbled completions from
  // what should have been a single clean message). Reading the data
  // register unconditionally, before enabling the interrupt, always
  // clears the hardware's own "ready" latch (see Board.cpp's
  // ChanReadByPPU) -- harmless if nothing was actually pending.
  //
  // The whole install-vector/drain/enable sequence now runs masked
  // (mtps $340, priority 7) until it's fully done, matching a
  // known-working reference implementation of this same interrupt
  // style for a different channel: without this, priority here is
  // already 0 (unmasked) the entire time, so the enable bit's own
  // immediate-fire path (ChanRxStateSetPPU, same as the drain comment
  // above) can dispatch ppus_recv_isr asynchronously mid-setup, before
  // ppus_recv_init() has even reached its own return -- rather than
  // staying pending and being picked up cleanly, in order, only once
  // priority actually drops back to 0 below.
  asm volatile(
      "mtps $0340\n\t"
      "mov @$0340, %0\n\t"
      "mov @$0342, %1\n\t"
      "mov $ppus_recv_isr, @$0340\n\t"
      "mov $0200, @$0342\n\t"
      "movb @$0177064, r0\n\t"
      "bisb $4, @$0177066\n\t"
      "mtps $0\n\t"
      "1:\n\t"
      "bitb $020, @$0177076\n\t"
      "beq 1b\n\t"
      "movb $1, @$0177072\n\t"
      : "=r"(ppus_recv_saved_vec_pc), "=r"(ppus_recv_saved_vec_psw)
      :
      : "r0", "cc", "memory");
}

// Puts vector 0340/0342 back to what ppus_recv_init() found there
// (the PPU-resident monitor's own handler) and disables channel 2's
// RX interrupt again (undoing ppus_recv_init()'s own bisb -- BIC only
// touches bit 2, leaving channels 0/1's enable bits exactly as they
// are, same reasoning as that bisb's own comment). Must be called
// before returning from ppu_main() -- i.e. before ppus_start.c's shim
// runs ppus_exit() -- because ppus_exit() frees this program's own
// PPU memory block, and vector 0340 would otherwise be left pointing
// at ppus_recv_isr, which lives inside that block: any channel-2
// interrupt after that (e.g. from the resident monitor's own use of
// ppuc_request() on behalf of a later ppuc_load_code()/ppuc_alloc()
// call) would jump into memory no longer guaranteed to hold that
// code, corrupting whatever's actually there now. Confirmed directly:
// omitting this call reproduces a monitor halt (RT-11's own "*** СТОП
// ***") shortly after a well-behaved ppuc_send()/ppuc_recv() exchange
// completes and the CPU side reaches its own .EXIT.
void ppus_recv_shutdown(void) {
  asm volatile(
      "mtps $0340\n\t"
      "bic $4, @$0177066\n\t"
      "mov %0, @$0340\n\t"
      "mov %1, @$0342\n\t"
      "mtps $0\n\t" ::
          "r"(ppus_recv_saved_vec_pc),
          "r"(ppus_recv_saved_vec_psw)
      : "cc", "memory");
}
