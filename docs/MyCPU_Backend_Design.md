# MyCPU LLVM Backend 设计与修改指南

> 版本: LLVM 23.0.0git | 最后更新: 2026-05-26
>
> 本文档详细描述 MyCPU 后端的指令格式、编码设计、各模块职责，以及如何修改指令结构。

---

## 目录

1. [架构总览](#1-架构总览)
2. [指令格式详解](#2-指令格式详解)
3. [TSFlags 编码方案](#3-tsflags-编码方案)
4. [指令编码流程](#4-指令编码流程)
5. [Fixup 系统 (重定位/修正)](#5-fixup-系统-重定位修正)
6. [如何添加新指令](#6-如何添加新指令)
7. [如何修改指令格式](#7-如何修改指令格式)
8. [如何修改操作码/字段位置](#8-如何修改操作码字段位置)
9. [子字加载/存储 (LDH/STH/LDB/STB)](#9-子字加载存储-ldhstHldbstb)
10. [已知 Bug 清单](#10-已知-bug-清单)
11. [代码导航索引](#11-代码导航索引)

---

## 1. 架构总览

### 1.1 编译流水线

```
C/C++ 源码
  │
  ▼
[Clang Frontend]  →  LLVM IR (中间表示)
  │
  ▼
[SelectionDAG]    →  DAG 优化 + 合法化(legalization)
  │
  ▼
[ISelDAGToDAG]    →  指令选择: SDNode → MachineInstr (MyCPU 指令)
  │
  ▼
[Register Alloc]  →  寄存器分配: 虚拟寄存器 → 物理寄存器 (r0-r31)
  │
  ▼
[Prologue/Epilogue] → 栈帧分配 (FrameLowering)
  │
  ▼
[AsmPrinter]      →  MachineInstr → MCInst (中间汇编表示)
  │                     ↓ lowerPseudoInstExpansion
  │                       伪指令展开 (LOAD_IMM → MOVIW 等)
  │
  ▼
[MCCodeEmitter]   →  MCInst → 32-bit 二进制指令字
  │  (手工编码, 未使用 TableGen 自动编码器)
  │
  ▼
[MCAsmBackend]    →  写入 .o 文件 (ELF object)
  │  applyFixup: 修正符号引用 (分支目标、全局变量地址)
  │
  ▼
.o / .s 文件
```

### 1.2 关键文件清单

| 文件 | 作用 | 修改频率 |
|------|------|---------|
| `MyCPUInstrFormats.td` | TableGen 指令格式定义 (字段布局、基类) | **高** — 改格式必改此处 |
| `MyCPUInstrInfo.td` | TableGen 指令定义 (opcode、汇编格式、Pat模式) | **高** — 加指令必改此处 |
| `MyCPUMCCodeEmitter.cpp` | C++ 手工编码器 (getBinaryCodeForInstr) | **高** — 与 .td 字段布局保持同步 |
| `MyCPUAsmBackend.cpp` | C++ 汇编后端 (applyFixup, 写 .o 文件) | **中** — fixup 逻辑修改 |
| `MyCPUAsmPrinter.cpp` | C++ 汇编打印 (伪指令展开) | **中** — 伪指令处理 |
| `MyCPUISelDAGToDAG.cpp` | C++ 指令选择 (DAG→MachineInstr) | **中** — 复杂指令匹配 |
| `MyCPUISelLowering.cpp` | C++ 合法化 (类型转换、调用约定) | **中** |
| `MyCPUFrameLowering.cpp` | C++ 栈帧分配 (prologue/epilogue) | **中** — 帧布局 |
| `MyCPUFixupKinds.h` | C++ header: fixup 种类枚举 | **低** — 除非加新 fixup 类型 |
| `MyCPUBaseInfo.h` | C++ header: 常量定义 | **低** |
| `MyCPURegisterInfo.td` | TableGen 寄存器定义 | **低** — 除非加寄存器 |
| `MyCPUCallingConv.td` | TableGen 调用约定 | **低** |
| `MyCPUELFStreamer.cpp` | C++ ELF 目标流化器 | **低** |
| `MyCPUInstPrinter.cpp` | C++ 反汇编打印器 | **低** — 加指令时更新 |

### 1.3 设计原则

- **32 位定长指令**: 所有指令固定 4 字节
- **小端序**: 低位字节在前 (和 x86 一致)
- **二操作数算术**: `rd = rd OP rs2[msb:lsb]`，rd 既是源又是目标
- **TableGen + 手工编码混合**: 格式定义用 TableGen，实际编码用 C++ (因为字段含义随指令变化)
- **三种操作宽度**: Word(32b), Half(16b), Byte(8b) 用相同的 opcode + 不同的 size 字段区分

---

## 2. 指令格式详解

### 2.1 32 位指令字整体结构

所有 32 位指令的最低 8 位是固定的:

```
bits[5:0]   = opcode (6-bit 硬件操作码)
bits[7:6]   = size   (2-bit 操作宽度)
                00 = Byte  (8-bit 操作)
                01 = Half  (16-bit 操作)
                10 = Word  (32-bit 操作)
                11 = 保留
```

高位 bits[31:8] 的布局随 **指令类型** 和 **操作宽度** 变化。

### 2.2 Word 格式 (size=10, 32-bit 操作)

```
 31  27 26  22 21  17 16  14 13    12   8  7   6  5      0
┌──────┬──────┬──────┬─────┬──┬─────────┬──────┬──────────┐
│fieldA│fieldB│fieldC│  f  │c │   rd    │ size │  opcode  │
│  5b  │  5b  │  5b  │ 3b  │1b│   5b    │  2b  │   6b     │
└──────┴──────┴──────┴─────┴──┴─────────┴──────┴──────────┘
  rs2/  msb/   lsb/  Freg 进位 目的寄存器  =10   硬件操作码
 immH  immM  immL  [0-7]
```

**字段宽度表**:

| 字段 | 位范围 | 宽度 | 含义 (RRR) | 含义 (RRI) | 含义 (LD) | 含义 (ST) |
|------|--------|------|-----------|-----------|----------|----------|
| fieldA | [31:27] | 5b | rs2 源寄存器 | imm[14:10] | base 基址 | src 源数据 |
| fieldB | [26:22] | 5b | msb 位域高位 | imm[9:5] | off[9:5] | base 基址 |
| fieldC | [21:17] | 5b | lsb 位域低位 | imm[4:0] | off[4:0] | off[4:0] |
| f | [16:14] | 3b | Freg 索引 | Freg 索引 | 0 | 0 |
| c | [13] | 1b | 进位输入 | 进位输入 | 0 | 0 |
| rd | [12:8] | 5b | 目的/源寄存器 | 目的寄存器 | 目的寄存器 | off[9:5] ★ |

★ **ST 的特殊设计**: ST 无目标寄存器，rd 字段被复用为 offset[9:5]。

### 2.3 Half 格式 (size=01, 16-bit 操作)

```
 31  26 25  22 21  18 17  15 14    13   8  7   6  5      0
┌──────┬──────┬──────┬─────┬──┬─────────┬──────┬──────────┐
│fieldA│fieldB│fieldC│  f  │c │   rd    │ size │  opcode  │
│  6b  │  4b  │  4b  │ 3b  │1b│   6b    │  2b  │   6b     │
└──────┴──────┴──────┴─────┴──┴─────────┴──────┴──────────┘
```

| 字段 | 位范围 | 宽度 | 说明 |
|------|--------|------|------|
| fieldA | [31:26] | 6b | rs2 (64个半字寄存器) 或 imm[13:8] |
| fieldB | [25:22] | 4b | msb (0-15) 或 imm[7:4] |
| fieldC | [21:18] | 4b | lsb (0-15) 或 imm[3:0] |
| f | [17:15] | 3b | Freg 索引 |
| c | [14] | 1b | 进位输入 |
| rd | [13:8] | 6b | 目的寄存器 |
| size | [7:6] | 2b | = 01 |
| opcode | [5:0] | 6b | 操作码 |

### 2.4 Byte 格式 (size=00, 8-bit 操作)

```
 31  25 24  22 21  19 18  16 15    14   8  7   6  5      0
┌──────┬──────┬──────┬─────┬──┬─────────┬──────┬──────────┐
│fieldA│fieldB│fieldC│  f  │c │   rd    │ size │  opcode  │
│  7b  │  3b  │  3b  │ 3b  │1b│   7b    │  2b  │   6b     │
└──────┴──────┴──────┴─────┴──┴─────────┴──────┴──────────┘
```

| 字段 | 位范围 | 宽度 | 说明 |
|------|--------|------|------|
| fieldA | [31:25] | 7b | rs2 (128个字节寄存器) 或 imm[13:7] |
| fieldB | [24:22] | 3b | msb (0-7) 或 imm[6:4] |
| fieldC | [21:19] | 3b | lsb (0-7) 或 imm[3:1] ★ |
| f | [18:16] | 3b | Freg 索引 |
| c | [15] | 1b | 进位输入 |
| rd | [14:8] | 7b | 目的寄存器 |
| size | [7:6] | 2b | = 00 |
| opcode | [5:0] | 6b | 操作码 |

★ Byte 格式 imm 隐含 bit0=0 (即 imm 为偶数)，实际可表示 14 位有符号值。

### 2.5 三格式寄存器数量对比

| 格式 | size | rd 宽度 | 寄存器数 | fieldA/rs2 | msb/lsb |
|------|------|---------|---------|-----------|---------|
| Byte | 00 | 7-bit [14:8] | 128 | 7-bit [31:25] | 3-bit each |
| Half | 01 | 6-bit [13:8] | 64 | 6-bit [31:26] | 4-bit each |
| Word | 10 | 5-bit [12:8] | 32 | 5-bit [31:27] | 5-bit each |

### 2.6 小端字节序

32 位指令 `0x00100483` 在内存中的字节排列:

```
地址偏移  │ +0   +1   +2   +3
─────────┼────────────────────
字节内容  │ 0x83 0x04 0x10 0x00
对应位   │[7:0][15:8][23:16][31:24]
```

字节 0 永远包含 opcode[5:0] + size[7:6]。

---

## 3. TSFlags 编码方案

### 3.1 为什么需要 TSFlags

`MI.getOpcode()` 返回的是 TableGen 内部分配的枚举 ID（如 `MyCPU::ADDW = 387`），**不是**硬件操作码。硬件操作码存在 TSFlags 中。

### 3.2 TSFlags 位分配

TSFlags 是 64 位字段，当前使用情况:

```
TSFlags[63:14]  = 0 (未使用)
TSFlags[13:8]   = 硬件 opcode (6-bit, 0x00-0x3F)
TSFlags[7:6]    = size 字段 (2-bit, 0=Byte 1=Half 2=Word)
TSFlags[5:0]    = 0 (未使用)
```

### 3.3 提取方法 (在 MCCodeEmitter 中)

```cpp
uint8_t HWOpcode = (Desc.TSFlags >> 8) & 0x3F;   // 硬件操作码
unsigned SizeField = (Desc.TSFlags >> 6) & 0x3;   // 操作宽度
uint32_t Binary = HWOpcode | (SizeField << 6);     // 构造指令字低字节
```

### 3.4 TableGen 侧设置 (MyCPUInstrFormats.td)

```tablegen
class MyCPUInst<...> {
  let TSFlags{13-8} = opcode;   // 硬件操作码 → TSFlags
  let TSFlags{7-6}  = 0b10;     // 默认 Word (子类覆盖)
}

class InstW<...> : MyCPUInst<...> {
  let TSFlags{7-6} = 0b10;      // Word 格式标识
}
class InstH<...> : MyCPUInst<...> {
  let TSFlags{7-6} = 0b01;      // Half 格式标识
}
class InstB<...> : MyCPUInst<...> {
  let TSFlags{7-6} = 0b00;      // Byte 格式标识
}
```

---

## 4. 指令编码流程

### 4.1 MCCodeEmitter 入口

```cpp
void encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &CB,
                       SmallVectorImpl<MCFixup> &Fixups,
                       const MCSubtargetInfo &STI) const;

uint32_t getBinaryCodeForInstr(const MCInst &MI,
                                SmallVectorImpl<MCFixup> &Fixups,
                                const MCSubtargetInfo &STI) const;
```

### 4.2 getBinaryCodeForInstr 编码逻辑

```
1. 从 TSFlags 提取 HWOpcode 和 SizeField
2. Binary = HWOpcode | (SizeField << 6)   ← 设定 opcode + size
3. 根据 SizeField 选择字段宽度:
   Word(2): RdMask=0x1F RdShift=8  MsbMask=0x1F MsbShift=22 ...
   Half(1): RdMask=0x3F RdShift=8  MsbMask=0xF  MsbShift=22 ...
   Byte(0): RdMask=0x7F RdShift=8  MsbMask=0x7  MsbShift=22 ...
4. 按 HWOpcode 分发不同编码逻辑:
   - 0x14 (LD): rd + base + offset
   - 0x15 (ST): src + base + offset
   - 0x18/0x1A/0x1B/0x1E (BR/CALL): offset 或 f+offset
   - default (RRR/RRI/...): 按操作数索引分别处理
5. 对于表达式操作数 (isExpr()): 创建 MCFixup，不写立即数位
6. 返回 32-bit Binary
```

### 4.3 操作数索引约定

所有指令格式中操作数顺序:

| 索引 | 类型 | 含义 | 编码位置 |
|------|------|------|---------|
| [0] | reg | rd (目的寄存器) | 对应格式的 rd 字段 |
| [1] | reg/imm | rs2 或 imm 高位 | fieldA |
| [2] | imm | msb / imm 中位 | fieldB |
| [3] | imm | lsb / imm 低位 | fieldC |
| [4] | imm | f (Freg 索引, 0-7) | f 字段 |
| [5] | imm | c_bit (进位输入) | c_bit 字段 |

### 4.4 encodeInstruction 输出

```cpp
// 小端序: 低字节在前
for (unsigned i = 0; i < 4; ++i)
    CB.push_back(static_cast<char>((Bits >> (i * 8)) & 0xFF));
// 例: Bits = 0x00100483 → CB = [0x83, 0x04, 0x10, 0x00]
```

---

## 5. Fixup 系统 (重定位/修正)

### 5.1 什么指令需要 Fixup

当指令包含**编译时未知的值**（如分支目标偏移、全局变量地址），编码器创建一个 MCFixup，由汇编器/链接器稍后填充。

```cpp
// 分支指令: JMP target_label → 目标地址编译时未知
JMP  .L1    → addFixup(i, MyCPU::fixup_MyCPU_PCRel_16)

// 加载全局地址: ld r1, [r2 + global_var] → 地址编译时未知
LDW  r1, [r2 + global_var]  → addFixup(i, MyCPU::fixup_MyCPU_16)
```

### 5.2 Fixup 种类 (MyCPUFixupKinds.h)

| Kind | 值 | 用途 | Bit 范围 |
|------|-----|------|---------|
| `fixup_MyCPU_32` | 0 | 32-bit 绝对地址 | 整个指令 |
| `fixup_MyCPU_16` | 1 | 16-bit 绝对字段 | bits[31:17] |
| `fixup_MyCPU_PCRel_16` | 2 | PC 相对偏移 | bits[31:17] |

### 5.3 applyFixup 工作流程

```
汇编器侧 (MCAssembler.cpp):
  1. 计算 Data = Contents.data() + Fixup.getOffset()
     ★ Data 已经偏移到 fixup 所在的指令起始位置
  2. 调用 applyFixup(F, Fixup, Target, Data, Value, IsResolved)

applyFixup 内部:
  1. adjustFixupValue: 对 PC 相对 fixup 减去 2 (PC 偏置)
  2. 按 Kind 分支:
     fixup_MyCPU_16 / PCRel_16:
       Data[2] |= (V[6:0] << 1) & 0xFE    ← 保留 bit0 (f[2])
       Data[3] |= V[14:7]
     fixup_MyCPU_32:
       Data[0..3] |= Value[31:0]
```

### 5.4 Fixup 值的位布局

15-bit 值 V[14:0] 映射到 bits[31:17]:

```
指令位:   31   27   22   17
          ├────┼────┼────┤
          │fieldA│fieldB│fieldC│
          │ 5b   │ 5b   │ 5b   │
          ├────┼────┼────┤
V bits:  [14:10][9:5] [4:0]

在字节中的位置 (小端序):
  Data[2] = byte[23:16]:
    bit0 = f[2] (保留!)
    bit1 = V[0] = fieldC[0] = bit17
    bit2 = V[1] = fieldC[1] = bit18
    bit3 = V[2] = fieldC[2] = bit19
    bit4 = V[3] = fieldC[3] = bit20
    bit5 = V[4] = fieldC[4] = bit21
    bit6 = V[5] = fieldB[0] = bit22
    bit7 = V[6] = fieldB[1] = bit23

  Data[3] = byte[31:24]:
    bit0 = V[7]  = fieldB[2] = bit24
    bit1 = V[8]  = fieldB[3] = bit25
    bit2 = V[9]  = fieldB[4] = bit26
    bit3 = V[10] = fieldA[0] = bit27
    bit4 = V[11] = fieldA[1] = bit28
    bit5 = V[12] = fieldA[2] = bit29
    bit6 = V[13] = fieldA[3] = bit30
    bit7 = V[14] = fieldA[4] = bit31
```

### 5.5 FixupKindInfo 配置

```cpp
// MyCPUAsmBackend.cpp: getFixupKindInfo()
// 格式: {name, TargetOffset, TargetSize, Flags}
{"fixup_MyCPU_32",       0, 32, 0},   // 整个指令, bit0开始, 32-bit
{"fixup_MyCPU_16",       0, 16, 0},   // bit0开始, 16-bit (实际是15-bit在[31:17])
{"fixup_MyCPU_PCRel_16", 0, 16, 0},   // 同上, PC相对
```

### 5.6 ELF 重定位类型

```cpp
// MyCPUELFStreamer.cpp
enum {
  R_MYCPU_NONE    = 0,
  R_MYCPU_32      = 1,   // 对应 fixup_MyCPU_32
  R_MYCPU_16      = 2,   // 对应 fixup_MyCPU_16
  R_MYCPU_PCRel_16 = 3,  // 对应 fixup_MyCPU_PCRel_16
};
```

---

## 6. 如何添加新指令

### 6.1 示例: 添加一条 MUL 指令

假设要添加 `MUL rd, rs2[msb:lsb]` (opcode=0x30):

**步骤 1**: 在 `MyCPUInstrInfo.td` 中添加指令定义

```tablegen
// 全字版
def MULW : InstW_RRR<0x30, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u5imm:$msb, u5imm:$lsb),
  "mul.w\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0;
  let c_bit = 0;
}
def : Pat<(mul i32:$rd, i32:$rs2), (MULW GPR:$rd, GPR:$rs2, 31, 0)>;

// 半字版
def MULH : InstH_RRR<0x30, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u4imm:$msb, u4imm:$lsb),
  "mul.h\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(mul i16:$rd, i16:$rs2), (MULH GPR:$rd, GPR:$rs2, 15, 0)>;

// 字节版
def MULB : InstB_RRR<0x30, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u3imm:$msb, u3imm:$lsb),
  "mul.b\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(mul i8:$rd, i8:$rs2), (MULB GPR:$rd, GPR:$rs2, 7, 0)>;
```

**步骤 2**: 更新 `MyCPUInstrInfo.td` 开头注释的 opcode 表

```
//   0x30 MUL    ...
```

**步骤 3**: 检查 MCCodeEmitter 是否需要修改

如果 MUL 是 RRR 格式，且操作数布局与 ADD/SUB 一致 (rd, rs2, msb, lsb)，则 **不需要修改** emitter — 它会走 default 分支，按操作数索引自动处理:

```cpp
// emitter 的 default 分支:
case 0: // rd
    if (Reg != 0) Binary |= ((Reg & RdMask) << RdShift);
    break;
case 1: // rs2 (fieldA)
    Binary |= ((Reg & RsMask) << RsShift);
    break;
case 2: // msb (fieldB)
    Binary |= ((Imm & MsbMask) << MsbShift);
    break;
case 3: // lsb (fieldC)
    Binary |= ((Imm & LsbMask) << LsbShift);
    break;
```

只有以下情况才需要修改 emitter:
- 新指令的操作数布局与现有模式不同 (如 ST 把 offset 分到 rd 和 fieldC)
- 新指令有特殊的字段复用逻辑

**步骤 4**: 重新编译

```bash
ninja -C build LLVMMyCPUCodeGen
```

### 6.2 可用的基类速查

| 如需... | 使用基类 | 操作数 | 字段映射 |
|--------|---------|--------|---------|
| 寄存器-寄存器算术 | `InstW_RRR` (Word) | rd, rs2, msb, lsb | fieldA=rs2, fieldB=msb, fieldC=lsb |
| 寄存器-立即数算术 | `InstW_RRI` (Word) | rd, imm15 | fieldA/B/C = imm[14:0] 三段 |
| 寄存器移位 | `InstW_SHIFT` (Word) | rd, rs2 | fieldA=rs2, fieldB/C=0 |
| 立即数移位 | `InstW_SHIFTI` (Word) | rd, shamt | fieldA=shamt |
| 寄存器传送 | `InstW_MOV` (Word) | rd, rs2 | fieldA=rs2, fieldB=31, fieldC=0 |
| 无条件跳转 | `InstW_BR` (Word) | target | fieldA/B/C = offset[14:0] |
| 条件跳转 | `InstW_BR` (Word) | f, target | f=f, fieldA/B/C=offset |
| 字加载 | `InstW_LD` (Word) | rd, base, off | fieldA=base, fieldB/C=off |
| 字存储 | `InstW_ST` (Word) | src, base, off | fieldA=src, fieldB=base, fieldC/rd=off |
| 自由格式 | `InstW` (Word) | 自定义 | 手动设 fieldA/B/C/rd/f/c |
| Half 版 (以上任一种) | `InstH_XXX` | 同上 | 同逻辑，不同位宽 |
| Byte 版 (以上任一种) | `InstB_XXX` | 同上 | 同逻辑，不同位宽 |
| 伪指令 | `Pseudo` | 自定义 | 无硬件编码 |

---

## 7. 如何修改指令格式

### 7.1 修改须知: 需要同时修改两处

每次修改字段位置 (如把 rd 从 [12:8] 移到 [15:11])，必须同时更新:

1. **TableGen** (`MyCPUInstrFormats.td`): `let Inst{12-8} = rd;` → `let Inst{15-11} = rd;`
2. **MCCodeEmitter** (`MyCPUMCCodeEmitter.cpp`): `RdShift = 8` → `RdShift = 11`

**两处不一致 = 汇编文本 (.s) 正确但机器码 (.o) 错误**

### 7.2 示例: 把 Word 格式的 rd 字段从 [12:8] 移到 [11:7]

**TableGen 侧** — 修改 `InstW` class:

```tablegen
class InstW<...> {
  bits<5> rd = 0;
  // let Inst{12-8} = rd;   ← 旧位置
  let Inst{11-7} = rd;      // ★ 新位置
}
```

**MCCodeEmitter 侧** — 修改 Word 格式的 shift 值:

```cpp
case 2: // Word
  RdMask = 0x1F;
  // RdShift = 8;            ← 旧值
  RdShift = 7;               // ★ 新值
  break;
```

### 7.3 示例: 扩大立即数字段宽度

假设要把 Word 格式的 15-bit 立即数扩展为 18-bit:

**分析影响**: 立即数占 fieldA(5b) + fieldB(5b) + fieldC(5b) = 15-bit。
要扩展为 18-bit，需要从其他地方"借"3 位。可能的方案:
- 缩小 f (3b→2b)，借 1 位
- 去掉 c_bit，借 1 位
- 缩小 rd (5b→4b)，借 1 位

假设: f 从 3b 缩到 2b [16:15], c_bit 从 [13] 移到 [14], imm 扩展:

```
新布局:  [31:27] fieldA 5b
         [26:22] fieldB 5b
         [21:17] fieldC 5b
         [16:15] f 2b
         [14]    c_bit
         [14:8]  imm[17:15] 3b  ← 3 个新 bit 放在原 f[0] 和 c_bit 位置
         [12:8]  rd 5b  ← 被挤占了！
```

不对，这样 rd 就和 imm 冲突了。换个思路:

**更好方案**: imm[17:15] 放在 fieldA/B/C 各自拓展 1 位

```
简化方案 — 都缩 1 位，imm 拓 3 位:
  fieldA 5b → 不变
  fieldB 5b → 不变
  fieldC 5b → 不变
  imm[17:15] 复用 rd[2:0] → 不可行，rd 需要 5 位

实用方案 — 改操作数语义:
  imm18 拆分为 movi.w (高 15b) + ori.w (低 3b) 两条指令序列
  (不改指令格式，由 LLVM 自动完成拆分)
```

**结论**: 大范围修改位宽通常不划算，建议保持现有格式，依赖 LLVM 的多指令展开 (legalization)。

### 7.4 Half/Byte 格式的 ld/st offset 字段

当前 Half 和 Byte 格式的 ST/LD 使用独立的格式类:

| 指令 | 格式 | base 宽 | offset 宽 | 备注 |
|------|------|---------|----------|------|
| STW/LDW | InstW_ST/InstW_LD | 5b | 10b (-512~511) | offset 拆分到 fieldB+fieldC+rd |
| STH/LDH | InstH_ST/InstH_LD | 6b | 8b (-128~127) | fieldB(4b)+fieldC(4b) |
| STB/LDB | InstB_ST/InstB_LD | 7b | 6b (-32~31) | fieldB(3b)+fieldC(3b) |

---

## 8. 如何修改操作码/字段位置

### 8.1 修改已有指令的操作码

只需改 `.td` 文件一个地方:

```tablegen
// MyCPUInstrInfo.td: 把 ADD 从 0x00 改为 0x28
def ADDW : InstW_RRR<0x28, ...>  // ← 只改这个数字
```

因为 TSFlags 自动从 opcode 参数填充 (`let TSFlags{13-8} = opcode`)。

### 8.2 修改一个字段的位置 (以 c_bit 为例)

假设把 Word 格式的 c_bit 从 [13] 移到 [17]：

**TableGen** (MyCPUInstrFormats.td):
```tablegen
// InstW class
let Inst{13}    = c_bit;   // 旧 → 删除或改为 0
let Inst{17}    = c_bit;   // 新
```

**MCCodeEmitter** (MyCPUMCCodeEmitter.cpp):
```cpp
// Word 格式
CBitShift = 17;  // 原来是 13
```

**同时**需要调整 fieldC 的位置，因为 bit17 原本属于 fieldC[0]:
- fieldC 从 5b [21:17] 缩为 4b [21:18]
- 或 c_bit 放别的地方

### 8.3 操作码分配表

| Opcode | 助记符 | 类型 | 说明 |
|--------|--------|------|------|
| 0x00 | ADD | RRR | 加法 |
| 0x01 | ADDI | RRI | 立即数加法 |
| 0x02 | SUB | RRR | 减法 |
| 0x03 | SUBI | RRI | 立即数减法 |
| 0x04 | AND | RRR | 按位与 |
| 0x05 | ANDI | RRI | 立即数与 |
| 0x06 | OR | RRR | 按位或 |
| 0x07 | ORI | RRI | 立即数或 |
| 0x08 | XOR | RRR | 按位异或 |
| 0x09 | XORI | RRI | 立即数异或 |
| 0x0A | NOT | RRR | 按位取反 |
| 0x0C | SHL | SHIFT | 左移 (寄存器移位量) |
| 0x0D | SHLI | SHIFTI | 左移 (立即数移位量) |
| 0x0E | SHR | SHIFT | 逻辑右移 |
| 0x0F | SHRI | SHIFTI | 逻辑右移立即数 |
| 0x10 | SAR | SHIFT | 算术右移 |
| 0x11 | SARI | SHIFTI | 算术右移立即数 |
| 0x12 | MOV | MOV | 寄存器传送 |
| 0x13 | MOVI | RRI | 加载立即数 |
| 0x14 | LD | LD | 内存加载 |
| 0x15 | ST | ST | 内存存储 |
| 0x16 | CMP | RRR | 比较 (寄存器) |
| 0x17 | CMPI | RRI | 比较 (立即数) |
| 0x18 | JMP | BR | 无条件跳转 |
| 0x19 | JALR | RRR | 间接跳转并链接 |
| 0x1A | BZ | BR | Freg.Z==1 跳转 |
| 0x1B | BNZ | BR | Freg.Z==0 跳转 |
| 0x1E | CALL | BR | 函数调用 |
| 0x1F | RET | — | 函数返回 |
| 0x22 | BSET | — | 位设置 |
| 0x23 | BCLR | — | 位清除 |
| 0x24 | BNOT | — | 位翻转 |
| 0x25 | BTST | — | 位测试 |
| 0x26 | NOP | — | 空操作 |
| 0x27 | HALT | — | 停机 |
| **0x28-0x3F** | — | — | ★ 可用 |

---

## 9. 子字加载/存储 (LDH/STH/LDB/STB)

### 9.1 已定义的指令

```tablegen
// Half (16-bit)
def LDH : InstH_LD<0x14, (outs GPR:$rd), (ins GPR:$base, i32imm:$offset), ...>;
def STH : InstH_ST<0x15, (outs), (ins GPR:$src, GPR:$base, i32imm:$offset), ...>;
def : Pat<(i16 (load ...)), (LDH ...)>;
def : Pat<(store (i16 ...)), (STH ...)>;

// Byte (8-bit)
def LDB : InstB_LD<0x14, (outs GPR:$rd), (ins GPR:$base, i32imm:$offset), ...>;
def STB : InstB_ST<0x15, (outs), (ins GPR:$src, GPR:$base, i32imm:$offset), ...>;
def : Pat<(i8 (load ...)), (LDB ...)>;
def : Pat<(store (i8 ...)), (STB ...)>;
```

### 9.2 已验证: STB/STH/STW 编码正确

```
st.b r7, [r8 + 0]  →  raw: 15 08 00 0e  → byte0=0x15 → size=00 (Byte) ✓
st.h r7, [r8 + 0]  →  raw: 55 08 00 1c  → byte0=0x55 → size=01 (Half) ✓
st.w r7, [r0 + 256] → raw: 95 00 00 3a  → byte0=0x95 → size=10 (Word) ✓
```

### 9.3 当前限制: ISel 对 sub-word store 的 trunc 问题

```
fatal error: Cannot select:
  ch = store<(store (s8) into %ir.5), trunc to i8>
```

**原因**: `*p = *p + 1` (uint8_t) 产生 i32→i8 trunc，ISel 无法匹配。

**已有 Pattern 但未覆盖所有情况**: TableGen 中有 trunc pattern:
```tablegen
def : Pat<(i8 (trunc i32:$src)), (MOVB GPR:$src)>;
def : Pat<(i8 (trunc i16:$src)), (MOVB GPR:$src)>;
def : Pat<(i16 (trunc i32:$src)), (MOVH GPR:$src)>;
```

但组合模式 `store(trunc(add(load, 1)))` 没有被匹配到，需要通过 ISelLowering 做自定义 legalization。

---

## 10. 已知 Bug 清单

### 10.1 严重/阻塞 (导致 crash 或无法编译)

| # | 问题 | 症状 | 文件 |
|---|------|------|------|
| 1 | **帧栈分配错误** | `subi.w r4, r4, 2462752620112` (巨大帧偏移) | FrameLowering.cpp |
| 2 | **ISel trunc store** | `Cannot select: store ... trunc to i8` | 需要 ISelLowering |
| 3 | **大立即数加载失败** | `*p = 0x12345` 直接从栈取垃圾值 | 需要 ExpandPseudo |

### 10.2 中等/功能 (编译通过但语义错误)

| # | 问题 | 症状 | 文件 |
|---|------|------|------|
| 4 | **PC 相对偏移** `Value -= 2` | 分支跳转目标可能不正确 | AsmBackend.cpp |
| 5 | **LDH/LDB 未触发** | `ld_byte(p)` 生成 `ld.w` 而非 `ld.b` | ISel |
| 6 | **CALL 无链接** | CALL 不保存返回地址 (LR) | 需 ISA 设计 |

### 10.3 轻微/美化

| # | 问题 | 症状 | 文件 |
|---|------|------|------|
| 7 | **反汇编器显示错误** | `addi.w r4, r4, 0` 实际 offset 不是 0 | Disassembler |
| 8 | **size 字段不一致** | 反汇编总显示 .w，实际部分指令 size=Byte | Disassembler |

---

## 11. 代码导航索引

### 11.1 添加新指令需要改哪些文件

```
★ 必须改:
  MyCPUInstrFormats.td     — 如果新格式 → 新基类; 如果新字段布局 → 改 InstW/H/B
  MyCPUInstrInfo.td        — def 指令定义 + Pat 模式

★ 可能需要改:
  MyCPUMCCodeEmitter.cpp   — 如果新指令操作数布局与现有模式不同
  MyCPUBaseInfo.h          — 添加注释 (opcode 说明)
  MyCPUInstPrinter.cpp     — 如果输出格式特殊

★ 一般不需要改:
  MyCPUAsmBackend.cpp      — 除非有新 fixup 类型
  MyCPUISelLowering.cpp    — 除非有复杂类型转换
  MyCPUISelDAGToDAG.cpp    — 除非有复杂指令选择
```

### 11.2 修改指令位布局需要改哪些文件

```
★ 必须同步修改 (两处必须一致):
  MyCPUInstrFormats.td     — let Inst{X-Y} = field;
  MyCPUMCCodeEmitter.cpp   — RdShift, RsShift, MsbShift, LsbShift 等常量

★ 可能需要改:
  MyCPUInstrInfo.td        — 如果字段含义变了 (如 rd 5b→6b)
  MyCPUInstPrinter.cpp     — 如果打印格式变了
  MyCPUAsmBackend.cpp      — 如果 fixup 位范围变了
  Disassembler             — 如果有反汇编器
```

### 11.3 调试建议

```bash
# 1. 看汇编输出 (检查 ISel 和 CodeGen)
./build/bin/clang -target mycpu-unknown-elf -S -o - test.c

# 2. 看最终机器码 (检查 Emitter + AsmBackend)
./build/bin/clang -target mycpu-unknown-elf -c -o test.o test.c
./build/bin/llvm-objdump -d --triple=mycpu-unknown-elf test.o

# 3. 看 section 原始 hex (手工解码)
./build/bin/llvm-objdump -s --triple=mycpu-unknown-elf -j .text test.o

# 4. 看重定位表 (检查 fixup 是否正确记录)
./build/bin/llvm-readobj --relocations test.o

# 5. 用 llvm-mc 单独测试汇编器
echo "JMP _start" | ./build/bin/llvm-mc -triple=mycpu-unknown-elf \
    -show-encoding -show-inst
```

### 11.4 编译单个组件

```bash
# 只编译 MyCPU 后端 (不重链接)
ninja -C build LLVMMyCPUCodeGen

# 编译 + 链接 clang (完整测试)
ninja -C build clang

# 编译 + 链接 llvm-mc (只测试汇编器)
ninja -C build llvm-mc
```
