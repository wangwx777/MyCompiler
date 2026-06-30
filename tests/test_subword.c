//===-- test_subword.c - MyCPU sub-word memory access tests ------------===//
// 编译: clang -target mycpu-unknown-elf -S/-c -o out test_subword.c

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

// ---- 1. uint8_t 基本读写 ----
uint8_t g_byte;

uint8_t load_byte(uint8_t *p) {
    return *p;                  // LDB
}

void store_byte(uint8_t *p, uint8_t v) {
    *p = v;                     // STB
}

// ---- 2. uint16_t 基本读写 ----
uint16_t g_half;

uint16_t load_half(uint16_t *p) {
    return *p;                  // LDH
}

void store_half(uint16_t *p, uint16_t v) {
    *p = v;                     // STH
}

// ---- 3. uint8_t 数组访问 (循环,会产生分支) ----
uint8_t sum_bytes(uint8_t *arr, int n) {
    uint8_t sum = 0;
    for (int i = 0; i < n; i++) {
        sum += arr[i];          // LDB + 加法
    }
    return sum;
}

// ---- 4. volatile sub-word (直接生成 LDB/STB/LDH/STH) ----
uint8_t read_volatile_byte(volatile uint8_t *reg) {
    return *reg;
}

void write_volatile_byte(volatile uint8_t *reg, uint8_t val) {
    *reg = val;
}

uint16_t read_volatile_half(volatile uint16_t *reg) {
    return *reg;
}

void write_volatile_half(volatile uint16_t *reg, uint16_t val) {
    *reg = val;
}

// ---- 5. 结构体含 uint8_t/uint16_t 字段 ----
typedef struct {
    uint8_t  a;
    uint16_t b;
    uint8_t  c;
} MixedStruct;

uint8_t read_struct_small(MixedStruct *s) {
    return s->a;                // LDB offset 0
}

uint16_t read_struct_half(MixedStruct *s) {
    return s->b;                // LDH offset 2 (需要对齐)
}

void write_struct_small(MixedStruct *s, uint8_t x) {
    s->c = x;                   // STB offset 4
}

// ---- 6. 混合: 全局读写 uint8_t/uint16_t ----
void set_byte_global(uint8_t v) {
    g_byte = v;
}

void set_half_global(uint16_t v) {
    g_half = v;
}

uint8_t get_byte_global(void) {
    return g_byte;
}

uint16_t get_half_global(void) {
    return g_half;
}
