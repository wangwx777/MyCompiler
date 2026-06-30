//===-- MyCPUBaseInfo.h - Top-level definitions for MyCPU MC ----*- C++ -*-===//
//
// ★ MyCPU 目标的基础常量和辅助函数 ★
//
// 包含指令大小、size 字段编码、操作码提取等基本定义。
// 这些定义被 MC 层(汇编器/反汇编器/编码器)和 CodeGen 层共享。
//
// ★ 修改指令编码前缀(如增加 prefix byte)时：
//   1. 修改 InstSize
//   2. 添加新的 SizeField 枚举值(如有新 size 类型)
//   3. 修改 getSizeField() / getOpcode() 的位掩码
//   4. 同步修改 MyCPUInstrFormats.td 中的位布局
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MYCPU_MCTARGETDESC_MYCPUBASEINFO_H
#define LLVM_LIB_TARGET_MYCPU_MCTARGETDESC_MYCPUBASEINFO_H

#include "llvm/MC/MCInstrDesc.h"

namespace llvm {

namespace MyCPU {

// ★ 指令固定长度: 32 位 = 4 字节
// 如果改为可变长度，需要修改为 0(未知)并修改编码器
static const unsigned InstSize = 4;

// ★ Size 字段编码 (bits[7:6])
// 用于区分 Byte(8-bit)、Half(16-bit)、Word(32-bit) 操作
// 与 MyCPUInstrFormats.td 中的 size_bits 保持一致
enum SizeField {
  SF_Byte     = 0,  // 00 — 字节操作 (8-bit)
  SF_Half     = 1,  // 01 — 半字操作 (16-bit)
  SF_Word     = 2,  // 10 — 字操作 (32-bit)
  SF_Reserved = 3,  // 11 — 保留(可用于扩展)
};

// ★ 从 32 位指令编码中提取 size 字段 [7:6]
inline unsigned getSizeField(uint32_t Encoding) {
  return (Encoding >> 6) & 0x3;
}

// ★ 从 32 位指令编码中提取操作码 [5:0]
// 注意: 操作码只有 6 位(0-63)，如果超过需要扩展
// 如需扩展：可修改此函数读更多位，或使用保留位(如 bit[8])
inline unsigned getOpcode(uint32_t Encoding) {
  return Encoding & 0x3F;
}

} // namespace MyCPU
} // namespace llvm

#endif
