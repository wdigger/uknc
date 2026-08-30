#ifndef _STDIO_H
#define _STDIO_H

#define size_t int

int putchar(int c);
int puts(const char* ptr);
size_t strlen(const char *str);
char* ltoa(long num, char* str, int base);
char* itoa(int num, char* str, int base);
int printf(const char *format, ...);
int atoi(const char *str);

#endif  // _STDIO_H
