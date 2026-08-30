#include "stdio.h"

#define MAX_N 3500
#define DEFAULT_N 3500

int main(int argc, char **argv) {
  char str[20];
  int N = DEFAULT_N;

  if (argc > 1) {
    N = atoi(argv[1]);
    if (N <= 0 || N > MAX_N) {
      N = DEFAULT_N;
    }
  }

  printf("N=%d (pass a digit count as a parameter, up to %d)\r\n", N, MAX_N);

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
    puts(ltoa(res, str, 10));
    c = d%10000;
  }
}
