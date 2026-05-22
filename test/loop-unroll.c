int small_loop(int *arr) {
  int sum = 0;
  for (int i = 0; i < 4; i++) { // small fixed bound → unroll
    sum += arr[i];
  }
  return sum;
}
