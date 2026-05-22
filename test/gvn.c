int cse_example(int x, int y) {
  int a = x * y + x;
  int b = x * y + y; // x*y is a common subexpression
  return a + b;
}
