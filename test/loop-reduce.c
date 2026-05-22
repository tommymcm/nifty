int strength_reduce(int n) {
  int sum = 0;
  for (int i = 0; i < n; i++) {
    sum += i * 8; // multiply → shift
  }
  return sum;
}
