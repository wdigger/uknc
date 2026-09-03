// ppuc_recv.c -- receive a buffer sent by a running PPU program's
// ppus_send() (see ppu_server.h/ppus_send.c)
//
// The CPU-side counterpart: mirrors ppuc_send()/ppus_recv_init() in
// the opposite direction, over channel 1 -- see ppus_send.c's header
// comment for the wire format. Unlike ppuc_send(), this direction
// needs no startup handshake of its own: ppuc_send() has to wait out
// the PPU-resident monitor's own vector-0340 receiver, which is
// already listening on channel 2 before a PPU program even starts,
// but nothing pre-existing ever listens on channel 1 the way that
// monitor does on channel 2 -- so there's no equivalent race to guard
// against here.
//
// A program that also uses ppus_recv_init()/ppuc_send() (the CPU->PPU
// direction) must make its first ppuc_send() call -- which consumes
// the one-shot "receiver armed" handshake byte ppus_recv_init() sends
// over this same channel 1 -- before ever calling ppuc_recv();
// otherwise that handshake byte would be misread here as the first
// byte of a real message's length prefix, desyncing the framing. A
// program that never calls ppus_recv_init() at all has no such byte
// to worry about.
//
// Blocks until a full message arrives -- there is no non-blocking or
// timeout variant. size larger than maxsize is not an error: the
// first maxsize bytes land in buf, the rest is read and discarded (to
// keep the wire framing correct for whatever message comes next) but
// otherwise lost; the return value is how many bytes actually landed
// in buf, so a caller can tell whether that happened.

#include "ppu_client.h"
#include "ppuc_internal.h"

static unsigned char ppuc_recv_byte(void) {
  while ((PPU_CHAN1_CSR & 0200) == 0) {
  }
  return PPU_CHAN1_DATA;
}

unsigned int ppuc_recv(void *buf, unsigned int maxsize) {
  unsigned char *p = (unsigned char *)buf;
  unsigned int size, i, n;

  size = ppuc_recv_byte();
  size |= (unsigned int)ppuc_recv_byte() << 8;

  n = size;
  if (n > maxsize) {
    n = maxsize;
  }
  for (i = 0; i < size; i++) {
    unsigned char b = ppuc_recv_byte();
    if (i < n) {
      p[i] = b;
    }
  }
  return n;
}
