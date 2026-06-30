// test_mycpu_core.c — MyCPU 后端核心功能测试
// 编译: clang -target mycpu-unknown-elf -S -o test_mycpu_core.s test_mycpu_core.c

// 1. 算术运算 — ADDW / SUBW / ANDW / ORW / XORW
int add(int a, int b)       { return a + b; }
int sub(int a, int b)       { return a - b; }
int bit_and(int a, int b)   { return a & b; }
int bit_or(int a, int b)    { return a | b; }
int bit_xor(int a, int b)   { return a ^ b; }

// 2. 移位 — SHLW / SHRW / SARW
int shift_left(int a, int b)  { return a << b; }
int shift_right(int a, int b) { return a >> b; }

// 3. 控制流 — CMPW + BZW / BNZ + JMP
int max(int a, int b) {
    if (a > b) return a;
    else return b;
}

int abs_val(int x) {
    if (x < 0) return -x;
    return x;
}

// 4. 循环 — 循环展开 + 数组访问
int sum_array(int *arr, int n) {
    int sum = 0;
    for (int i = 0; i < n; i++)
        sum += arr[i];
    return sum;
}

// 5. 全局变量 — LDW / STW + 全局地址
int g_counter = 0;
void increment(void)  { g_counter++; }
int get_counter(void) { return g_counter; }

// 6. 指针 — 间接 LDW / STW
void swap(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

int load_val(int *p)  { return *p; }
void store_val(int *p, int v) { *p = v; }

// 7. volatile — 强制生成 LD/ST，不走优化
int read_reg(volatile int *reg)     { return *reg; }
void write_reg(volatile int *reg, int v) { *reg = v; }
