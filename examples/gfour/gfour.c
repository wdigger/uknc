// gfour.c -- animates 3 colored squares bouncing around a black
// 320x288 screen until any key is pressed (or NUM_FRAMES elapses with
// none, as a safety cap for an unattended run), then exits.
//
// The first real graphics output in this project: the video
// generator's plane 0 (tag list, palette, horizontal scale) is
// entirely PPU-owned (see gfourppu.c). Pixel data itself lives in
// RT-11's own default console screen memory (plane offset 0100000,
// shared by planes 0/1/2 -- see gfourppu.c's own header comment for
// how that address was found and why it's safe to take over), which
// is outside the CPU's normal address space -- reached here through
// this side's own address/data window ports (0176640/0176642) rather
// than a plain pointer, exactly like gfourppu.c does from its own
// side.
//
// Keypresses reach this side via libppu's own channel-1 PPU->CPU
// messaging (ppuc_recv_init(), see ppu_client.h) instead of any kind
// of polling: gfourppu.c's own keyboard interrupt handler (see its
// header comment) forwards raw scancodes as they happen, and
// kbd_recv() below just watches for one that looks like a press. This
// replaces an earlier version of this file that instead polled
// channel 0's CPU-side status/data registers (0177560/0177562) --
// the same ones EMT 0340/.TTYIN uses -- directly, hoping to read the
// keyboard without RT-11's own blocking read(); that never saw a
// keypress at all, since channel 0 only carries whatever the
// PPU-resident monitor's own console driver is still choosing to send
// over it, and this program's tag list (see gfourppu.c) has already
// taken over the monitor's video/keyboard state the same way it
// breaks console *output* too (see the comment right before
// ppuc_run() below).
//
// Console messages go through write() rather than printf(): mixing
// buffered stdio with real file I/O has a known, unresolved GCC
// pdp11-backend codegen bug on this toolchain (see examples/ppurun's
// own header comment). They only ever happen before ppuc_run() --
// console I/O only works again once the PPU side has actually handed
// control back to its own resident monitor (see examples/ppupong's
// own header comment, and gfourppu.c's own header comment for when
// gfourppu.c itself does that), and every write()/read() attempted
// between ppuc_run() and that point has been observed to block,
// possibly forever -- this program never attempts one, since it has
// no way to know exactly when gfourppu.c has handed control back
// without asking first, and its own kbd_recv()/any_key_pressed
// mechanism below already covers everything it needs from the
// keyboard -- exactly why that has its own, non-console channel.

#include <unistd.h>

#include "ppu_client.h"
#include "pdp11_irq.h"

#define SCREEN_W 320
#define SCREEN_H 288
#define LINE_WORDS                                                             \
  (SCREEN_W / 8) /* one word = 8px: low byte plane 1, high byte plane 2 */
#define SCREEN_BASE                                                            \
  0100000 /* plane offset -- see gfourppu.c's header comment */

#define SQ_WIDTH 24 /* square width, in pixels */
#define SQ_ROWS 24  /* square height, in pixels */
#define NUM_SQUARES 3
#define NUM_FRAMES                                                             \
  3000 /* safety cap: stop even if no key ever comes (see kbd_recv()) */

// Incremented by vsync_tick (see vsync_init() below) on every
// EVNT trap -- vector 0000100, this hardware's line-clock interrupt,
// confirmed earlier this session (against ukncbtl-qt's own Board.cpp
// timing model) to fire twice per 40ms video frame, the second
// landing right after the last visible scanline. wait_vsync() below
// waits for this to advance by 2 (a full ~40ms video frame, not just
// one ~20ms half of one) before returning.
//
// Just one tick wasn't enough headroom: fill_rect_px()'s per-pixel
// horizontal positioning needs a read-modify-write for every
// partially-covered edge byte (see its own comment), which made a
// single square's erase-move-draw noticeably heavier than the
// original always-byte-aligned version -- confirmed directly, this
// example visibly flickered again once movement went per-pixel, the
// same symptom as the original no-vsync-at-all version, meaning
// drawing was again finishing mid-scan on at least some frames rather
// than safely inside the blanking gap. Waiting a full frame instead
// of a half doubles that gap.
static volatile unsigned int vsync_count;

// The interrupt entry point (vsync_tick itself, declared by this same
// macro -- see its own comment in pdp11_irq.h -- so vsync_init() below
// can just take its address directly) and its callback body (a
// separate, generated vsync_tick_impl) are both produced by this one
// definition. Heavier than the hand-written version this replaces (that
// one was a bare `INC
// _vsync_count; RTI` -- INC operates directly on the memory operand,
// so it never touched a register at all, nothing to save/restore):
// saving and restoring all 6 registers around a real jsr/rts, twice
// per video frame, costs real cycles this ISR never needed, but reuses
// the same trampoline shape as gfourppu.c's own kbd_recv_byte instead
// of a second hand-written one-off.
PDP11_IRQ_HANDLER(vsync_tick) { vsync_count++; }

static struct pdp11_vector saved_evnt;

// Installs vsync_tick over vector 0000100 (saving whatever was
// there before, for vsync_shutdown() to put back, via
// pdp11_irq_vector_swap() -- see pdp11_irq.h) so it can actually fire.
// RT-11 itself may depend on this same vector for its own
// clock/scheduling, hence the save/restore discipline -- matching
// libppu's own ppus_recv_init() precedent for a different vector (see
// ../../libs/libppu/ppus_recv.c). pdp11_irq_vector_swap() itself masks
// interrupts for the swap, so an already-pending EVNT can't fire
// mid-install and land on a half-written vector.
static void vsync_init(void) {
  struct pdp11_vector v;

  v.pc = (unsigned short)(unsigned int)vsync_tick;
  v.psw = 0200;
  saved_evnt = pdp11_irq_vector_swap(0100, v);
}

// Puts vector 0000100 back to whatever vsync_init() found there.
// Must be called before this program exits -- RT-11's own resident
// use of this vector (if any) must not end up pointing at memory this
// program no longer owns once it's gone.
static void vsync_shutdown(void) {
  pdp11_irq_vector_set(0100, saved_evnt);
}

// Busy-waits for a full video frame (2 EVNT traps) to elapse -- called
// once per animation frame, right before that frame's drawing.
// Unsigned wraparound-safe: works even if vsync_count wraps past
// 65535 while this is waiting.
static void wait_vsync(void) {
  unsigned int start = vsync_count;

  while (vsync_count - start < 2) {
  }
}

// Set by kbd_recv() (see main()'s own ppuc_recv_init() call), read by
// the animation loop below -- volatile since it's written from inside
// an interrupt handler (channel 1's own, vector 0460, installed by
// ppuc_recv_init() itself) and read from ordinary foreground code.
static volatile int any_key_pressed;

// gfourppu.c's own keyboard ISR forwards every scancode, press and
// release alike (see its own header comment for the format: a press
// carries the full 7-bit code with bit 7 clear, a release carries
// only a row number with bit 7 set) -- this only cares that *some*
// press happened, so a release (bit 7 set) is simply ignored.
//
// Runs inside ppuc_recv_init()'s own interrupt handler (see
// ppu_client.h's own contract: keep it short, no ppuc_*-family calls
// that themselves wait on an interrupt) -- setting one volatile flag
// is all it does.
static void kbd_recv(const void *buf, unsigned int size) {
  const unsigned char *p = (const unsigned char *)buf;

  if (size >= 1 && (p[0] & 0200) == 0) {
    any_key_pressed = 1;
  }
}

static void msg(const char *s) {
  unsigned int len = 0;

  while (s[len] != 0) {
    len++;
  }
  write(STDOUT_FILENO, s, len);
}

// Small 16-bit LCG (classic constants) -- avoids pulling in newlib's
// rand()/srand() state for something this cosmetic. Low bits of a
// plain LCG have a very short period (bit 0 <=2, bit 1 <=4, ... --
// confirmed directly earlier in this example's own history: masking
// the raw low bits produced perfectly regular, non-random output), so
// every caller here reads bits 6 and up instead.
static unsigned int rng_state = 1;
static unsigned int next_rand(void) {
  rng_state = rng_state * 25173u + 13849u;
  return (rng_state >> 6);
}

// Writes one word (8 pixels: low byte plane 1, high byte plane 2) to
// screen memory at plane offset `addr` through this side's own
// address/data window ports -- see the file header comment for why a
// plain pointer can't reach this memory.
static void write_screen_word(unsigned int addr, unsigned short word) {
  *(volatile unsigned short *)0176640 = (unsigned short)addr;
  *(volatile unsigned short *)0176642 = word;
}

// Reads one word back from screen memory at plane offset `addr` --
// same address/data window ports as write_screen_word(), just read
// instead of written; needed by fill_rect_px() below to preserve
// whatever's outside a square's own edge within a byte it only
// partially covers.
static unsigned short read_screen_word(unsigned int addr) {
  *(volatile unsigned short *)0176640 = (unsigned short)addr;
  return *(volatile unsigned short *)0176642;
}

// color: 0-3 (bit 0 -> plane 1, bit 1 -> plane 2); 0 is black, used
// here to erase a square's old position as well as to paint one.
static unsigned short color_word(unsigned int color) {
  return (unsigned short)((color & 1) ? 0x00ff : 0) |
         (unsigned short)((color & 2) ? 0xff00 : 0);
}

// Zeroes planes 1/2 across the whole screen -- called once at the very
// start (planes 1/2 still hold whatever RT-11's own console last drew
// there; this memory is otherwise ordinary video RAM, not cleared by
// gfourppu.c's own build_tags(), which only ever touches plane 0 --
// see its header comment), and again right before this program exits,
// so gfourppu.c's own clear_screen()/restore_tag0() (plane 0 only, see
// its own comment) hands back a genuinely blank console instead of one
// with this program's last-drawn squares still showing through RT-11's
// own restored palette.
static void clear_screen(void) {
  unsigned int row, col;

  for (row = 0; row < SCREEN_H; row++) {
    for (col = 0; col < LINE_WORDS; col++) {
      write_screen_word(SCREEN_BASE + row * LINE_WORDS + col, 0);
    }
  }
}

// Fills a SQ_WIDTH x SQ_ROWS block with its top-left corner at pixel
// (x, row0) -- x need not be a multiple of 8: a screen word covers 8
// pixels, one bit each, the same bit position in both planes' bytes,
// so a byte the square only partially overlaps (its left or right
// edge) is read, has just the covered bits replaced, and written
// back; a byte it fully covers skips straight to a plain write, the
// same fast path fill_rect() used before this could only ever move in
// whole 8-pixel steps.
static void fill_rect_px(int x, int row0, unsigned int color) {
  unsigned short word = color_word(color);
  int byte_lo = x >> 3;
  int byte_hi = (x + SQ_WIDTH - 1) >> 3;
  int r, bc;

  for (r = 0; r < SQ_ROWS; r++) {
    unsigned int line_base =
        SCREEN_BASE + (unsigned int)(row0 + r) * LINE_WORDS;

    for (bc = byte_lo; bc <= byte_hi; bc++) {
      int bit_start = (bc == byte_lo) ? (x & 7) : 0;
      int bit_end = (bc == byte_hi) ? ((x + SQ_WIDTH - 1) & 7) : 7;

      if (bit_start == 0 && bit_end == 7) {
        write_screen_word(line_base + (unsigned int)bc, word);
      } else {
        unsigned short bitmask =
            (unsigned short)(((2 << bit_end) - 1) & ~((1 << bit_start) - 1));
        unsigned short fullmask = (unsigned short)(bitmask | (bitmask << 8));
        unsigned short old = read_screen_word(line_base + (unsigned int)bc);

        write_screen_word(
            line_base + (unsigned int)bc,
            (unsigned short)((old & ~fullmask) | (word & fullmask)));
      }
    }
  }
}

// One bouncing square: x/row track its current top-left corner, both
// in pixels; dx/drow are its current per-frame velocity, reversed on
// hitting an edge. Horizontal and vertical motion are identical in
// kind (both per-pixel, both magnitude 1-4px/frame) -- fill_rect_px()
// above is what makes arbitrary (not just 8-pixel-aligned) horizontal
// positions possible at all.
struct square {
  int x, row;
  int dx, drow;
  unsigned int color;
};

static int random_speed(void) {
  int speed = (int)(1 + (next_rand() & 3)); /* 1..4 px/frame */

  return (next_rand() & 1) ? speed : -speed;
}

static void init_square(struct square *s, unsigned int color) {
  s->x = (int)(next_rand() % (unsigned int)(SCREEN_W - SQ_WIDTH));
  s->row = (int)(next_rand() % (unsigned int)(SCREEN_H - SQ_ROWS));
  s->dx = random_speed();
  s->drow = random_speed();
  s->color = color;
}

// Moves one square by its current velocity, bouncing (reversing the
// relevant velocity component and clamping back inside the screen)
// off any edge it would otherwise cross.
static void step_square(struct square *s) {
  int x = s->x + s->dx;
  int row = s->row + s->drow;

  if (x < 0) {
    x = 0;
    s->dx = -s->dx;
  } else if (x > SCREEN_W - SQ_WIDTH) {
    x = SCREEN_W - SQ_WIDTH;
    s->dx = -s->dx;
  }

  if (row < 0) {
    row = 0;
    s->drow = -s->drow;
  } else if (row > SCREEN_H - SQ_ROWS) {
    row = SCREEN_H - SQ_ROWS;
    s->drow = -s->drow;
  }

  s->x = x;
  s->row = row;
}

int main(void) {
  long ppu_addr;
  struct square sq[NUM_SQUARES];
  unsigned int i, frame;

  msg("gfour: loading GFPPU.PPU...\r\n");
  ppu_addr = ppuc_load_code("GFPPU.PPU");
  if (ppu_addr < 0) {
    msg("gfour: ppuc_load_code failed\r\n");
    return 1;
  }

  /* 1/2/3 -> palette slots 2/4/6 (see gfourppu.c): green, red, white. */
  init_square(&sq[0], 1);
  init_square(&sq[1], 2);
  init_square(&sq[2], 3);

  msg("gfour: starting PPU...\r\n");
  if (ppuc_run((unsigned short)ppu_addr) < 0) {
    msg("gfour: ppuc_run failed\r\n");
    return 1;
  }

  // Arms kbd_recv() for every scancode gfourppu.c's own keyboard ISR
  // forwards from here on (see that file's header comment) -- no
  // handshake wait before this, unlike ppuc_send() elsewhere in this
  // project: that wait only matters when the PPU program itself also
  // calls ppus_recv_init() (the opposite direction), which gfourppu.c
  // never does.
  ppuc_recv_init(kbd_recv);

  // No console output from here on, ever again: RT-11's own text
  // console draws character bitmaps into the same screen memory this
  // program's own tag list (see gfourppu.c) now controls the meaning
  // of -- confirmed directly, printing anything here corrupts the
  // screen with moving, color-cycling text-shaped noise (the cursor
  // blink included), since the console driver has no idea the tag
  // list it's still implicitly relying on was just replaced.

  clear_screen();

  for (i = 0; i < NUM_SQUARES; i++) {
    fill_rect_px(sq[i].x, sq[i].row, sq[i].color);
  }

  vsync_init();
  for (frame = 0; frame < NUM_FRAMES; frame++) {
    wait_vsync();

    if (any_key_pressed)
      break; /* any key -- stop animating and exit */

    for (i = 0; i < NUM_SQUARES; i++) {
      fill_rect_px(sq[i].x, sq[i].row, 0); /* erase */
      step_square(&sq[i]);
      fill_rect_px(sq[i].x, sq[i].row, sq[i].color);
    }
  }
  clear_screen();
  vsync_shutdown();
  ppuc_recv_shutdown();

  return 0;
}
