// ppus_send.c -- send a buffer from a running PPU program to the CPU
//
// The PPU-side counterpart to ppuc_recv() (see ppu_client.h/
// ppuc_recv.c): together they mirror ppuc_send()/ppus_recv_init() in
// the opposite direction, over channel 1 -- the same CPU<->PPU
// sub-channel ppus_recv_init() already uses for its one-shot
// "receiver armed" handshake byte (see ppus_recv.c), reused here for
// the data itself now that byte has done its job. Wire format is the
// same length-prefixed byte stream ppuc_send() uses in the other
// direction: a little-endian 16-bit byte count, then that many
// payload bytes, one byte at a time, each gated on channel 1's own
// PPU-side TX-ready bit (0177076 bit 4 -- confirmed against
// ukncbtl-debugger's Board.cpp: ChanTxStateGetPPU()'s bit 4 is
// m_chanpputx[1].ready, and 0177072 is where ChanWriteByPPU(1, ...)
// gets triggered from).
//
// No interrupt/state-machine plumbing needed here, unlike ppus_recv.c:
// this is an ordinary function called from ppu_main() (or from the
// ppus_recv_init() callback, though see ppu_server.h's own note there
// about keeping that callback short -- this blocks one byte at a
// time, same as ppuc_send() does on the CPU side), not something that
// itself needs to run from inside an interrupt handler.
//
// If a program also uses ppus_recv_init()/ppuc_send() (the CPU->PPU
// direction), the CPU side must make its first ppuc_send() call --
// which consumes the one-shot handshake byte ppus_recv_init() sends
// over this same channel -- before calling ppuc_recv() to read
// anything this function sends; see ppuc_recv.c's own header comment
// for why.

#include "ppu_server.h"

#define PPU_CHAN1_TX_CSR (*(volatile unsigned char *)0177076)
#define PPU_CHAN1_TX_DATA (*(volatile unsigned char *)0177072)

static void ppus_send_byte(unsigned char b) {
  while ((PPU_CHAN1_TX_CSR & 020) == 0) {
  }
  PPU_CHAN1_TX_DATA = b;
}

void ppus_send(const void *buf, unsigned int size) {
  const unsigned char *p = (const unsigned char *)buf;
  unsigned int i;

  ppus_send_byte((unsigned char)size);
  ppus_send_byte((unsigned char)(size >> 8));
  for (i = 0; i < size; i++) {
    ppus_send_byte(p[i]);
  }
}
