// ppupongppu.c -- PPU-side payload for the ppupong example
//
// Demonstrates the CPU->PPU and PPU->CPU data channels together
// (ppus_recv_init()/ppus_send(), see libs/libppu/ppu_server.h): waits
// for a "ping <N>" string from the CPU (ppupong.c) and sends back
// "pong <N>" with the same suffix.
//
// Deliberately does no number parsing/formatting at all: "ping " and
// "pong " are the same length (5 bytes), so everything after "ping "
// -- whatever it looks like, not necessarily digits -- is copied
// through unchanged as the suffix of "pong ". All the actual counting
// (parsing N, incrementing it, formatting the next ping) lives on the
// CPU side in ppupong.c, which has no reason not to use ordinary
// variable-width decimal.
//
// Every message carries an explicit one-byte type ahead of its
// payload (MSG_TYPE_DATA/MSG_TYPE_EXIT below) -- not just a "suffix
// looks like a sentinel value" convention, since that would collide
// with a real suffix that happened to look the same way. MSG_TYPE_EXIT
// is how ppupong.c tells this program to stop, once its own counting
// loop is done, and return control to the PPU-resident monitor:
// without an explicit signal, this side would have no way to know the
// CPU side is finished, and would sit in ppu_main()'s loop forever
// waiting for a message that will never arrive -- never reaching
// ppus_start.c's automatic ppus_exit() call, so the resident monitor
// never gets its own channel-2 receiver back, which real UKNC console
// I/O (EMT-based TTY output on the CPU side) turns out to depend on
// for its own character-ready signaling.

#include "ppu_server.h"

// Message type byte, sent as the first byte of every ppuc_send()/
// ppus_send() payload.
#define MSG_TYPE_DATA 1
#define MSG_TYPE_EXIT 2

// Prefix length shared by "ping " and "pong " -- both 5 bytes -- not
// counting the type byte.
#define PREFIX_LEN 5

// Longest suffix this ever needs to carry: ppupong.c counts to 100,
// i.e. at most 3 digits -- kept tight (not PPUS_RECV_MAX's own full
// 64) since every byte here adds to ppuc_load_code()'s real ceiling,
// see that function's own header comment.
#define SUFFIX_MAX 4

// Set by on_receive() (called from the channel-2 interrupt handler --
// see ppus_recv_init()), read back out by ppu_main()'s own loop below,
// well outside interrupt context. 0xffff is never a valid length (a
// real suffix here is always well under that), so it doubles as "no
// message waiting yet".
static volatile unsigned int got_len = 0xffff;
static unsigned char got_suffix[SUFFIX_MAX];

// Set by on_receive() on a MSG_TYPE_EXIT message -- see the file
// header comment for why ppu_main()'s loop needs this at all.
static volatile unsigned char exit_requested = 0;

// Diagnostic: counts every MSG_TYPE_DATA message actually processed.
// Non-static so its address shows up directly in the link map (no
// relocation-offset arithmetic needed to find it in the debugger).
unsigned int ping_count;

static void on_receive(const void *buf, unsigned int size) {
  const unsigned char *p = (const unsigned char *)buf;
  unsigned int i, n;

  if (size < 1) {
    return;
  }
  if (p[0] == MSG_TYPE_EXIT) {
    exit_requested = 1;
    return;
  }

  ping_count++;

  // MSG_TYPE_DATA: whatever comes after the type byte and the fixed
  // "ping " prefix is the suffix -- no need to check the prefix
  // itself, ppupong.c is the only thing that ever calls ppuc_send()
  // for this program.
  n = (size > 1 + PREFIX_LEN) ? size - 1 - PREFIX_LEN : 0;
  if (n > sizeof(got_suffix)) {
    n = sizeof(got_suffix);
  }
  for (i = 0; i < n; i++) {
    got_suffix[i] = p[1 + PREFIX_LEN + i];
  }
  got_len = n;
}

void ppu_main(void) {
  unsigned char reply[1 + PREFIX_LEN + SUFFIX_MAX];

  ppus_recv_init(on_receive);
  // No fixed iteration count here on purpose, unlike ppupong.c's own
  // loop: if this side also stopped after some fixed number of
  // messages, it would race ppupong.c's own MSG_TYPE_EXIT send --
  // confirmed directly, this is exactly what an earlier version of
  // this loop (bounded the same way, i<10) did. Whichever exchange
  // happens to be in flight when this side's own count runs out would
  // get torn down by ppus_recv_shutdown() mid-message (its first byte
  // already consumed by ppus_recv_isr, the rest never arriving,
  // since shutdown disables the interrupt and restores the vector
  // unconditionally). MSG_TYPE_EXIT is the only thing that should ever
  // end this loop.
  while (!exit_requested) {
    unsigned int n, j;

    while (got_len == 0xffff && !exit_requested) {
    }
    if (exit_requested) {
      break;
    }
    n = got_len;
    got_len = 0xffff;

    reply[0] = MSG_TYPE_DATA;
    reply[1] = 'p';
    reply[2] = 'o';
    reply[3] = 'n';
    reply[4] = 'g';
    reply[5] = ' ';
    for (j = 0; j < n; j++) {
      reply[1 + PREFIX_LEN + j] = got_suffix[j];
    }
    ppus_send(reply, 1 + PREFIX_LEN + n);
  }

  // Must happen before returning -- ppus_start.c's shim runs
  // ppus_exit() right after, which frees this program's own PPU
  // memory block; see ppus_recv_shutdown()'s own comment for why
  // leaving vector 0340 pointing into it is a real, confirmed hazard.
  ppus_recv_shutdown();
}
