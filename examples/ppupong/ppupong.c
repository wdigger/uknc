// ppupong.c -- CPU-side driver for the ppupong example
//
// Demonstrates both CPU<->PPU data channels from libppu at once (see
// libs/libppu/ppu_client.h/ppu_server.h), each one interrupt-driven on
// both ends: loads and starts ppupongppu.c on the PPU
// (ppuc_load_code()/ppuc_run(), same as ../ppurun), then runs 10
// rounds of "ping N" / "pong N" -- ppuc_send() carries each ping to
// the PPU (received there by ppus_recv_init()'s callback, see
// ppupongppu.c), ppuc_recv_init()'s own callback (on_pong() below)
// picks up the matching pong. The PPU side does no number parsing at
// all -- it just echoes whatever follows "ping " as the suffix of
// "pong " -- so all of the actual counting (extracting N,
// incrementing it, formatting the next ping) happens here, in
// ordinary variable-width decimal ("ping 1", not a zero-padded
// "ping 001"), the same as it would on any platform. No console
// output during the exchange itself -- see the comment further down,
// by ppuc_run() -- so this is verified by the final "back from PPU"
// message actually appearing, not by watching each round go by.
//
// Every ppuc_send()/ppus_send() payload carries an explicit one-byte
// type ahead of its string (MSG_TYPE_DATA/MSG_TYPE_EXIT below): once
// the counting loop finishes, this sends one MSG_TYPE_EXIT message so
// the PPU side returns control to its own resident monitor, rather
// than sit waiting for an eleventh message that will never come --
// see MSG_TYPE_EXIT's own comment for why that matters beyond just
// tidiness.
//
// Console output goes through write() rather than printf()/puts():
// ppuc_load_code() does real file I/O (open/read/close) to pull the
// .PPU file off disk, and mixing that with buffered stdio has a
// known, unresolved GCC pdp11-backend codegen bug on this toolchain
// (see the project README's newlib section) -- same reasoning as
// ppurun.c, which this is modeled on. Rolls its own tiny decimal
// formatter/parser (build_ping()/parse_num()) instead of
// sprintf()/atoi() for the same reason.

#include <errno.h>
#include <unistd.h>

#include "ppu_client.h"

// Message type byte, sent as the first byte of every ppuc_send()/
// ppus_send() payload -- must match ppupongppu.c's own definitions.
// MSG_TYPE_EXIT tells the PPU side to stop and return control to the
// PPU-resident monitor once this program's own counting loop is done:
// without it, the PPU side has no way to know the exchange is over,
// so it never returns from ppu_main() (never reaching ppus_start.c's
// automatic ppus_exit() call) -- and the resident monitor never gets
// its own channel-2 receiver back, which real UKNC console I/O
// (EMT-based TTY output, right here on the CPU side) turns out to
// depend on for its own character-ready signaling.
#define MSG_TYPE_DATA 1
#define MSG_TYPE_EXIT 2

#define MSG_MAX 16

static void msgln(const char *s) {
  unsigned int len = 0;

  while (s[len] != 0) {
    len++;
  }
  write(STDOUT_FILENO, s, len);
  write(STDOUT_FILENO, "\r\n", 2);
}

// Diagnostic-only: prints prefix followed by v in decimal. Not used by
// build_ping()/the wire protocol -- just for narrowing down where this
// program gets stuck when it does, so plain %/ is fine here regardless
// of libgcc divide-routine cost.
static void msgnum(const char *prefix, unsigned int v) {
  char digits[6];
  unsigned int n = 0, i, len = 0;

  while (prefix[len] != 0) {
    len++;
  }
  write(STDOUT_FILENO, prefix, len);

  if (v == 0) {
    digits[n++] = '0';
  } else {
    while (v > 0) {
      digits[n++] = (char)('0' + (v % 10));
      v /= 10;
    }
  }
  for (i = 0; i < n; i++) {
    write(STDOUT_FILENO, &digits[n - 1 - i], 1);
  }
  write(STDOUT_FILENO, "\r\n", 2);
}

// Writes "ping " followed by num in decimal, NUL-terminated (this
// side treats its own buffer as a C string for msg(); the wire itself
// only ever sees the byte count ppuc_send() is given, not the NUL).
// Returns the byte count, not counting the NUL.
//
// Hand-rolled, not snprintf(): pulling in newlib's stdio here hits a
// real, already-documented GCC pdp11-backend codegen bug when a
// program also does real file I/O (ppuc_load_code()'s open/read/close)
// -- confirmed directly, it crashes with a monitor trap during the
// very next file read after linking in printf's machinery. Ordinary
// num/10 and num%10 below, though: no reason to avoid the libgcc
// software-divide routine they pull in now that a CPU program's own
// size no longer caps how big a PPU program ppuc_load_code() can load
// (see that function's own header comment) -- crt0rt.s's .SETTOP call
// gives every program the monitor's real available memory instead of
// a conservative link-time guess, leaving far more headroom than
// either side of this example needs.
//
// Writes the type byte plus "ping " plus num in decimal, NUL-terminated
// after the string part (out+1) for msgln()'s sake. Returns the total
// byte count ppuc_send() should be given (type byte included), not
// counting the NUL.
static unsigned int build_ping(char *out, unsigned int num) {
  char digits[4];
  unsigned int len, n;

  out[0] = MSG_TYPE_DATA;
  out[1] = 'p';
  out[2] = 'i';
  out[3] = 'n';
  out[4] = 'g';
  out[5] = ' ';
  len = 6;

  n = 0;
  if (num == 0) {
    digits[n++] = '0';
  } else {
    while (num > 0) {
      digits[n++] = (char)('0' + (num % 10));
      num /= 10;
    }
  }
  while (n > 0) {
    out[len++] = digits[--n];
  }
  out[len] = 0;
  return len;
}

// Pulls the number out of a MSG_TYPE_DATA reply's "pong N" -- the type
// byte plus "pong " is always exactly 6 bytes (matches build_ping()'s
// own layout above), so this skips straight to index 6 instead of
// scanning for the space. size is how many bytes ppuc_recv() actually
// delivered into s (type byte included): bounding the scan to it
// (rather than relying on s being NUL-terminated) matters because a
// reply with size<=6 has no digits at all, and reading past byte size
// would walk into whatever this call's own stack frame happens to
// hold, not real received data.
static unsigned int parse_num(const char *s, unsigned int size) {
  unsigned int n = 0, i = 6;

  while (i < size && s[i] >= '0' && s[i] <= '9') {
    n = n * 10 + (unsigned int)(s[i] - '0');
    i++;
  }
  return n;
}

// Set by on_pong() (called from channel 1's interrupt handler -- see
// ppuc_recv_init()), read back out by main()'s own loop below, well
// outside interrupt context. 0xffff is never a valid length (a real
// reply here is always well under that), so it doubles as "no reply
// waiting yet" -- same convention ppupongppu.c's own got_len uses.
static volatile unsigned int cpu_got_len = 0xffff;
static char cpu_got_buf[MSG_MAX];

static void on_pong(const void *buf, unsigned int size) {
  const unsigned char *p = (const unsigned char *)buf;
  unsigned int i, n;

  n = size;
  if (n > sizeof(cpu_got_buf)) {
    n = sizeof(cpu_got_buf);
  }
  for (i = 0; i < n; i++) {
    cpu_got_buf[i] = (char)p[i];
  }
  cpu_got_len = n;
}

int main(void) {
  long ppu_addr;
  char sendbuf[MSG_MAX];
  unsigned int num, i, len, n;

  msgln("loading PONG.PPU...");
  ppu_addr = ppuc_load_code("PONG.PPU");
  if (ppu_addr < 0) {
    msgnum("load failed, errno=", (unsigned int)errno);
    return 1;
  }
  msgnum("loaded at ", (unsigned int)ppu_addr);

  // No console output from here on, deliberately: every write() after
  // ppuc_run() hands the PPU over to our own code has been observed to
  // block for a very long time (possibly forever) in the monitor's own
  // TTY-ready busy-wait at 0177560 -- apparently fed by whatever the
  // PPU-resident monitor was doing before we replaced it. Silencing
  // these calls isolates whether the actual ping/pong exchange
  // completes on its own, independent of that separate, unresolved
  // console-output problem.
  if (ppuc_run((unsigned short)ppu_addr) < 0) {
    return 1;
  }

  num = 1;
  for (i = 0; i < 10; i++) {
    len = build_ping(sendbuf, num);
    ppuc_send(sendbuf, len);

    // Arm the interrupt-driven receiver only after this program's
    // first ppuc_send() call -- which is what consumes
    // ppus_recv_init()'s one-shot handshake byte over this same
    // channel 1 (see ppuc_recv_init()'s own precondition in
    // ppu_client.h). Arming any earlier would feed that handshake byte
    // into this receiver's own state machine as if it were the start
    // of a real message.
    if (i == 0) {
      ppuc_recv_init(on_pong);
    }

    while (cpu_got_len == 0xffff) {
    }
    n = cpu_got_len;
    cpu_got_len = 0xffff;

    num = parse_num(cpu_got_buf, n) + 1;
  }

  // Must happen before this program exits -- see ppuc_recv_shutdown()'s
  // own comment in ppu_client.h for why leaving channel 1's interrupt
  // armed is a real hazard, not just untidy.
  ppuc_recv_shutdown();

  // Tell the PPU side to stop and hand control back to its own
  // resident monitor -- see MSG_TYPE_EXIT's own comment above for why
  // this is needed at all, not just a fixed iteration count on both
  // sides.
  sendbuf[0] = MSG_TYPE_EXIT;
  ppuc_send(sendbuf, 1);

  // ppuc_send() only confirms the byte went out -- not that the PPU
  // has actually finished acting on it: ppu_main() still has to notice
  // exit_requested, return, and let ppus_start.c's shim run
  // ppus_exit() (free this program's PPU memory block, jump back to
  // the PPU-resident monitor at 0176300). Give that some real time to
  // happen before this side's own exit path (crt0rt.s's EMT 0350)
  // starts interacting with the monitor too -- a plain busy-wait, not
  // console I/O, since every console write after ppuc_run() has been
  // observed to itself get stuck (see the comment above).
  {
    volatile unsigned int delay;
    for (delay = 0; delay < 50000; delay++) {
    }
  }

  // Console output works again here (unlike everywhere between
  // ppuc_run() and this point -- see the comment above), confirming
  // the PPU really did return control to its own resident monitor:
  // that's exactly the state real UKNC console I/O turns out to
  // depend on for its own character-ready signaling.
  msgln("back from PPU");

  return 0;
}
