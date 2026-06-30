int g1, g2;
int complex_fn(int a, int b, int c) {
  int x = a + b;
  int y = c - b;
  if (x > y) {
    g1 = x;
    return x + y;
  } else {
    g2 = y;
    return x - y;
  }
}
