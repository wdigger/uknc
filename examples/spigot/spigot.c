#include <stdio.h>
#include <stdlib.h>

// MAX_N sizes r[], a *stack*-resident array (2 bytes/digit): this project's
// RT-11 configuration gives a program only ~4.5KB of combined stack+heap
// above its own loaded image (SAV_STACK_ADDRESS=16896 in bfd/sav-pdp11.c,
// minus this program's own ~12KB image size) before running into the
// resident monitor -- there's no guard page, so overrunning it silently
// corrupts the program's own code/data instead of a clean crash. 1500
// keeps r[] at 3002 bytes, leaving comfortable headroom.
#define MAX_N 1500
#define DEFAULT_N 1500

// newlib's printf always pulls in the dtoa/mprec float-formatting machinery
// on this target -- _printf_float is meant to be a weak symbol so it's only
// linked when actually used, but GCC's pdp11 backend doesn't support weak
// declarations ("warning: weak declaration of 'foo' not supported"), so the
// reference is always strong and always drags dtoa/mprec in, regardless of
// whether any %f/%e/%g is ever used.  A local ltoa + fputs (which, unlike
// puts, doesn't append a newline) avoids printf entirely and keeps this
// program's memory footprint small enough for UKNC's 64K address space.
static char *ltoa(long num, char *str, int base) {
  int i = 0;
  int isNegative = num < 0 && base == 10;

  if (num == 0) {
    str[0] = '0';
    str[1] = '\0';
    return str;
  }

  unsigned long unum = isNegative ? -(unsigned long)num : (unsigned long)num;

  while (unum != 0) {
    unsigned long rem = unum % base;
    str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
    unum /= base;
  }

  if (isNegative) {
    str[i++] = '-';
  }
  str[i] = '\0';

  for (int start = 0, end = i - 1; start < end; start++, end--) {
    char tmp = str[start];
    str[start] = str[end];
    str[end] = tmp;
  }

  return str;
}

int main(int argc, char **argv) {
  // Avoid the default buffered stdio path: it mallocs a 1024-byte BUFSIZ
  // buffer on the first write (our _fstat() doesn't set st_blksize, so
  // __swhatbuf_r falls back to BUFSIZ) -- on top of r[]'s own footprint,
  // that would burn through most of the ~4.5KB budget explained above.
  setvbuf(stdout, NULL, _IONBF, 0);

  char buf[20];
  int N = DEFAULT_N;

  if (argc > 1) {
    N = atoi(argv[1]);
    if (N <= 0 || N > MAX_N) {
      N = DEFAULT_N;
    }
  }

  fputs("N=", stdout);
  fputs(ltoa(N, buf, 10), stdout);
  fputs(" (pass a digit count as a parameter, up to ", stdout);
  fputs(ltoa(MAX_N, buf, 10), stdout);
  fputs(")\r\n", stdout);

  short r[MAX_N + 1], i, k, b, c;
  long d;
  c = 0;
  for (i = 1; i <= N; i++)
    r[i] = 2000;

  for (k = N; k > 0; k -= 14) {
    d = 0;
    i = k;
    for(;;) {
      d += r[i]*10000L;
      b = i*2 - 1;
      r[i] = d%b;
      d /= b;
      i--;
      if (i == 0) break;
      d *= i;
    }
    long res = (long)(c + d/10000);
    fputs(ltoa(res, buf, 10), stdout);
    c = d%10000;
  }
}
