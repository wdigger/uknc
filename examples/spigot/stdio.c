#include <stdarg.h>

int printf(const char *format, ...);

int putchar(int c) {
  unsigned int cc = (unsigned int)c;
  asm (
      "emt    0341\n\t"
      "bcs    .-2\n\t"
      :
      : "r" (cc)
      );
  return c;
}

int puts(const char* ptr) {
/*
  asm (
      "emt    0351;\n\t"
      :
      : "r" (ptr)
      );
*/
  while(*ptr != 0) {
    putchar(*ptr++);
  }
}

#define size_t int

size_t strlen(const char *str) {
  const char *s;
  for (s = str; *s; ++s) { }
  return (s - str);
}

void reverse(char str[], int length) {
  int start = 0;
  int end = length - 1;
  while (start < end) {
    char temp = str[start];
    str[start] = str[end];
    str[end] = temp;
    end--;
    start++;
  }
}

char* oltoa(long num, char* str) {
  int i = 0;

  if (num == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return str;
  }

  unsigned long unum = *((unsigned long*)(&num));

  char *s = str;
  while (unum != 0) {
    *s = '0' + (unum & 7);
    unum = unum >> 3;
    s++;
  }
  *s = 0;

  reverse(str, s - str);

  return str;
}

char* ltoa(long lnum, char* str, int base) {
  if (base == 8) {
    return oltoa(lnum, str);
  }

  int i = 0;
  int isNegative = 0;

  if (lnum == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return str;
  }

  if (lnum < 0L && base == 10) {
    isNegative = 1;
  }

  unsigned long num = (unsigned long)((lnum < 0L) ? -lnum : lnum);

  while (num != 0UL) {
    unsigned long rem = num % base;
    char c = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
    str[i++] = c;
    num = num / base;
  }

  if (isNegative) {
    str[i++] = '-';
  }

  str[i] = 0;

  reverse(str, i);

  return str;
}

char* itoa(int num, char* str, int base) {
  return ltoa(num, str, base);
}

int atoi(const char *str) {
  int result = 0;
  int sign = 1;

  if (*str == '-') {
    sign = -1;
    str++;
  } else if (*str == '+') {
    str++;
  }

  while (*str >= '0' && *str <= '9') {
    result = result * 10 + (*str - '0');
    str++;
  }

  return result * sign;
}

int printf(const char *format, ...) {
  va_list args;
  int count = 0;

  if (!format)
    return -1;

  va_start(args, format);

  for (const char *ptr = format; *ptr != '\0'; ptr++) {
    if (*ptr == '%') {
      ptr++;
      if (*ptr == '\0') {
        va_end(args);
        return -1;
      }

      switch (*ptr) {
        case 'c': {
          char c = va_arg(args, int);
          putchar(c);
          count++;
          break;
        }
        case 's': {
          char *str = va_arg(args, char*);
          if (!str) str = "(null)";
          for (int i = 0; str[i] != '\0'; i++) {
            putchar(str[i]);
            count++;
          }
          break;
        }
        case '%': {
          putchar('%');
          count++;
          break;
        }
        case 'i':
        case 'd': {
          int num = va_arg(args, int);
          char str[10] = {};
          itoa(num, str, 10);
          puts(str);
          break;
        }
        case 'l': {
          ptr++;
          switch(*ptr) {
            case 'd': {
              long num = va_arg(args, long);
              char str[10] = {};
              ltoa(num, str, 10);
              puts(str);
              break;
            }
            default:
              puts("%l");
              putchar(*ptr);
              count += 2;
              break;
          }
          break;
        }
        case 'o': {
          ptr++;
          switch(*ptr) {
            case 'l': {
              long num = va_arg(args, long);
              char str[10] = {};
              ltoa(num, str, 8);
              puts(str);
              break;
            }
            default:
              puts("%o");
              putchar(*ptr);
              count += 2;
              break;
          }
          break;
        }

        default:
          putchar('%');
          putchar(*ptr);
          count += 2;
          break;
      }
    } else {
      putchar(*ptr);
      count++;
    }
  }

  va_end(args);
  return count; // Return the number of characters printed
}
