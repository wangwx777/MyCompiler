# MyCPU 理论基础与源码对应

> 本文档梳理 MyCPU 指令集架构的理论基础，并精确对应到 LLVM 后端源码的每个关键位置。

---

## 目录

1. [整体架构概览](#1-整体架构概览)
2. [指令集编码格式](#2-指令集编码格式)
3. [寄存器文件](#3-寄存器文件)
4. [指令集详解](#4-指令集详解)
5. [调用约定](#5-调用约定)
6. [栈帧布局](#6-栈帧布局)
7. [编译流水线](#7-编译流水线)
8. [源码文件索引](#8-源码文件索引)

---

## 1. 整体架构概览

### 1.1 CPU 基本特性

| 特性 | 值 | 源码位置 |
|------|-----|----------|
| 指令长度 | 32-bit 定长 | `MCTargetDesc/MyCPUBaseInfo.h:27` — `InstSize = 4` |
| 端序 | 小端 (Little-Endian) | `frontend/MyCPU.h:26` — `BigEndian = false` |
| 地址总线 | 32-bit | `frontend/MyCPU.h:35` — `PointerWidth = 32` |
| 寄存器宽度 | 32-bit | `MyCPURegisterInfo.td:20` — `let Size = 32` |
| 栈增长方向 | 向下（低地址） | `MyCPUFrameLowering.cpp:58` — `StackGrowsDown` |
| 栈对齐 | 4 字节 | `MyCPUFrameLowering.cpp:58` — `Align(4)` |

### 1.2 编译器工具链结构

```
C 源码 (.c)
    │
    ▼
┌──────────────────────────────────────┐
│  clang (frontend/MyCPU.{h,cpp})      │  ← 定义目标三元组、类型宽度、预定义宏
│  → LLVM IR                           │
└──────────────────────────────────────┘
    │
    ▼
┌──────────────────────────────────────┐
│  LLVM 优化器 (与目标无关)              │
└──────────────────────────────────────┘
    │
    ▼
┌──────────────────────────────────────┐
│  MyCPU 后端 (backend/MyCPU/)          │
│  ├── ISelLowering (合法化+降级)        │
│  ├── ISelDAGToDAG (指令选择)           │
│  ├── FrameLowering (栈帧管理)          │
│  ├── RegisterInfo (寄存器分配)         │
│  ├── InstrInfo (指令操作)              │
│  └── AsmPrinter (汇编输出)             │
│  → MachineInstr                      │
└──────────────────────────────────────┘
    │
    ▼
┌──────────────────────────────────────┐
│  MC 层 (MCTargetDesc/)                │
│  ├── MCCodeEmitter (二进制编码)        │
│  ├── AsmParser (汇编文本→二进制)       │
│  ├── Disassembler (二进制→汇编文本)    │
│  └── InstPrinter (汇编打印)            │
│  → .o 目标文件 / .s 汇编文件           │
└──────────────────────────────────────┘
```

---

## 2. 指令集编码格式

### 2.1 固定字段

32 位指令中，**低 8 位固定不变**：

| 位域 | 位置 | 宽度 | 含义 |
|------|------|------|------|
| opcode | `[5:0]` | 6 bit | 操作码，标识指令功能 |
| size | `[7:6]` | 2 bit | 操作宽度: `00`=Byte, `01`=Half, `10`=Word, `11`=保留 |

**源码关键位置：**

| 内容 | 文件 |
|------|------|
| Size 枚举定义 | `MCTargetDesc/MyCPUBaseInfo.h:32-37` — `enum SizeField` |
| 提取 size 字段 | `MCTargetDesc/MyCPUBaseInfo.h:40-42` — `getSizeField()` |
| 提取 opcode | `MCTargetDesc/MyCPUBaseInfo.h:47-49` — `getOpcode()` |
| TableGen size_bits 声明 | `MyCPUInstrFormats.td:101-102` — `bits<2> size_bits` |

### 2.2 Word 格式 (size=10, 32-bit 操作)

```
 31   27  26   22  21   17  16 14  13  12    8  7  6  5     0
┌───────┬───────┬───────┬──────┬───┬───────┬─────┬────┬───────┐
│fieldA │fieldB │fieldC │  f   │c │  rd   │size │ opcode    │
│  5b   │  5b   │  5b   │  3b  │1b│  5b   │ 10  │ 0x00-0x3F │
└───────┴───────┴───────┴──────┴───┴───────┴─────┴────┴───────┘

 RRR (寄存器版):  fieldA=rs2[4:0],  fieldB=msb[4:0],  fieldC=lsb[4:0]
 RRI (立即数版):  fieldA=imm[14:10], fieldB=imm[9:5],  fieldC=imm[4:0]
 BR  (分支):      fieldA+B+C = 15-bit PC相对偏移
```

**源码位置：**

| 内容 | 文件 |
|------|------|
| InstW 基类定义 | `MyCPUInstrFormats.td:124-143` |
| InstW_RRR (寄存器-寄存器) | `MyCPUInstrFormats.td:218-229` |
| InstW_RRI (寄存器-立即数) | `MyCPUInstrFormats.td:234-243` |
| InstW_SHIFT (寄存器移位) | `MyCPUInstrFormats.td:248-257` |
| InstW_SHIFTI (立即数移位) | `MyCPUInstrFormats.td:262-272` |
| InstW_LD/ST (访存) | `MyCPUInstrFormats.td:301-331` |
| InstW_BR (分支) | `MyCPUInstrFormats.td:289-297` |
| 编码器 Word 布局 | `MCTargetDesc/MyCPUMCCodeEmitter.cpp:110-117` |

### 2.3 Half 格式 (size=01, 16-bit 操作)

```
 31   26  25   22  21   18  17 15  14  13     8  7  6  5     0
┌───────┬───────┬───────┬──────┬───┬───────┬─────┬────┬───────┐
│fieldA │fieldB │fieldC │  f   │c │  rd   │size │ opcode    │
│  6b   │  4b   │  4b   │  3b  │1b│  6b   │ 01  │ 0x00-0x3F │
└───────┴───────┴───────┴──────┴───┴───────┴─────┴────┴───────┘
```

**源码位置：** `MyCPUInstrFormats.td:157-176` — InstH 基类

### 2.4 Byte 格式 (size=00, 8-bit 操作)

```
 31   25  24   22  21   19  18 16  15  14      8  7  6  5     0
┌───────┬───────┬───────┬──────┬───┬───────┬─────┬────┬───────┐
│fieldA │fieldB │fieldC │  f   │c │  rd   │size │ opcode    │
│  7b   │  3b   │  3b   │  3b  │1b│  7b   │ 00  │ 0x00-0x3F │
└───────┴───────┴───────┴──────┴───┴───────┴─────┴────┴───────┘
```

**源码位置：** `MyCPUInstrFormats.td:190-209` — InstB 基类

### 2.5 三种格式对比

| 参数 | Word (32-bit) | Half (16-bit) | Byte (8-bit) |
|------|---------------|---------------|--------------|
| size 编码 | `10` | `01` | `00` |
| rd 宽度 | 5 bit (32 个) | 6 bit (64 个) | 7 bit (128 个) |
| fieldA 宽度 | 5 bit | 6 bit | 7 bit |
| fieldB 宽度 | 5 bit | 4 bit | 3 bit |
| fieldC 宽度 | 5 bit | 4 bit | 3 bit |
| msb 最大值 | 31 | 15 | 7 |
| 立即数范围 | [-16384, 16383] | [-8192, 8191] | [-8192, 8191] |
| LD/ST 偏移 | 10 bit (±511) | 8 bit (±127) | 6 bit (±31) |

### 2.6 关键字段语义

```
c_bit (进位/方向位):
    算术指令: 进位输入 (ADD/SUB 的 +c_bit 部分)
    协处理器: 0=LD (读), 1=ST (写)
    内存访存: 0=Load, 1=Store

f (Freg 索引):
    位 [16:14] (Word) / [17:15] (Half) / [18:16] (Byte)
    选择哪个 Freg 接收比较结果标志位
    取值范围: 0-3 (F0-F3)
```

---

## 3. 寄存器文件

### 3.1 通用寄存器 (GPR) 32 位 × 32 个

| 编号 | 名称 | 别名 | 用途 | 调用约定 |
|------|------|------|------|----------|
| R0 | `r0` | `zero` | 硬连线零寄存器 | 预留 (不可分配) |
| R1-R7 | `r1`-`r7` | `a0`-`a6` | 函数参数 / 返回值 | Caller-saved |
| R8-R15 | `r8`-`r15` | `t0`-`t7` | 临时寄存器 | Caller-saved |
| R16-R23 | `r16`-`r23` | `s0`-`s7` | 被调用者保存 | Callee-saved |
| R24-R27 | `r24`-`r27` | `t8`-`t11` | 扩展临时寄存器 | Caller-saved |
| R28 | `r28` | `lr` | 链接寄存器 (返回地址) | 预留 |
| R29 | `r29` | `sp` | 栈指针 | 预留 |
| R30 | `r30` | `fp` | 帧指针 | Callee-saved / 预留 |
| R31 | `r31` | `gp` | 全局指针 | 预留 |

**源码位置：**

| 内容 | 文件 |
|------|------|
| 寄存器定义 | `MyCPURegisterInfo.td:29-65` — 每个 `def` 对应一个寄存器 |
| GPR 寄存器类 | `MyCPURegisterInfo.td:85-92` — `def GPR` |
| 预留寄存器列表 | `MyCPURegisterInfo.cpp:96-104` — `getReservedRegs()` |
| Callee-saved 列表 | `MyCPURegisterInfo.cpp:52-55` — `CSR_MyCPU_SaveList` |
| HWEncoding 映射 | `MyCPURegisterInfo.td:12-16` — `class MyCPUReg` |
| 参数寄存器列表 | `MyCPUISelLowering.cpp:42-45` — `ArgRegs[]` |

### 3.2 标志寄存器 (Freg) 8 位 × 4 个

| 编号 | 名称 | 位定义 |
|------|------|--------|
| F0 | `f0` | bit0=Z (零), bit1=C (进位), bit2=N (负), bit3=V (溢出) |
| F1 | `f1` | 同上 |
| F2 | `f2` | 同上 |
| F3 | `f3` | 同上 |

**源码位置：**

| 内容 | 文件 |
|------|------|
| Freg 定义 | `MyCPURegisterInfo.td:71-79` |
| FREG 寄存器类 | `MyCPURegisterInfo.td:94-96` — 8 bit 宽度 |

### 3.3 寄存器编码对照表

```
寄存器编号 → HWEncoding (5-bit 物理编码, 直接对应指令中 rd/rs 字段)

R0  = 0x00    R8  = 0x08    R16 = 0x10    R24 = 0x18
R1  = 0x01    R9  = 0x09    R17 = 0x11    R25 = 0x19
R2  = 0x02    R10 = 0x0A    R18 = 0x12    R26 = 0x1A
R3  = 0x03    R11 = 0x0B    R19 = 0x13    R27 = 0x1B
R4  = 0x04    R12 = 0x0C    R20 = 0x14    R28 = 0x1C
R5  = 0x05    R13 = 0x0D    R21 = 0x15    R29 = 0x1D
R6  = 0x06    R14 = 0x0E    R22 = 0x16    R30 = 0x1E
R7  = 0x07    R15 = 0x0F    R23 = 0x17    R31 = 0x1F
```

---

## 4. 指令集详解

### 4.1 完整操作码表

#### 算术逻辑指令 (0x00-0x0A)

| Opcode | 助记符 | 格式 | 语义 | 源码 (TableGen) |
|--------|--------|------|------|-----------------|
| 0x00 | `add` | RRR / RRI | rd = rd + operand + c_bit | `MyCPUInstrInfo.td:62-66` (ADDW) |
| 0x01 | `addi` | RRI | rd = rd + sext(imm15) + c_bit | `MyCPUInstrInfo.td:128-131` (ADDIW) |
| 0x02 | `sub` | RRR | rd = rd - operand - c_bit | `MyCPUInstrInfo.td:72-75` (SUBW) |
| 0x03 | `subi` | RRI | rd = rd - sext(imm15) - c_bit | `MyCPUInstrInfo.td:136-139` (SUBIW) |
| 0x04 | `and` | RRR | rd = rd & operand | `MyCPUInstrInfo.td:80-83` (ANDW) |
| 0x05 | `andi` | RRI | rd = rd & zext(imm15) | `MyCPUInstrInfo.td:143-146` (ANDIW) |
| 0x06 | `or` | RRR | rd = rd \| operand | `MyCPUInstrInfo.td:88-91` (ORW) |
| 0x07 | `ori` | RRI | rd = rd \| zext(imm15) | `MyCPUInstrInfo.td:151-154` (ORIW) |
| 0x08 | `xor` | RRR | rd = rd ^ operand | `MyCPUInstrInfo.td:96-99` (XORW) |
| 0x09 | `xori` | RRI | rd = rd ^ zext(imm15) | `MyCPUInstrInfo.td:159-162` (XORIW) |
| 0x0A | `not` | RRR | rd = ~rs2[msb:lsb] | `MyCPUInstrInfo.td:106-115` (NOTW) |

#### 移位指令 (0x0C-0x11)

| Opcode | 助记符 | 语义 | 源码 |
|--------|--------|------|------|
| 0x0C | `shl` | rd = rd << rs2 | `MyCPUInstrInfo.td:173-176` (SHLW) |
| 0x0D | `shli` | rd = rd << shamt | `MyCPUInstrInfo.td:232-235` (SHLIW) |
| 0x0E | `shr` | rd = rd >> rs2 (逻辑) | `MyCPUInstrInfo.td:180-183` (SHRW) |
| 0x0F | `shri` | rd = rd >> shamt (逻辑) | `MyCPUInstrInfo.td:238-241` (SHRIW) |
| 0x10 | `sar` | rd = rd >> rs2 (算术) | `MyCPUInstrInfo.td:187-190` (SARW) |
| 0x11 | `sari` | rd = rd >> shamt (算术) | `MyCPUInstrInfo.td:244-247` (SARIW) |

#### 数据传送 (0x12-0x15)

| Opcode | 助记符 | 语义 | 源码 |
|--------|--------|------|------|
| 0x12 | `movab` | rd = rs2[msb:lsb] (寄存器→寄存器) | `MyCPUInstrInfo.td:309-311` (MOVABW) |
| 0x13 | `movi` | rd = sext(imm15) (立即数→寄存器) | `MyCPUInstrInfo.td:314-318` (MOVIW) |
| 0x14 | `ld` | rd = coproc[cop_id][addr] (协处理器读) | `MyCPUInstrInfo.td:300-301` (COPLDW) |
| 0x15 | `st` | coproc[cop_id][addr] = src (协处理器写) | `MyCPUInstrInfo.td:302-303` (COPSTW) |

#### 比较指令 (0x16-0x17)

| Opcode | 助记符 | 语义 | 源码 |
|--------|--------|------|------|
| 0x16 | `cmp` | Freg[f] = flags(rd - rs2[msb:lsb]) | `MyCPUInstrInfo.td:366-370` (CMPW) |
| 0x17 | `cmpi` | Freg[f] = flags(rd - sext(imm15)) | `MyCPUInstrInfo.td:373-375` (CMPIW) |

#### 分支 / 跳转 (0x18-0x1F)

| Opcode | 助记符 | 语义 | 源码 |
|--------|--------|------|------|
| 0x18 | `jmp` | PC = PC + sext(offset15) × 4 | `MyCPUInstrInfo.td:390-396` (JMP) |
| 0x19 | `jalr` | rd = PC+4; PC = rs2 | `MyCPUInstrInfo.td:399-406` (JALR) |
| 0x1A | `bz` | if Freg[f].Z then PC += sext(offset15)×4 | `MyCPUInstrInfo.td:408-412` (BZW) |
| 0x1B | `bnz` | if ~Freg[f].Z then PC += sext(offset15)×4 | `MyCPUInstrInfo.td:415-419` (BNZ) |
| 0x1E | `call` | LR = PC+4; PC = PC + sext(offset15)×4 | `MyCPUInstrInfo.td:429-434` (CALL) |
| 0x1F | `ret` | PC = LR | `MyCPUInstrInfo.td:437-446` (RET) |

#### 位操作指令 (0x22-0x25)

| Opcode | 助记符 | 语义 | 源码 |
|--------|--------|------|------|
| 0x22 | `bset` | rd[bitpos] = 1 | `MyCPUInstrInfo.td:468-477` (BSET) |
| 0x23 | `bclr` | rd[bitpos] = 0 | `MyCPUInstrInfo.td:480-489` (BCLR) |
| 0x24 | `bnot` | rd[bitpos] = ~rd[bitpos] | `MyCPUInstrInfo.td:492-501` (BNOT) |
| 0x25 | `btst` | Freg[f].Z = (rd[bitpos] == 0) | `MyCPUInstrInfo.td:504-510` (BTST) |

#### 系统指令 (0x26-0x27)

| Opcode | 助记符 | 语义 | 源码 |
|--------|--------|------|------|
| 0x26 | `nop` | 无操作 | `MyCPUInstrInfo.td:575-578` (NOP) |
| 0x27 | `halt` | 停止执行 | `MyCPUInstrInfo.td:581-586` (HALT) |

#### 内存访存 (0x28)

| Opcode | c_bit | 助记符 | 语义 | 源码 |
|--------|-------|--------|------|------|
| 0x28 | 0 | `mov` (load) | rd = mem[base + offset] | `MyCPUInstrInfo.td:343-347` (LDW) |
| 0x28 | 1 | `mov` (store) | mem[base + offset] = src | `MyCPUInstrInfo.td:350-355` (STW) |

### 4.2 操作粒度与对应的指令后缀

| C 类型 | LLVM 类型 | 粒度 | 后缀 | 示例 |
|--------|-----------|------|------|------|
| `char` / `int8_t` | `i8` | Byte | `.b` | `add.b r1, r2[7:0]` |
| `short` / `int16_t` | `i16` | Half | `.h` | `add.h r1, r2[15:0]` |
| `int` / `int32_t` | `i32` | Word | `.w` | `add.w r1, r2[31:0]` |

### 4.3 MSB/LSB 位域提取机制

MyCPU 的算术/逻辑指令支持**位域提取**：只对源操作数的指定位范围进行运算。

```
add.w r1, r2[15:8]    → r1 = r1 + (r2[15:8] 零扩展)
and.w r3, r4[7:0]     → r3 = r3 & (r4[7:0] 零扩展)
cmp.w r5, r6[31:16], f0 → F0 = flags(r5 - sext(r6[31:16]))
```

- `msb=31, lsb=0` 表示全字运算（最常见）
- 全字运算由 `Pat<>` 模式中的 `(ADDW GPR:$rd, GPR:$rs2, 31, 0)` 自动匹配
- 在 ISel 中由 `ISD::ADD` case 设置 `MSB=31, LSB=0`（`MyCPUISelDAGToDAG.cpp:273`）

---

## 5. 调用约定

### 5.1 参数传递

```
前 7 个 i32 参数 → 寄存器 A0-A6 (R1-R7)
超出部分        → 栈传递 (caller 分配在调用帧中)
返回值          → A0 (R1)
```

**源码位置：**

| 内容 | 文件 |
|------|------|
| 参数寄存器数组 | `MyCPUISelLowering.cpp:42-45` — `ArgRegs[]` |
| 参数分配函数 | `MyCPUISelLowering.cpp:49-63` — `CC_MyCPU_Assign()` |
| 形参处理 | `MyCPUISelLowering.cpp:222-258` — `LowerFormalArguments()` |
| 返回值处理 | `MyCPUISelLowering.cpp:268-282` — `LowerReturn()` |
| 调用处理 | `MyCPUISelLowering.cpp:297-353` — `LowerCall()` |
| Callee-saved CSR 定义 | `MyCPUCallingConv.td:14-20` — `CSR_MyCPU` |

### 5.2 寄存器保存约定

```
Caller-saved (调用者保存):
    A0-A6 (R1-R7), T0-T11 (R8-R15, R24-R27)
    函数调用后值可能改变，调用者如需保留需自行保存

Callee-saved (被调用者保存):
    S0-S7 (R16-R23), FP (R30)
    被调用函数如需使用必须在入口保存、出口恢复

特殊预留 (不可用于普通分配):
    ZERO (R0), LR (R28), SP (R29), FP (R30), GP (R31)
```

---

## 6. 栈帧布局

### 6.1 标准栈帧

```
高地址 (调用前 SP)
+----------------+
| 调用者栈帧     |
+----------------+
| 返回地址 (LR)  |  ← 仅当函数内有调用时保存 (MFI.hasCalls())
+----------------+
| 旧 FP 值       |  ← 仅当 hasFP() 为 true 时保存
+----------------+  ← FP 指向此处
| 局部变量       |
| ...            |
+----------------+  ← 当前 SP
低地址
```

**源码位置：**

| 内容 | 文件 |
|------|------|
| Prologue 生成 | `MyCPUFrameLowering.cpp:75-133` — `emitPrologue()` |
| Epilogue 生成 | `MyCPUFrameLowering.cpp:145-187` — `emitEpilogue()` |
| FP 判断逻辑 | `MyCPUFrameLowering.cpp:252-256` — `hasFPImpl()` |
| 帧偏移计算 | `MyCPUFrameLowering.cpp:199-216` — `getFrameIndexReference()` |
| Callee-save 决策 | `MyCPUFrameLowering.cpp:225-237` — `determineCalleeSaves()` |

### 6.2 Prologue 序列

```asm
; 1. 如果有子调用，保存 LR
subiw sp, sp, 4
mov.w  lr, [sp + 0]

; 2. 如果需要帧指针，保存 FP
subiw sp, sp, 4
mov.w  fp, [sp + 0]
movab.w fp, sp            ; 建立新 FP

; 3. 分配局部变量空间
subiw sp, sp, <locals_size>
```

### 6.3 Epilogue 序列 (逆序)

```asm
; 1. 回收局部变量
addiw sp, sp, <locals_size>

; 2. 恢复 FP
mov.w  fp, [sp + 0]
addiw sp, sp, 4

; 3. 恢复 LR
mov.w  lr, [sp + 0]
addiw sp, sp, 4

; 4. 返回
ret
```

---

## 7. 编译流水线

### 7.1 LLVM Pass 流水线与源码对应

```
阶段                        源码位置
────────────────────────────────────────────────────────
IR 生成 (Clang)             frontend/MyCPU.{h,cpp}
  │
IR 优化 (LLVM 通用)         (LLVM 内置, 无 MyCPU 特定)
  │
▼ DAG Lowering
  ├─ 操作合法化              MyCPUISelLowering.cpp:77-179 (构造函数)
  ├─ Custom 降级              MyCPUISelLowering.cpp:364-379 (LowerOperation)
  ├─ 调用约定降级             MyCPUISelLowering.cpp:222-353
  └─ 条件分支降级             MyCPUISelLowering.cpp:426-473 (LowerBR_CC)
  │
▼ DAG→DAG 指令选择
  └─ SDNode → MachineSDNode  MyCPUISelDAGToDAG.cpp:100-448 (Select)
  │
▼ Pseudo 展开 (Pre-RA)
  └─ LOAD_IMM/BEQ etc.       MyCPUTargetMachine.cpp:287-481 (expandMI)
  │
▼ 寄存器分配 (LLVM 通用)
  ├─ 预留寄存器               MyCPURegisterInfo.cpp:95-104
  ├─ Callee-saved spill       MyCPUFrameLowering.cpp:282-294
  └─ 帧索引消除               MyCPURegisterInfo.cpp:122-148
  │
▼ Prologue/Epilogue 插入
  └─ 栈帧设置/恢复            MyCPUFrameLowering.cpp:75-187
  │
▼ 汇编输出
  ├─ Pseudo → MCInst          MyCPUAsmPrinter.cpp:106-140
  ├─ MCInst → 文本             MyCPUInstPrinter.cpp:19-93
  └─ MCInst → 二进制           MyCPUMCCodeEmitter.cpp:90-387
  │
▼ 目标文件输出 (.o / .s)
```

### 7.2 Pass 注册

| Pass | 注册位置 |
|------|----------|
| 指令选择 (DAG→DAG) | `MyCPUTargetMachine.cpp:202-204` — `addInstSelector()` |
| 伪指令展开 | `MyCPUTargetMachine.cpp:209-211` — `addPreRegAlloc()` |
| 汇编输出 | `MyCPUAsmPrinter.cpp:209-211` — `LLVMInitializeMyCPUAsmPrinter()` |
| 目标注册 | `MyCPUTargetMachine.cpp:62-66` — `LLVMInitializeMyCPUTarget()` |

---

## 8. 源码文件索引

### 8.1 完整文件清单 (按功能分组)

#### TableGen 描述文件 (.td)

| 文件 | 内容 | 关键度 |
|------|------|--------|
| `MyCPU.td` | 根描述: include 所有子文件 | ★★ |
| `MyCPURegisterInfo.td` | 寄存器定义 (GPR × 32 + Freg × 4) | ★★★ |
| `MyCPUInstrFormats.td` | 指令格式基类 (InstW/InstH/InstB + 所有子类) | ★★★ |
| `MyCPUInstrInfo.td` | 指令定义 (每条指令 + Pat<> 模式) | ★★★ |
| `MyCPUCallingConv.td` | Callee-saved 寄存器列表 | ★★ |
| `MyCPUFeatures.td` | Subtarget 特性定义 | ★ |
| `MyCPUProcessors.td` | 处理器模型 | ★ |
| `MyCPUSchedule.td` | 调度模型 | ★ |

#### CodeGen 实现文件 (.cpp/.h)

| 文件 | 内容 | 关键度 |
|------|------|--------|
| `MyCPUTargetMachine.h/.cpp` | TargetMachine + Pass 配置 + Pseudo 展开 | ★★★ |
| `MyCPUISelLowering.h/.cpp` | ISel 降级策略 + 调用约定 | ★★★ |
| `MyCPUISelDAGToDAG.cpp` | DAG→DAG 指令选择 | ★★★ |
| `MyCPUInstrInfo.h/.cpp` | 指令操作 (copyPhysReg, 分支分析, spill) | ★★★ |
| `MyCPURegisterInfo.h/.cpp` | 寄存器信息 + 帧索引消除 | ★★★ |
| `MyCPUFrameLowering.h/.cpp` | Prologue/Epilogue 生成 | ★★★ |
| `MyCPUSubtarget.h/.cpp` | Subtarget 容器 + 特性查询 | ★★ |
| `MyCPUAsmPrinter.cpp` | 汇编输出 + 伪指令展开 | ★★ |
| `MyCPUCallingConv.cpp` | 调用约定 (当前仅占位) | ★ |
| `MyCPUMachineFunctionInfo.h/.cpp` | 函数级后端数据 | ★ |
| `MyCPU.h` | 顶层头文件 + ISel pass 声明 | ★ |

#### MC 层文件

| 文件 | 内容 | 关键度 |
|------|------|--------|
| `MCTargetDesc/MyCPUBaseInfo.h` | 指令固定参数 (InstSize, SizeField, getOpcode) | ★★★ |
| `MCTargetDesc/MyCPUMCCodeEmitter.cpp` | 32 位指令编码 (MCInst → 二进制) | ★★★ |
| `MCTargetDesc/MyCPUInstPrinter.h/.cpp` | 汇编文本打印 | ★★ |
| `MCTargetDesc/MyCPUMCTargetDesc.h/.cpp` | MC 层组件注册 | ★★ |
| `MCTargetDesc/MyCPUAsmBackend.cpp` | 汇编后端 (fixup/relaxation) | ★ |
| `MCTargetDesc/MyCPUFixupKinds.h` | Fixup 类型定义 | ★ |
| `MCTargetDesc/MyCPUELFStreamer.h/.cpp` | ELF 流处理器 | ★ |
| `MCTargetDesc/MyCPUMCAsmInfo.h/.cpp` | 汇编信息配置 | ★ |
| `AsmParser/MyCPUAsmParser.cpp` | 汇编文本解析 (.s → MCInst) | ★★ |
| `Disassembler/MyCPUDissassembler.cpp` | 反汇编 (二进制 → 汇编文本) | ★★ |
| `TargetInfo/MyCPUTargetInfo.h/.cpp` | 目标信息注册 | ★ |

#### Clang 前端

| 文件 | 内容 | 关键度 |
|------|------|------|
| `frontend/MyCPU.h` | Clang TargetInfo: 类型宽度、端序、数据布局 | ★★ |
| `frontend/MyCPU.cpp` | 寄存器名别名、预定义宏、内联汇编约束 | ★★ |

#### LLVM 通用补丁

| 文件 | 需要修改的原始文件 |
|------|-------------------|
| `backend/llvm-patches/Triple.h.patched` | `llvm/include/llvm/TargetParser/Triple.h` |
| `backend/llvm-patches/Triple.cpp.patched` | `llvm/lib/TargetParser/Triple.cpp` |
| `backend/llvm-patches/TargetDataLayout.cpp.patched` | `llvm/lib/TargetParser/TargetDataLayout.cpp` |
| `backend/llvm-patches/LLVM_CMakeLists.txt.patched` | `llvm/CMakeLists.txt` |
| `frontend/Targets.cpp.patched` | `clang/lib/Basic/Targets.cpp` |
| `frontend/CMakeLists.txt.patched` | `clang/lib/Basic/CMakeLists.txt` |

---

### 8.2 修改热力地图

当你需要修改某个功能时，最可能涉及的源文件 (★ 越多影响越大):

```
需要修改什么           最可能涉及的文件 (按优先级排列)
─────────────────────────────────────────────────────────
加一条新指令           ① MyCPUInstrInfo.td        ② MyCPUInstrFormats.td
                       ③ MyCPUMCCodeEmitter.cpp   ④ MyCPUISelDAGToDAG.cpp
                       ⑤ MyCPUISelLowering.cpp    ⑥ MyCPUInstrInfo.cpp

改指令编码/格式         ① MyCPUInstrFormats.td     ② MyCPUMCCodeEmitter.cpp
                       ③ MyCPUBaseInfo.h          ④ AsmParser/MyCPUAsmParser.cpp
                       ⑤ Disassembler/MyCPUDissassembler.cpp

改寄存器               ① MyCPURegisterInfo.td      ② MyCPURegisterInfo.cpp
                       ③ MyCPUCallingConv.td       ④ MyCPUISelLowering.cpp
                       ⑤ MyCPUFrameLowering.cpp    ⑥ frontend/MyCPU.cpp

改调用约定             ① MyCPUISelLowering.cpp     ② MyCPUCallingConv.td
                       ③ MyCPURegisterInfo.cpp     ④ MyCPUFrameLowering.cpp

改栈帧/Prologue        ① MyCPUFrameLowering.cpp    ② MyCPURegisterInfo.cpp

改协处理器支持          ① MyCPUInstrInfo.td        ② MyCPUInstrFormats.td
                       ③ MyCPUMCCodeEmitter.cpp   ④ MyCPUISelLowering.cpp

改操作合法化策略        ① MyCPUISelLowering.cpp

改汇编格式输出          ① MyCPUInstrInfo.td        ② MyCPUInstPrinter.cpp
                       ③ MyCPUAsmPrinter.cpp

增加 Subtarget 特性     ① MyCPUFeatures.td         ② MyCPUSubtarget.h/.cpp
                       ③ MyCPUISelLowering.cpp
```

---

> **文档版本**: 2026-07-01
> **适用版本**: LLVM 23.x + MyCPU backend
