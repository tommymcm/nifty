extern int rand_int();

int check_cmp() {
  int x = rand_int();
  int y = rand_int();

  if (x < y)
    return x * y;
  else if (x == y)
    return x + y;
  else
    return 0;
}

int check_cmp_const() {
  int var = rand_int();

  if (var < 10)
    return var;
  else if (var == 0)
    return var * 10;
  else
    return 0;
}

int check_argument(int arg) {
  if (arg)
    return 3;
  else
    return 7;
}

int check_cmp_arguments(int x, int y) {
  if (x < y)
    return x * y;
  else if (x == y)
    return x + y;
  else
    return 0;
}

int check_call() {
  if (rand_int())
    return 3;
  else
    return 7;
}

int check_while() {
  int var = rand_int();
  while (var)
    var = rand_int();

  return var;
}
