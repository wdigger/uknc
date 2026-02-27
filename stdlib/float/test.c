float test_01(float a, float b) {
  return a + b;
}

int main() {
  if (test_01(12.34, 43.21) != 533.2114) {
    return -1;
  }
  return 0;
}
