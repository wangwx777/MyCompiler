// Test: immediate values in range vs out of range
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;

// 小立即数 — 都在范围内
void stb_small(uint8_t *p)  { *p = 10; }     // 10 在 15-bit 内
void sth_small(uint16_t *p) { *p = 100; }    // 100 在 15-bit 内
void stw_small(int *p)      { *p = 1000; }   // 1000 在 15-bit 内

// 大立即数 — 超出 15-bit 范围 (movi.w 只能装 15-bit signed)
void stw_big(int *p)        { *p = 0x12345; } // 74565 超出 15-bit
