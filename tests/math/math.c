/* math.c -- libm on pdp11-uknc-rt11
 *
 * newlib's math functions are fdlibm, which reaches into a double for its
 * exponent and mantissa on the understanding that the bits are laid out
 * the IEEE way.  Here they are not: float and double are the DEC formats
 * (see pdp11_f_format and pdp11_d_format in gcc/config/pdp11/pdp11.cc).
 * Every one of those reads goes through a handful of macros in
 * newlib/libm/common/fdlibm.h, though, so the gcc fork redefines the
 * macros to hand out an IEEE view of a DEC number and take one back, and
 * the algorithms themselves need no changes: they read the exponent they
 * expect, compute in the arithmetic the machine really has, and hand back
 * bits that go away again in the real format.
 *
 * Each check compares against a constant the compiler folded, to a
 * relative tolerance rather than exactly: these are approximations, and
 * the view costs the low 3 bits of a double besides, 55 bits of fraction
 * not fitting in IEEE's 52.
 *
 * No printf: it costs ten kilobytes, and these programs are already close
 * to what will load.  Which is the other thing to know about libm here:
 * it is enormous against a 64KB address space, so the checks come in
 * groups, one program each -- "make", "make GROUP=2" and so on up to 5 --
 * with sine and cosine in separate ones because the two together do not
 * load, and tan left out altogether, coming to 47KB on its own with a
 * harness around it.  It was checked by hand instead, against values read
 * out of memory in the emulator.  Each failing check appends its own
 * letter to the output.
 */

#include <unistd.h>
#include <math.h>

static char out[64] = "math? ";
static int n = 6, idx = 0, failures = 0;

static void check(double got, double want) {
  double err = got - want, mag = want < 0 ? -want : want;
  char c = (char) ('a' + idx);

  idx++;
  if (err < 0)
    err = -err;
  if (mag < 1.0)
    mag = 1.0;
  if (err <= mag * 1e-12)
    return;
  failures++;
  out[n++] = c;
}

volatile double v0 = 0.0, v1 = 1.0, v2 = 2.0, v4 = 4.0, v100 = 100.0;
volatile double vsmall = 1e-10, vbig = 1e10, vneg = -2.25;
volatile double ve = 2.718281828459045;

int main(void) {
  int e;
  double ip;

  out[4] = '0' + GROUP;

#if GROUP == 1
  check(sqrt(v4), 2.0);                     /* a */
  check(sqrt(v2), 1.4142135623730951);      /* b */
  check(sqrt(vsmall), 1e-5);                /* c */
  check(sqrt(vbig), 100000.0);              /* d */
  check(floor(vneg), -3.0);                 /* e */
  check(ceil(vneg), -2.0);                  /* f */
  check(fabs(vneg), 2.25);                  /* g */
  check(fmod(v100, 7.0), 2.0);              /* h */
  check(ldexp(v1, 10), 1024.0);             /* i */
  check(frexp(v100, &e), 0.78125);          /* j */
  check((double) e, 7.0);                   /* k */
  check(modf(-vneg, &ip), 0.25);            /* l */
  check(ip, 2.0);                           /* m */
#elif GROUP == 2
  check(exp(v0), 1.0);                      /* a */
  check(exp(v1), 2.718281828459045);        /* b */
  check(exp(-v1), 0.36787944117144233);     /* c */
  check(log(v1), 0.0);                      /* d */
  check(log(ve), 1.0);                      /* e */
  check(log(v100), 4.605170185988092);      /* f */
  check(log(vsmall), -23.025850929940457);  /* g */
  (void) ip; (void) e;
#elif GROUP == 3
  check(atan(v1), 0.7853981633974483);      /* a */
  check(atan(v0), 0.0);                     /* b */
  check(atan(vbig), 1.5707963266948965);    /* c */
  check(atan2(v1, v1), 0.7853981633974483); /* d */
  check(atan2(-v1, v1), -0.7853981633974483); /* e */
  check(asin(v1 / v2), 0.5235987755982989); /* f */
  check(acos(v1 / v2), 1.0471975511965979); /* g */
  (void) ip; (void) e;
#elif GROUP == 4
  check(sin(v0), 0.0);                      /* a */
  check(sin(v1), 0.8414709848078965);       /* b */
  check(sin(v100), -0.5063656411097588);    /* c */
  (void) ip; (void) e;
#else
  check(cos(v0), 1.0);                      /* a */
  check(cos(v1), 0.5403023058681398);       /* b */
  check(cos(v100), 0.8623188722876839);     /* c */
  (void) ip; (void) e;
#endif

  out[n++] = ' ';
  if (failures) {
    out[n++] = 'B'; out[n++] = 'A'; out[n++] = 'D';
  } else {
    out[n++] = 'O'; out[n++] = 'K';
  }
  out[n++] = '\n';
  write(1, out, n);
  return failures != 0;
}
