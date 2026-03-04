int puts(const char* ptr) {
  asm (
      "emt    0351;\n\t"
      :
      : "r" (ptr)
      );
}

float const_55_55() {
  return 55.55f;
}

float test_add(float a, float b) {
  return a + b;
}

float test_div(float a, float b) {
  return a / b;
}

bool test_01() {
  return (const_55_55() == 55.55f);
}

bool test_01_n() {
  return (const_55_55() == 55.56f);
}

bool test_02() {
  return !(const_55_55() != 55.55f);
}

bool test_02_n() {
  return !(const_55_55() != 55.56f);
}

bool test_03() {
  return (test_add(12.34, 43.21) == 55.55f);
}

bool test_04() {
  return (test_div(654.321, 123.456) == 5.30003402f);
}

#define TEST_RUN(name) if(name()) {puts(#name"\tOk");} else puts(#name"\tFailure");
#define TEST_NEG_RUN(name) if(!name()) {puts(#name"\tOk");} else puts(#name"\tFailure");

int main() {
  TEST_RUN(test_01)
  TEST_NEG_RUN(test_01_n)
  TEST_RUN(test_02)
  TEST_NEG_RUN(test_02_n)
  TEST_RUN(test_03)
  TEST_RUN(test_04)
}
