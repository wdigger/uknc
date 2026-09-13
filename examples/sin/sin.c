// sin.c -- draws a sine wave across a 320x288 screen and scrolls it,
// with the curve computed by the C library rather than from a table
// written out by hand.
//
// The point of this example is that sinf() works here at all.  This
// target's float is the DEC format, not IEEE, and newlib's math
// functions are fdlibm, which reads a number's exponent and mantissa
// straight out of the bits on the understanding that they are laid out
// the IEEE way -- so until the gcc fork taught those reads about the DEC
// formats (see the README's libm section), sqrt(4.0) and everything
// beside it returned nonsense.  The curve below would have been a mess
// of noise.
//
// Single precision on purpose.  sin() drags in the double precision
// half of the library, and a program with the graphics of this one plus
// sin() comes to about 50KB, which RT-11 here will not load at all; with
// sinf() it is around 35KB.  That is the shape of libm on a machine with
// a 64KB address space: it works, it just does not all fit at once, and
// the choice between sin and sinf is a choice about whether the program
// exists.
//
// The wave is computed once, into a table of one full period, and
// animated by walking a moving offset through that table.  Not for want
// of trying to be clever: a single sinf() takes a good fraction of a
// frame here, so 320 of them per frame would leave nothing for anything
// else, while re-reading a table that is already there costs nothing.
// The point stands either way, since the table is what the C library
// computed.
//
// The PPU side, the palette and the keyboard are exactly what
// examples/gfour does, and its files explain them: the video generator's
// plane 0 is PPU-owned, and the address the pixels live at is sent over
// libppu's channel 1, which also brings keypresses back.  The PPU side
// here is gfour's own source, built again under a name RT-11 will take
// (see the Makefile): it asks for nothing this program does not want.
//
// Where the pixels live is the one thing done differently, and libm is
// the reason.  gfour keeps them in a malloc()ed buffer of its own, which
// is both faster and tidier -- the CPU's RAM is planes 1 and 2
// interleaved, so a pixel is a memory write, and RT-11's own text screen
// is left untouched.  But that buffer is 23KB, and 23KB on top of this
// program's 33KB does not fit in a 64KB address space: the screen never
// got allocated at all.  So this draws where RT-11's console text screen
// lives instead, plane offset 0100000, which costs nothing in RAM
// because it is not in the CPU's address space at all -- it is reached
// through the window registers at 0176640/0176642, an address write and
// a data write per eight pixels.  gfourppu.c's header comment tells the
// story of that address; this program is the case where the old way is
// the only way that fits.  The text that was on the console screen is
// overwritten, and the screen is cleared again on the way out.
//
// Console messages go through write() rather than printf(), and only
// before ppuc_run(): once the PPU side owns the screen, console I/O
// blocks until it hands control back.  gfour.c's header comment has the
// whole account.

#include <math.h>
#include <unistd.h>

#include "pdp11_irq.h"
#include "ppu_client.h"

#define SCREEN_W 320
#define SCREEN_H 288
/* One word is 8 pixels: low byte plane 1, high byte plane 2.  A line is
   40 of them, and they are counted in plane offsets, which is what the
   window registers and the PPU's tag list both take.  */
#define LINE_WORDS (SCREEN_W / 8)

/* RT-11's console text screen, where this program draws -- see the file
   header comment for why it is not a buffer of its own.  */
#define SCREEN_BASE 0100000

#define PPU_MSG_BYE 0377

/* Two of the three colours gfourppu.c's palette provides; 0 is the
   background.  */
#define COLOR_AXIS 1
#define COLOR_CURVE 3

#define MID_ROW (SCREEN_H / 2)
#define AMPLITUDE (MID_ROW - 24) /* leave a margin top and bottom */
#define TICK_EVERY 40            /* columns between ticks on the axis */
#define TICK_HALF 3              /* tick half-height, in pixels */

#define SCROLL_STEP 2 /* columns the wave moves per frame */
#define NUM_FRAMES 3000 /* safety cap for an unattended run */

static volatile unsigned int vsync_count;

PDP11_IRQ_HANDLER(vsync_tick) { vsync_count++; }

static struct pdp11_vector saved_evnt;

static void vsync_init(void) {
  struct pdp11_vector v;

  v.pc = (unsigned short)(unsigned int)vsync_tick;
  v.psw = 0200;
  saved_evnt = pdp11_irq_vector_swap(0100, v);
}

static void vsync_shutdown(void) { pdp11_irq_vector_set(0100, saved_evnt); }

static void wait_vsync(void) {
  unsigned int start = vsync_count;

  while (vsync_count - start < 1) {
  }
}

static volatile int any_key_pressed;
static volatile int ppu_bye;

static void kbd_recv(const void *buf, unsigned int size) {
  const unsigned char *p = (const unsigned char *)buf;

  if (size < 1) {
    return;
  }
  if (p[0] == PPU_MSG_BYE) {
    ppu_bye = 1;
  } else if ((p[0] & 0200) == 0) {
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

/* The window registers: write the plane offset to the first, then the
   pixels to the second -- low byte plane 1, high byte plane 2, eight
   pixels in all.  Reading the second gives back what is there, which a
   single pixel needs, since its seven neighbours have to survive.  */
#define PLANE_ADDR_PORT ((volatile unsigned short *)0176640)
#define PLANE_DATA_PORT ((volatile unsigned short *)0176642)

static unsigned short screen_read(unsigned int offset) {
  *PLANE_ADDR_PORT = (unsigned short)offset;
  return *PLANE_DATA_PORT;
}

static void screen_write(unsigned int offset, unsigned short value) {
  *PLANE_ADDR_PORT = (unsigned short)offset;
  *PLANE_DATA_PORT = value;
}

static unsigned int line_offset(int row) {
  return SCREEN_BASE + (unsigned int)row * LINE_WORDS;
}

static void clear_screen(void) {
  unsigned int off = SCREEN_BASE;
  unsigned int n = (unsigned int)SCREEN_H * LINE_WORDS;

  while (n-- != 0) {
    screen_write(off++, 0);
  }
}

/* One pixel, two bits: the column's bit in the low byte is plane 1, the
   same bit in the high byte is plane 2.  */
static void plot(int x, int row, unsigned int color) {
  unsigned int off;
  unsigned short bit, both, value;

  if (x < 0 || x >= SCREEN_W || row < 0 || row >= SCREEN_H) {
    return;
  }
  off = line_offset(row) + (unsigned int)(x >> 3);
  bit = (unsigned short)(1u << (x & 7));
  both = (unsigned short)(bit | (bit << 8));
  value = (unsigned short)(((color & 1) ? bit : 0) |
                           ((color & 2) ? (unsigned short)(bit << 8) : 0));
  screen_write(off, (unsigned short)((screen_read(off) & (unsigned short)~both) |
                                     value));
}

/* What is under the curve at a given pixel, so that erasing the curve
   puts the axes back rather than punching holes in them.  */
static unsigned int background_at(int x, int row) {
  if (row == MID_ROW) {
    return COLOR_AXIS;
  }
  if (x == 0) {
    return COLOR_AXIS;
  }
  if (x % TICK_EVERY == 0 && row >= MID_ROW - TICK_HALF &&
      row <= MID_ROW + TICK_HALF) {
    return COLOR_AXIS;
  }
  return 0;
}

static void draw_axes(void) {
  int x, row;

  for (x = 0; x < SCREEN_W; x++) {
    plot(x, MID_ROW, COLOR_AXIS);
  }
  for (row = 0; row < SCREEN_H; row++) {
    plot(0, row, COLOR_AXIS);
  }
  for (x = TICK_EVERY; x < SCREEN_W; x += TICK_EVERY) {
    for (row = MID_ROW - TICK_HALF; row <= MID_ROW + TICK_HALF; row++) {
      plot(x, row, COLOR_AXIS);
    }
  }
}

/* One period of the sine, as row offsets from the middle of the screen.
   This is the whole reason the example exists: the numbers come from the
   C library, not from a table someone typed in.  */
static short wave[SCREEN_W];

static void compute_wave(void) {
  int i;

  for (i = 0; i < SCREEN_W; i++) {
    float angle = 2.0f * (float)M_PI * (float)i / (float)SCREEN_W;
    float y = sinf(angle) * (float)AMPLITUDE;

    /* Round away from zero rather than truncating, so that the curve is
       symmetric about the axis.  */
    wave[i] = (short)(y < 0.0f ? y - 0.5f : y + 0.5f);
  }
}

static int wave_row(int x, int phase) {
  int i = (x + phase) % SCREEN_W;

  /* Rows count downward, so a positive sine goes up the screen.  */
  return MID_ROW - wave[i];
}

/* One column of the curve, drawn as the span between this column's row
   and the previous one's: a pixel per column alone leaves the curve in
   dots wherever it is steep, which near the zero crossings is most of
   it.  COLOR zero erases, putting back whatever the axes had there.  */
static void draw_column(int x, int phase, unsigned int color) {
  int row = wave_row(x, phase);
  int prev = (x == 0) ? row : wave_row(x - 1, phase);
  int from = (prev < row) ? prev : row;
  int to = (prev < row) ? row : prev;
  int r;

  for (r = from; r <= to; r++) {
    plot(x, r, color != 0 ? color : background_at(x, r));
  }
}

int main(void) {
  long ppu_addr;
  int x, phase, prev_phase;
  unsigned int frame;

  msg("sin: loading SINPPU.PPU...\r\n");
  ppu_addr = ppuc_load_code("SINPPU.PPU");
  if (ppu_addr < 0) {
    msg("sin: ppuc_load_code failed\r\n");
    return 1;
  }

  /* Before the PPU takes the screen, while the console still answers.  */
  msg("sin: computing one period with sinf()...\r\n");
  compute_wave();

  msg("sin: starting PPU...\r\n");
  if (ppuc_run((unsigned short)ppu_addr) < 0) {
    msg("sin: ppuc_run failed\r\n");
    return 1;
  }
  {
    unsigned short base = SCREEN_BASE;

    ppuc_send(&base, sizeof base);
  }
  ppuc_recv_init(kbd_recv);

  clear_screen();
  draw_axes();

  phase = 0;
  for (x = 0; x < SCREEN_W; x++) {
    draw_column(x, phase, COLOR_CURVE);
  }

  vsync_init();
  for (frame = 0; frame < NUM_FRAMES; frame++) {
    wait_vsync();
    if (any_key_pressed) {
      break; /* any key stops it */
    }

    prev_phase = phase;
    phase = (phase + SCROLL_STEP) % SCREEN_W;
    for (x = 0; x < SCREEN_W; x++) {
      draw_column(x, prev_phase, 0);
      draw_column(x, phase, COLOR_CURVE);
    }
  }

  /* The pixels went into RT-11's own console screen memory, so clear it
     on the way out rather than leave the curve there for the monitor to
     print over.  */
  clear_screen();

  while (!ppu_bye) {
  }
  vsync_shutdown();
  ppuc_recv_shutdown();
  return 0;
}
