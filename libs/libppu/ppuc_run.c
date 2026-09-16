// ppuc_run.c -- start code running on the PPU
//
// Failure is reported by the return value alone, deliberately: touching
// errno here would cost a program some 7 K it may have no other use for.
// On this newlib, errno resolves through _impure_ptr, and the object
// defining it initialises the reentrancy struct with pointers to the
// three standard FILEs -- which drags in the whole of stdio, and behind
// it malloc, sbrk and the RT-11 read/write/close/lseek layer. Measured
// on Digger, whose only other use for any of it had just been removed
// (2026-09-09). errno stays where a program is paying for the C library
// anyway: the file loaders and ppuc_alloc().

#include "ppu_client.h"
#include "ppuc_internal.h"

// Where the code that is running on the PPU was loaded, kept for the
// debugger's sake -- see ppu_client.h. Two bytes of .bss and one store
// below; nothing here reads it.
unsigned short ppuc_code_base;

int ppuc_run(unsigned short ppu_addr) {
  struct ppu_desc desc;

  desc.stat = 0;
  desc.func = PPU_F_RUN;
  desc.dev = PPU_DEV;
  desc.addr = ppu_addr;
  if (!ppuc_request(&desc)) {
    return -1;
  }
  // A fresh program run means ppuc_send()'s next call (if any) must
  // wait for a fresh ppus_recv_init() handshake -- see ppuc_send.c and
  // ppuc_internal.h.
  ppuc_send_need_handshake = 1;
  ppuc_code_base = ppu_addr;
  return 0;
}
