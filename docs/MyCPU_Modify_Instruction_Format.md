# MyCPU 指令集字段格式修改指南

> 适用版本: LLVM 23.x + MyCPU backend (2026-06-08)

当你需要修改指令的字段排布（比如调整 opcode 位置、改变寄存器宽度、增加新的指令格式等），需要同步修改以下文件。

---

## 1. 文件地图

```
backend/MyCPU/
├── MyCPUInstrFormats.td     ← ★ 格式类定义 (字段布局)
├── MyCPUInstrInfo.td        ← ★ 指令定义 + 模式匹配
├── MyCPURegisterInfo.td     ← 寄存器定义
│
├── MCTargetDesc/
│   ├── MyCPUMCCodeEmitter.cpp  ← ★ 手工编码器 (按 opcode 分发)
│   ├── MyCPUAsmBackend.cpp     ← fixup 修正
│   ├── MyCPUBaseInfo.h         ← 基础常量
│   └── MyCPUInstPrinter.cpp    ← 汇编打印
│
├── AsmParser/
│   └── MyCPUAsmParser.cpp      ← ★ 汇编器 (助记符→MCInst)
│
├── Disassembler/
│   └── MyCPUDissassembler.cpp  ← ★ 反汇编器 (机器码→MCInst)
│
└── MyCPUISelDAGToDAG.cpp       ← 指令选择 (SDNode→MachineInstr)
```

**经验法则**：任何字段布局改动，至少影响加粗标记的 5 个文件。

---

## 2. 当前指令格式 (32-bit 定长)

### 2.1 三种粒度布局

```
                         Word (size=10)
 31  27 26  22 21  17 16  14 13 12   8 7   6 5      0
┌──────┬──────┬──────┬─────┬──┬───────┬──────┬────────┐
│fieldA│fieldB│fieldC│  f  │c │  rd   │  10  │ opcode │
│ 5b   │ 5b   │ 5b   │ 3b  │1b│  5b   │      │  6b    │
└──────┴──────┴──────┴─────┴──┴───────┴──────┴────────┘

                         Half (size=01)
 31  26 25  22 21  18 17  15 14 13   8 7   6 5      0
┌──────┬──────┬──────┬─────┬──┬───────┬──────┬────────┐
│fieldA│fieldB│fieldC│  f  │c │  rd   │  01  │ opcode │
│ 6b   │ 4b   │ 4b   │ 3b  │1b│  6b   │      │  6b    │
└──────┴──────┴──────┴─────┴──┴───────┴──────┴────────┘

                         Byte (size=00)
 31  25 24  22 21  19 18  16 15 14   8 7   6 5      0
┌──────┬──────┬──────┬─────┬──┬───────┬──────┬────────┐
│fieldA│fieldB│fieldC│  f  │c │  rd   │  00  │ opcode │
│ 7b   │ 3b   │ 3b   │ 3b  │1b│  7b   │      │  6b    │
└──────┴──────┴──────┴─────┴──┴───────┴──────┴────────┘
```

公共区域（所有格式不变）：
- `[7:6]` size (00=Byte, 01=Half, 10=Word, 11=Reserved)
- `[5:0]` opcode (6-bit, 0x00-0x3F)

---

## 3. 修改场景 & 步骤

### 场景 A: 修改 opcode 位数 (如从 6bit 改为 8bit)

**影响的文件 (5个)**：

**① MyCPUInstrFormats.td** — 调整字段布局
```tablegen
// 原来:
class MyCPUInst<...> {
  let Inst{5-0}   = opcode;          // opcode 低 6 位
  let Inst{7-6}   = size_bits;
  let TSFlags{13-8} = opcode;        // TSFlags 携带 opcode
}

// 改为 8bit opcode (占用 bit[7:0], size 移到 bit[9:8]):
class MyCPUInst<...> {
  let Inst{7-0}   = opcode;          // opcode 低 8 位
  let Inst{9-8}   = size_bits;       // size 移到上面
  let TSFlags{15-8} = opcode;        // TSFlags 更新
}
```

同时更新 InstW / InstH / InstB 中的字段位移：
```tablegen
// Word 格式 — 所有字段右移 2 位
class InstW<...> {
  let Inst{7-0}   = opcode;    // 原来是 {5-0}
  let Inst{9-8}   = 0b10;      // 原来是 {7-6}
  // rd/fieldA/fieldB/fieldC/f/c 都需要调整位移
  bits<5>  rd = 0;
  let Inst{10-8} = rd;          // 原来是 {12-8}, 现在右移 2 位
  ...
}
```

**② MyCPUMCCodeEmitter.cpp** — 编码器提取 HWOpcode
```cpp
// 原来:
uint8_t HWOpcode = (Desc.TSFlags >> 8) & 0x3F;   // 6bit opcode
unsigned SizeField = (Desc.TSFlags >> 6) & 0x3;

// 改为 8bit:
uint8_t HWOpcode = (Desc.TSFlags >> 8) & 0xFF;   // 8bit opcode
unsigned SizeField = (Desc.TSFlags >> 6) & 0x3;  // size 还在原位置
```

并调整位移常量：
```cpp
case 2: // Word
  RdMask = 0x1F; RdShift = 10;   // 原来是 8
  RsMask = 0x1F; RsShift = 29;   // 原来是 27
  ...
```

**③ MyCPUAsmParser.cpp** — 无需修改（使用 TableGen 枚举）

**④ MyCPUDissassembler.cpp** — 反汇编器提取 opcode
```cpp
// 原来:
static unsigned getOpcode(uint32_t Insn) {
  return Insn & 0x3F;                   // bits[5:0]
}
static unsigned getSizeFromInsn(uint32_t Insn) {
  return (Insn >> 6) & 0x3;             // bits[7:6]
}

// 改为:
static unsigned getOpcode(uint32_t Insn) {
  return Insn & 0xFF;                   // bits[7:0]
}
static unsigned getSizeFromInsn(uint32_t Insn) {
  return (Insn >> 8) & 0x3;             // bits[9:8]
}
```

同样调整 getFieldA/B/C、getRd、getF、getCBit 函数中的位移。

**⑤ MyCPUBaseInfo.h** — 常量定义
```cpp
// 原来:
inline unsigned getOpcode(uint32_t Encoding) {
  return Encoding & 0x3F;
}
// 改为:
inline unsigned getOpcode(uint32_t Encoding) {
  return Encoding & 0xFF;
}
```

---

### 场景 B: 新增一种宽度格式 (如 Double Word size=11)

**影响的文件 (5个)**：

**① MyCPUInstrFormats.td** — 新增 InstD 基类
```tablegen
// 新增 DoubleWord 格式 (size=11, 64-bit 操作)
class InstD<bits<6> opcode, dag outs, dag ins, string asmstr, list<dag> pattern>
    : MyCPUInst<opcode, outs, ins, asmstr, pattern> {
  let Inst{7-6} = 0b11;       // size = 3
  let TSFlags{7-6} = 0b11;

  bits<6>  fieldA;            // 跟 Half 一样宽
  bits<5>  fieldB;
  bits<5>  fieldC;
  bits<3>  f = 0;
  bit      c_bit = 0;
  bits<5>  rd = 0;

  let Inst{31-26} = fieldA;
  let Inst{25-21} = fieldB;
  let Inst{20-16} = fieldC;
  let Inst{15-13} = f;
  let Inst{12}    = c_bit;
  let Inst{11-7}  = rd;
}
```

然后定义对应的格式子类 (InstD_RRR, InstD_RRI 等)。

**② MyCPUMCCodeEmitter.cpp** — 新增 case
```cpp
case 3: // DoubleWord
  RdMask = 0x1F; RdShift = 7;
  RsMask = 0x3F; RsShift = 26;
  ...
  break;
```

**③ MyCPUDissassembler.cpp** — 新增 Sz==3 分支
```cpp
if (Sz == 3) {
  switch (Opc) {
    ...
  }
}
```

**④ MyCPUInstrInfo.td** — 定义新指令
```tablegen
def ADDD : InstD_RRR<0x00, ...>;
```

**⑤ MyCPUBaseInfo.h** — 新增枚举
```cpp
enum SizeField {
  SF_Byte     = 0,
  SF_Half     = 1,
  SF_Word     = 2,
  SF_Double   = 3,  // 新增
};
```

---

### 场景 C: 调整字段含义 (如把 rd 从 5bit 扩到 6bit)

假设 Word 格式 rd 从 5bit 扩到 6bit，占用 c_bit 的位置。

**影响的文件 (5个)**：

**① MyCPUInstrFormats.td**
```tablegen
// 新 Word 布局: c_bit 合并到 rd 高位
// [13:8] rd (6b)  ← 原来是 [12:8] rd (5b)
class InstW<...> {
  bits<6>  rd = 0;        // 原来是 bits<5>
  let Inst{13-8} = rd;    // 原来是 {12-8}
  // let Inst{13} = c_bit;  ← 删除，c_bit 不再有独立位
}
```

**② MyCPUMCCodeEmitter.cpp**
```cpp
case 2: // Word
  RdMask = 0x3F; RdShift = 8;   // 6bit rd, 原来是 0x1F
  ...
  // CBitShift 不再需要
```

**③ MyCPUDissassembler.cpp**
```cpp
static unsigned getRd(uint32_t Insn, unsigned Size) {
  switch (Size) {
  case 2: return (Insn >> 8) & 0x3F;  // 6bit, 原来是 0x1F
  ...
}
// getCBit 函数可能需要删除或调整
```

**④ MyCPUInstrInfo.td** — 所有指令的 `c_bit` / `rd` 关联需要更新

**⑤ MyCPUAsmPrinter.cpp** — tied operand 检查可能需要调整

---

### 场景 D: 新增指令类型 (如增加乘法指令 MUL)

只需修改 **2 个文件**：

**① MyCPUInstrFormats.td** — 无需修改（复用 InstW_RRR）

**② MyCPUInstrInfo.td** — 分配新 opcode + 定义指令
```tablegen
// 分配 opcode 0x29
def MULW : InstW_RRR<0x29, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u5imm:$msb, u5imm:$lsb),
  "mul.w\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0;
  let c_bit = 0;
}
def : Pat<(mul i32:$rd, i32:$rs2), (MULW GPR:$rd, GPR:$rs2, 31, 0)>;
```

如操作数布局与已有格式不同，则还需要修改编码器和汇编器。

---

## 4. 修改流程总结

```
1. 确定修改范围
   ├── 只加新指令?              → 场景 D (2 个文件)
   ├── 调整个别字段?            → 场景 C (5 个文件)
   ├── 改 opcode/size 位置?    → 场景 A (5 个文件)
   └── 新增宽度格式?            → 场景 B (5 个文件)

2. 按顺序修改文件
   ① MyCPUInstrFormats.td     ← TableGen 格式定义
   ② MyCPUInstrInfo.td        ← 指令定义(如需要)
   ③ MyCPUMCCodeEmitter.cpp   ← C++ 编码器
   ④ MyCPUDissassembler.cpp   ← C++ 反汇编器
   ⑤ MyCPUAsmParser.cpp       ← 汇编器(如需要)
   ⑥ MyCPUBaseInfo.h          ← 常量(如需要)

3. 编译验证
   ninja -C build LLVMMyCPUCodeGen LLVMMyCPUDesc
   ninja -C build llvm-mc clang

4. 测试
   echo '新指令' | llvm-mc -triple=mycpu-unknown-elf -show-encoding
   clang -target mycpu-unknown-elf -c test.c
```

---

## 5. 编码器分发逻辑 (当前)

`MyCPUMCCodeEmitter::getBinaryCodeForInstr()` 按 HWOpcode 分发：

```
HWOpcode == 0x28  → MOV 内存访存 (检查 TSFlags[14] 区分 Load/Store)
HWOpcode == 0x14  → COP LD 协处理器读
HWOpcode == 0x15  → COP ST 协处理器写
HWOpcode == 0x25  → BTST 位测试
HWOpcode == 0x18/0x1A/0x1B/0x1E → BR 分支
default          → RRR/RRI/SHIFT/BITOP/CMP/MOVAB 等通用格式
```

**新指令如果用新操作数布局**，需要在编码器添加新的 `if (HWOpcode == 0x??)` 分支。

---

## 6. TSFlags 编码

TSFlags 是 TableGen 传递给 C++ 编码器的元数据：

```
TSFlags[13:8]  = 硬件 opcode (6-bit)
TSFlags[7:6]   = size 字段 (2-bit)
TSFlags[14]    = MOV 访存方向 (0=Load, 1=Store) / COP 区分 LD/ST
```

编码器通过 `Desc.TSFlags` 读取这些信息来辅助编码决策。
