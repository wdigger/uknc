// ppuc_send.c -- send a buffer to an already-running PPU program
//
// ppuc_request() (see ppuc_internal.h/ppuc_proto.c) is the
// control-plane protocol: it talks to the PPU-resident monitor, which
// is what's listening on the CPU<->PPU channel (vector 0340) before a
// PPU program is loaded and started (ppuc_alloc/ppuc_load*/ppuc_run)
// -- and for some time *after* ppuc_run() too: ppuc_run()'s own
// PPU_F_RUN request only tells the monitor to jump to the program's
// start(), it does not wait for that program to reach ppus_recv_init()
// and install its own vector-0340 handler over the monitor's. Confirmed
// the hard way, by instrumenting ukncbtl-debugger's own emulator core:
// the monitor's resident channel-2 receiver (the same one
// ppuc_request() itself talks to) stayed installed and kept consuming
// the first few incoming bytes even after ppuc_run() returned, then
// mishandled the next one once its own small fixed buffer filled --
// which is exactly why a payload needing 5+ total interrupts reliably
// hung: the 4th-5th byte landed on the monitor's receiver, not ours,
// while our own ppus_recv_init() hadn't run yet.
//
// The fix is the handshake below: ppus_recv_init() (see ppus_recv.c),
// once it has actually installed ppus_recv_isr over vector 0340, sends
// one byte over channel 1 -- a separate CPU<->PPU sub-channel from
// channel 2/PPU_CSR/PPU_DATA above, so it can't be confused with
// anything channel 2 is doing -- and ppuc_send()'s first call waits
// for it before sending anything for real. This guarantees our own
// receiver, not the monitor's, is what's listening by the time any
// real byte goes out.
//
// ppuc_send() is the data-plane counterpart to ppuc_request(): it
// delivers an arbitrary buffer to a PPU program that has opted into
// receiving one, by calling ppus_recv_init() (see ppu_server.h) and
// defining its own ppus_receive(). The wire format is a plain
// length-prefixed byte stream -- a little-endian 16-bit byte count,
// then that many payload bytes -- sent one byte at a time over the
// same PPU_CSR/PPU_DATA handshake ppuc_request() already uses (see
// ppuc_proto.c): wait for PPU_CSR's ready bit, write the next byte to
// PPU_DATA, repeat. CPU priority is raised for the duration, same
// reasoning as ppuc_request(): nothing else should stall a handshake
// byte mid-send.
//
// size larger than PPUS_RECV_MAX (see ppu_server.h) is not rejected
// here -- the whole point is that the CPU side doesn't need to know
// the PPU program's own buffer capacity -- but ppus_receive() on the
// PPU side only ever sees the first PPUS_RECV_MAX bytes; the rest is
// silently discarded there (not here), so the wire framing for the
// *next* message stays correct either way.

#include "ppu_client.h"
#include "ppuc_internal.h"

static void ppuc_send_byte(unsigned char b) {
  while ((PPU_CSR & 0200) == 0) {
  }
  PPU_DATA = b;
}

// Cleared by ppuc_run() (see ppuc_run.c) every time it starts a PPU
// program, so a CPU program that calls ppuc_run() more than once (a
// fresh program, or the same one restarted) gets a fresh one-shot
// handshake wait each time -- not just the first ever ppuc_send() call
// for the process's whole lifetime.
unsigned char ppuc_send_need_handshake = 1;

void ppuc_send(const void *buf, unsigned int size) {
  const unsigned char *p = (const unsigned char *)buf;
  unsigned int i;

  // One-shot handshake: block until ppus_recv_init() (on the PPU,
  // possibly still mid-startup) confirms over channel 1 that it has
  // actually installed its own receiver over vector 0340 -- see the
  // header comment above for why this is needed at all. Only the
  // first call to ppuc_send() after each ppuc_run() needs to wait for
  // this; ppus_recv_init() only ever sends the one byte.
  if (ppuc_send_need_handshake) {
    while ((PPU_CHAN1_CSR & 0200) == 0) {
    }
    (void)PPU_CHAN1_DATA;
    ppuc_send_need_handshake = 0;
  }

  asm volatile("mtps $0340" ::: "cc");
  ppuc_send_byte((unsigned char)size);
  ppuc_send_byte((unsigned char)(size >> 8));
  for (i = 0; i < size; i++) {
    ppuc_send_byte(p[i]);
  }
  asm volatile("mtps $0" ::: "cc");
}
