int branch_simplify(int x) {
  if (x > 0) {
    return 1;
  } else if (x > 0) { // redundant
    return 2;
  }
  return 0;
}

int select_example(int x) {
  int r;
  if (x > 0)
    r = 1; // convertible to select
  else
    r = -1;
  return r;
}
