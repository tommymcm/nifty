int factorial_tail(int n, int acc) {
  if (n <= 1)
    return acc;
  return factorial_tail(n - 1, n * acc); // tail call
}
