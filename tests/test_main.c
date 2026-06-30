//===-- test_main.c - MyCPU 后端功能测试 ---------------------------------===//
//
// 编译命令:
//   clang -target mycpu-unknown-elf -S -o test.s test_main.c
//   clang -target mycpu-unknown-elf -O2 -S -o test_opt.s test_main.c
//
//===----------------------------------------------------------------------===//

// 1. 简单算术运算
int add(int a, int b) {
    return a + b;
}

int sub(int a, int b) {
    return a - b;
}

int bitwise_and(int a, int b) {
    return a & b;
}

int bitwise_or(int a, int b) {
    return a | b;
}

int bitwise_xor(int a, int b) {
    return a ^ b;
}

int shift_left(int a, int b) {
    return a << b;
}

int shift_right(int a, int b) {
    return a >> b;
}

// 2. 控制流 — if/else
int max(int a, int b) {
    if (a > b)
        return a;
    else
        return b;
}

int abs_value(int x) {
    if (x < 0)
        return -x;
    return x;
}

// 3. 循环
int sum_array(int *arr, int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += arr[i];
    }
    return sum;
}

int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; i++) {
        result = result * i;
    }
    return result;
}

// 4. 函数调用链
int mul_add(int x, int y, int z) {
    return add(x, sub(y, z));
}

// 5. 全局变量
int g_counter = 0;

void increment_counter(void) {
    g_counter++;
}

int get_counter(void) {
    return g_counter;
}

// 6. 指针与内存
void swap(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

int load_value(int *p) {
    return *p;
}

void store_value(int *p, int v) {
    *p = v;
}

// 7. volatile (直接生成 LD/ST，不经过优化)
int read_volatile(volatile int *reg) {
    return *reg;
}

void write_volatile(volatile int *reg, int val) {
    *reg = val;
}
