
int puts(const char* ptr) {
  asm (
      "emt    0351;\n\t"
      :
      : "r" (ptr)
      );
}

int main() {
  puts("Hello GCC world!");
}
