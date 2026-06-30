int callee(int x) { return x * 2; }
int caller(int a) { return callee(a) + 1; }
