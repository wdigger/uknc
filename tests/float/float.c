/* float.c -- floating point arithmetic, conversions and comparisons for
 * pdp11-uknc-rt11
 *
 * Every check compares a value computed at run time against the same
 * value folded by the compiler, so the two have to agree bit for bit.
 * That is the whole point: this target's float and double are the DEC F
 * and D formats (see pdp11_f_format and pdp11_d_format in
 * gcc/config/pdp11/pdp11.cc), while libgcc's generic soft float in
 * fp-bit.c was hardwired to IEEE.  The gcc fork now gives fp-bit the DEC
 * numbers, and carries its own smaller conversions and comparisons for
 * single precision in libgcc/config/pdp11/fpconv-*.c.
 *
 * Everything is checked both ways, with the FIS instructions, which this
 * vendor has on by default, and without them ("make NOFIS=1"), since
 * fp-bit was taught both DEC formats and is correct on its own; FIS only
 * makes single precision faster.
 *
 * Double precision is the DEC D format, an 8-bit exponent and 55 bits of
 * fraction, which is nothing like IEEE double and its 11-bit exponent --
 * all of it was meaningless before fp-bit knew the format.
 *
 * No %f anywhere: the formatting machinery in newlib costs about 36KB and
 * does not fit in this machine's address space, which is why the
 * toolchain stubs it out by default.  Values are compared as the words
 * they are made of instead.
 */

#include <stdio.h>

union fw {
  float f;
  unsigned short w[2];
};

union dw {
  double d;
  unsigned short w[4];
};

static int failures = 0;

/* Volatile so that the left hand side of every check is really computed
 * at run time, by the routine or instruction under test, rather than
 * folded away into the constant it is being compared against.
 */
volatile int i3 = 3, im7 = -7, imin16 = -32768;
volatile long lmax = 2147483647L, lmin = -2147483648L, ltie = 16777217L;
volatile unsigned long umax = 4294967295UL;
volatile long long ll5 = 5LL, llbig = 123456789012LL, llneg = -98765432109LL;
volatile long long llmax = 9223372036854775807LL;
volatile unsigned long long ullmax = 18446744073709551615ULL;
volatile float fa = 3.0f, fb = 2.0f, fc = 10.0f, fd = 0.25f;
volatile float f1 = 1.0f, f99 = 99.75f, fm99 = -99.75f;
volatile float fe11 = 1.23456789e11f, fsmall = 0.25f;
volatile float fzero = 0.0f, fneg1 = -1.0f, fneg2 = -2.0f;
volatile double da = 3.0, db = 2.0, dbig = 1e7, dneg = -1.5;
/* Two values a hair apart, for the cancellation checks below.  */
volatile double dnear = 0x1.0000000001p0;   /* 1 + 2**-40, exactly */
volatile long long lltop = 0xd1cf7980LL, llmid = 0xd1cf00000000LL;

static void check_float(const char *name, float got, float want) {
  union fw g, w;

  g.f = got;
  w.f = want;
  if (g.w[0] == w.w[0] && g.w[1] == w.w[1])
    return;
  failures++;
  printf("%s: %04x%04x, want %04x%04x\n", name, g.w[0], g.w[1], w.w[0],
         w.w[1]);
}

static void check_double(const char *name, double got, double want) {
  union dw g, w;

  g.d = got;
  w.d = want;
  if (g.w[0] == w.w[0] && g.w[1] == w.w[1] && g.w[2] == w.w[2]
      && g.w[3] == w.w[3])
    return;
  failures++;
  printf("%s: %04x%04x%04x%04x, want %04x%04x%04x%04x\n", name, g.w[0], g.w[1],
         g.w[2], g.w[3], w.w[0], w.w[1], w.w[2], w.w[3]);
}

static void check_long(const char *name, long long got, long long want) {
  if (got == want)
    return;
  failures++;
  printf("%s: %ld:%lu, want %ld:%lu\n", name, (long) (got >> 32),
         (unsigned long) got, (long) (want >> 32), (unsigned long) want);
}

static void check_int(const char *name, int got, int want) {
  if (got == want)
    return;
  failures++;
  printf("%s: %d, want %d\n", name, got, want);
}

int main(void) {
  /* Integer to float.  */
  check_float("int 3", (float) i3, 3.0f);
  check_float("int -7", (float) im7, -7.0f);
  check_float("int min", (float) imin16, -32768.0f);
  check_float("long max", (float) lmax, 2147483647.0f);
  check_float("long min", (float) lmin, -2147483648.0f);
  check_float("ulong max", (float) umax, 4294967295.0f);
  /* 2**24+1 needs a 25th bit, so it rounds, and the tie goes to even.  */
  check_float("long tie", (float) ltie, 16777216.0f);
  check_float("llong 5", (float) ll5, 5.0f);
  check_float("llong big", (float) llbig, 123456789012.0f);
  check_float("llong neg", (float) llneg, -98765432109.0f);
  check_float("llong max", (float) llmax, 9223372036854775807.0f);
  check_float("ullong max", (float) ullmax, 18446744073709551615.0f);

  /* Float to integer, truncating towards zero.  */
  check_long("to long 1", (long long) f1, 1LL);
  check_long("to long 99", (long long) f99, 99LL);
  check_long("to long -99", (long long) fm99, -99LL);
  check_long("to long small", (long long) fsmall, 0LL);
  check_long("to long e11", (long long) fe11, (long long) 1.23456789e11f);
  check_long("to ullong", (long long) (unsigned long long) fe11,
             (long long) 1.23456789e11f);
  check_int("to int 99", (int) f99, 99);
  check_int("to int -99", (int) fm99, -99);

  /* Addition and subtraction.  */
  check_float("add", fa + fb, 5.0f);
  check_float("sub", fa - fb, 1.0f);
  check_float("sub neg", fb - fa, -1.0f);
  check_float("add mixed", fc + fd, 10.25f);

  /* Comparison.  */
  check_int("gt", fa > fb, 1);
  check_int("lt", fm99 < fd, 1);
  check_int("eq", fa == 3.0f, 1);
  check_int("gt neg", fneg1 > fneg2, 1);
  check_int("lt zero", fneg1 < fzero, 1);

  /* Comparison at the top of the exponent range, where an exponent field
   * of 255 stands for an ordinary value near 1.7e38.  fp-bit used to read
   * that as a NaN and answer false to everything asked about it.
   */
  {
    union fw big;
    volatile float vbig;

    big.w[0] = 0x7fff;
    big.w[1] = 0xffff;
    vbig = big.f;
    check_int("big gt one", vbig > 1.0f, 1);
    check_int("big gt var", vbig > fa, 1);
    check_int("one gt big", 1.0f > vbig, 0);
    check_int("big eq self", vbig == vbig, 1);
    check_int("neg big lt", -vbig < fneg1, 1);
    check_int("unordered", __builtin_isunordered(fa, vbig), 0);
  }

  /* A sign bit over a zero exponent is not a negative zero in this
   * format, it is a reserved operand; and a zero exponent with a fraction
   * is no more than a zero either.  Both compare as zero.
   */
  {
    union fw odd;
    volatile float vodd;

    odd.w[0] = 0x8000;
    odd.w[1] = 0x0000;
    vodd = odd.f;
    check_int("signed zero eq", vodd == fzero, 1);
    check_int("signed zero lt", vodd < fzero, 0);
    odd.w[0] = 0x0040;
    odd.w[1] = 0x0000;
    vodd = odd.f;
    check_int("dirty zero eq", vodd == fzero, 1);
  }

  /* Multiplication and division.  */
  check_float("mul", fa * fb, 6.0f);
  check_float("div", fa / fb, 1.5f);
  check_float("mul frac", fc * fd, 2.5f);
  check_float("div frac", fc / fd, 40.0f);
  check_float("div inexact", fb / fa, 0.666666666f);
  check_float("compound", (fa + fb) * (fa - fb), 5.0f);

  /* Double precision, which is the DEC D format: an 8-bit exponent like
   * single precision, and 55 bits of fraction.
   */
  check_double("d add", da + db, 5.0);
  check_double("d sub", db - da, -1.0);
  check_double("d mul", da * db, 6.0);
  check_double("d div", da / db, 1.5);
  check_double("d inexact", da / 7.0, 3.0 / 7.0);
  check_double("d big", dbig * dbig, 1e14);
  check_double("d neg", -da, -3.0);
  check_int("d gt", da > db, 1);
  check_int("d lt", dneg < db, 1);
  check_double("int to d", (double) i3, 3.0);
  check_long("d to long", (long long) dbig, 10000000LL);
  /* Subtracting two nearly equal values leaves a result that has to be
   * shifted a long way back up, and fp-bit settles its sign by testing a
   * 64-bit difference against zero.  A backend bug in that test -- the
   * sign of a four-word value was read off the first word that was not
   * zero rather than off the most significant one -- made this come out
   * as about minus one, which is what made sinh and expm1 wrong.
   */
  check_double("cancel", dnear - 1.0, 0x1p-40);
  check_double("cancel twice", (dnear - 1.0) - 0x1p-40, 0.0);

  /* The same backend bug seen directly: these are positive values whose
   * most significant word is zero and whose next word has its top bit
   * set, which used to test as negative.
   */
  check_int("llong sign", lltop >= 0, 1);
  check_int("llong sign 2", llmid >= 0, 1);
  check_int("llong gt", lltop > 0, 1);
  check_int("llong neg", -lltop < 0, 1);

  check_double("float to d", (double) fa, 3.0);
  check_float("d to float", (float) da, 3.0f);
  check_float("d to float inexact", (float) (da / 7.0), (float) (3.0 / 7.0));

#ifdef __pdp11_fis
  puts("FIS: yes, single precision arithmetic is done in hardware");
#else
  puts("FIS: no, single precision arithmetic goes to the library");
#endif

  if (failures == 0) {
    puts("float: all checks passed");
    return 0;
  }
  printf("float: %d check(s) failed\n", failures);
  return 1;
}
