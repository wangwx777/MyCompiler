// test_call2.c — 多函数调用测试 (avoid mul)
int add_one(int x) {
    return x + 1;
}

int add_two(int x) {
    return add_one(add_one(x));
}

int main_func(int n) {
    int s = 0;
    for (int i = 0; i < n; i++) {
        s = add_two(s);
    }
    return s;
}
