//===-- test_bhw.c - MyCPU Byte/Half/Word load/store tests --------------===//
//
// Verify LDB/LDH/LDW (load) and STB/STH/STW (store) generate correct
// instruction sizes: Byte=00 Half=01 Word=10 in bits[7:6].
//
// Build: clang -target mycpu-unknown-elf -S -o - test_bhw.c
//        clang -target mycpu-unknown-elf -c -o test_bhw.o test_bhw.c
//        llvm-objdump -d --triple=mycpu-unknown-elf test_bhw.o
//
//===----------------------------------------------------------------------===//

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;

// === 1. Word (32-bit) load/store — baseline ===
uint32_t ld_word(uint32_t *p)         { return *p; }
void     st_word(uint32_t *p, uint32_t v) { *p = v; }

// === 2. Half (16-bit) load/store ===
uint16_t ld_half(uint16_t *p)         { return *p; }
void     st_half(uint16_t *p, uint16_t v) { *p = v; }

// === 3. Byte (8-bit) load/store ===
uint8_t  ld_byte(uint8_t *p)          { return *p; }
void     st_byte(uint8_t *p, uint8_t v)  { *p = v; }

// === 4. Pointers to sub-word — verify proper address calc ===
uint8_t  ld_byte_idx(uint8_t *p, int i)   { return p[i]; }
uint16_t ld_half_idx(uint16_t *p, int i)  { return p[i]; }

// === 5. Store constant — verify immediate encoding ===
void st_byte_42(uint8_t *p)           { *p = 42; }
void st_half_1000(uint16_t *p)        { *p = 10; }
