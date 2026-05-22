int licm_example(int *arr, int n, int factor) {
  int result = 0;
  for (int j = 0; j < n; j++) {
    for (int i = 0; i < n; i++) {
      int inv = factor * j; // loop-invariant, should hoist
      result += arr[i] * inv;
    }
  }
  return result;
}
