// ppuc_internal.h -- shared internals for libppu's CPU-side (client)
// code
//
// Not part of the public API (see ppu_client.h for that) -- this is
// what ppuc_proto.c/ppuc_alloc.c/ppuc_free.c/ppuc_load.c all need in
// common, split out one-function-per-file (like this project's own
// newlib rt11 syscalls port) so a program linking against libppu.a
// only pulls in whichever of ppuc_alloc()/ppuc_free()/ppuc_load() it
// actually calls, not all three.
//
// Based on Oleg Safiullin's PRUN utility for RT-11
// (pdp-11.org.ru/~form/files/pdp-11/uknc/prun/): same CPU<->PPU
// request descriptor layout and port protocol (PIPKT/PDESC/PPURQ in
// prun.mac), reimplemented in C.
//
// The CPU and PPU talk over a one-byte-wide port: PPU_CSR (176674) is
// a status/control register the CPU can read, PPU_DATA (176676) is
// where the CPU writes bytes to the PPU. Bit 7 of PPU_CSR means "PPU
// ready for the next byte".
//
// A request is NOT streamed byte by byte to the PPU: only the address
// of a "descriptor" (struct ppu_desc below) plus a -1 terminator word
// -- 4 bytes total -- are sent this way (see ppuc_proto.c's
// ppuc_request()). The PPU then reads the descriptor, and for a write
// request the data buffer it names, directly out of CPU memory itself
// (the two processors share the same physical RAM). Confirmed by
// tracing PRUN's own PPURQ: it loads R1 with 4+1 and uses SOB
// (decrement-then-branch-if-nonzero) to count the bytes sent, which
// actually sends 4 bytes, not 5 -- exactly sizeof(address) +
// sizeof(-1 as a word). That "+1" is a real, load-bearing extra
// readiness wait after the last byte, not a fencepost quirk -- PRUN
// polls for PPU readiness once more before checking desc->stat, and
// ppuc_request() below does the same.

#ifndef PPUC_INTERNAL_H
#define PPUC_INTERNAL_H

#define PPU_CSR (*(volatile unsigned char *)0176674)
#define PPU_DATA (*(volatile unsigned char *)0176676)

// Channel 1's CPU-side registers -- a separate CPU<->PPU sub-channel
// from channel 2 (PPU_CSR/PPU_DATA above) or channel 0 (the
// console/keyboard channel). Shared by ppuc_send.c (the one-shot
// "ppus_recv_init() has taken over vector 0340" handshake byte) and
// ppuc_recv.c (real data from the PPU's own ppus_send() -- see
// ppu_client.h) -- both read this same direction (PPU writes, CPU
// reads), never anything else.
#define PPU_CHAN1_CSR (*(volatile unsigned char *)0176660)
#define PPU_CHAN1_DATA (*(volatile unsigned char *)0176662)

#define PPU_DEV 032  // PPU device code, in ppu_desc.dev

// Function codes for struct ppu_desc.func (PRUN's PF.* constants).
#define PPU_F_ALLOC 01
#define PPU_F_DEALLOC 02
#define PPU_F_READ 010
#define PPU_F_WRITE 020
#define PPU_F_RUN 030

// The request descriptor -- field layout matches PRUN's PDESC. The PPU
// reads this directly out of CPU memory once told where it is, so the
// field order/sizes here are load-bearing, not just convention.
struct ppu_desc {
  unsigned char stat;   // [out] 0 on success, nonzero on PPU error
  unsigned char func;   // [in]  PPU_F_* above
  unsigned short dev;   // [in]  always PPU_DEV
  unsigned short addr;  // [in/out] PPU address
  unsigned short buff;  // [in]  CPU buffer address (or requested size,
                         //       in words, for PPU_F_ALLOC -- PRUN
                         //       reuses this field the same way)
  unsigned short wcnt;  // [in]  word count
};

// Sends the 4-byte handshake (address of desc, then a -1 terminator
// word) that tells the PPU where to find the descriptor, waiting for
// the PPU to be ready before each byte -- plus one more readiness wait
// after the last byte, matching PRUN's own PPURQ (its SOB loop counter
// starts at 4+1, not 4) before checking desc->stat. CPU priority is
// raised for the duration, like PRUN's own PPURQ: this isn't going
// through an EMT trap, so nothing else stops a keyboard/clock
// interrupt from landing mid-handshake. Returns nonzero (true) on
// success, zero if the PPU reported an error via desc->stat.
int ppuc_request(struct ppu_desc *desc);

// Copies size bytes from buf into PPU memory at ppu_addr, which the
// caller must already have gotten from a successful ppuc_alloc() --
// the PPU reads the buffer directly out of CPU memory once told where
// and how much, the same as PRUN's own PPU_F_WRITE (PF.WLB) call.
// Shared by ppuc_load.c and ppuc_load_reloc.c; not part of the public
// API (see ppu_client.h) since it needs an address ppuc_alloc()
// already handed out, not something a caller would reach for on its
// own. Returns nonzero (true) on success, zero on failure (caller maps
// to errno).
int ppuc_write_buf(unsigned short ppu_addr, const void *buf,
                    unsigned int size);

// Set to 1 by ppuc_run() every time it starts a PPU program, and
// cleared by ppuc_send()'s first call after that (see ppuc_send.c for
// the full story) -- makes the one-shot "wait for ppus_recv_init()'s
// channel-1 ready handshake" apply fresh after every ppuc_run(), not
// just once for the whole CPU program's lifetime.
extern unsigned char ppuc_send_need_handshake;

#endif  // PPUC_INTERNAL_H
