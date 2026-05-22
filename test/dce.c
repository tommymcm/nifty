// Before: dead store, unused branch
int dce_example(int x) {
  int unused = x * 42; // dead store
  int result = x + 1;
  if (0) { // dead branch
    result = 999;
  }
  return result;
}
