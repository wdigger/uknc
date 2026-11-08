#include <stdio.h>
#include <koi7.h>

int main(void) {
  puts("Hello GCC world!");
  printf("printf: %s\n", TO_KOI7(L"Привет, УКНЦ!"));
  puts(TO_KOI7(L"puts: Привет, УКНЦ!"));
  puts(TO_KOI7(L"mixed: Привет GCC мир!"));
}
