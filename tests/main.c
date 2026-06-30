// MyCPU 后端测试程序
// 编译命令: clang -target mycpu -S main.c -o main.s
// 查看汇编: cat main.s

// === 1. 算术运算测试 ===
int add(int a, int b) {
    return a + b;
}

int sub(int a, int b) {
    return a - b;
}

int and_op(int a, int b) {
    return a & b;
}

int or_op(int a, int b) {
    return a | b;
}

int xor_op(int a, int b) {
    return a ^ b;
}

// === 2. 移位运算测试 ===
int shl(int a, int n) {
    return a << n;
}

int shr(int a, int n) {
    return a >> n;
}

// === 3. 条件分支测试 ===
int max(int a, int b) {
    if (a > b)
        return a;
    else
        return b;
}

int min(int a, int b) {
    if (a < b)
        return a;
    else
        return b;
}

// === 4. 循环测试 ===
int sum_to_n(int n) {
    int s = 0;
    for (int i = 1; i <= n; i++)
        s += i;
    return s;
}

// === 5. 阶乘 (递归, 测试函数调用) ===
int factorial(int n) {
    if (n <= 1)
        return 1;
    return n * factorial(n - 1);
}

// === 6. 复杂表达式测试 ===
int complex_expr(int a, int b, int c) {
    return (a + b) * c - (a & b) | (c << 2);
}

// === 7. 全局变量测试 ===
int global_counter = 0;

void increment_counter(void) {
    global_counter++;
}

int get_counter(void) {
    return global_counter;
}

// === 8. 数组/指针测试 ===
int array_sum(int *arr, int n) {
    int s = 0;
    for (int i = 0; i < n; i++)
        s += arr[i];
    return s;
}

// === 9. 主函数 ===
int main(void) {
    int a = 10;
    int b = 3;
    int result = 0;

    result += add(a, b);           // 13
    result += sub(a, b);           // 7  → result=20
    result += and_op(a, b);        // 2  → result=22
    result += or_op(a, b);         // 11 → result=33
    result += xor_op(a, b);        // 9  → result=42
    result += shl(a, 1);           // 20 → result=62
    result += shr(a, 1);           // 5  → result=67
    result += max(a, b);           // 10 → result=77
    result += min(a, b);           // 3  → result=80
    result += sum_to_n(5);         // 15 → result=95
    result += factorial(5);        // 120 → result=215
    result += complex_expr(5, 3, 2); // (8)*2-(1)|(8) = 16-1|8 = 15|8 = 15 → result=230

    increment_counter();
    increment_counter();
    result += get_counter();       // 2  → result=232

    int nums[] = {1, 2, 3, 4, 5};
    result += array_sum(nums, 5);  // 15 → result=247

    return result;  // 预期返回 247
}
