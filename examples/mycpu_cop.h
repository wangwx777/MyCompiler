// ============================================================
// mycpu_cop.h — MyCPU Coprocessor (COP) Intrinsics
//
// The compiler recognizes these functions and replaces them
// with single COP instructions when all arguments are
// compile-time constants.
//
// Usage:
//   #include "mycpu_cop.h"
//   uint32_t val = __mycpu_cop_ld_w(/*cop_id=*/1, /*addr=*/0x100);
//   __mycpu_cop_st_w(/*src=*/val, /*cop_id=*/2, /*addr=*/0x200);
//
// Limits:
//   - cop_id and addr MUST be compile-time constants
//   - addr range: Word <= 1023, Half <= 255, Byte <= 63
//   - Non-constant args fall back to a regular call (linker error)
// ============================================================
#ifndef MYCPU_COP_H
#define MYCPU_COP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* COP Load — read from coprocessor */
uint32_t __mycpu_cop_ld_w(uint32_t cop_id, uint32_t addr);
uint16_t __mycpu_cop_ld_h(uint32_t cop_id, uint32_t addr);
uint8_t  __mycpu_cop_ld_b(uint32_t cop_id, uint32_t addr);

/* COP Store — write to coprocessor */
void __mycpu_cop_st_w(uint32_t src, uint32_t cop_id, uint32_t addr);
void __mycpu_cop_st_h(uint16_t src, uint32_t cop_id, uint32_t addr);
void __mycpu_cop_st_b(uint8_t  src, uint32_t cop_id, uint32_t addr);

#ifdef __cplusplus
}
#endif

#endif /* MYCPU_COP_H */
