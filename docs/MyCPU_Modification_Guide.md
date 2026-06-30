# MyCPU 后端修改适配指南

> 当你需要人为修改 MyCPU 指令集架构时，本文档提供精准的修改步骤和源码位置。

---

## 目录

1. [修改前必读](#1-修改前必读)
2. [增加一条新指令](#2-增加一条新指令)
3. [删除/禁用一条指令](#3-删除禁用一条指令)
4. [修改指令编码格式](#4-修改指令编码格式)
5. [修改寄存器信息](#5-修改寄存器信息)
6. [修改协处理器指令/扩大 cop_id 范围](#6-修改协处理器指令扩大-cop_id-范围)
7. [修改调用约定](#7-修改调用约定)
8. [修改栈帧布局](#8-修改栈帧布局)
9. [增加 Subtarget 特性](#9-增加-subtarget-特性)
10. [修改操作合法化策略](#10-修改操作合法化策略)
11. [完整修改检查清单](#11-完整修改检查清单)

---

## 1. 修改前必读

### 1.1 改完源码后如何编译

```bash
# 1. 把修改后的后端安装到 LLVM 源树
cd E:/llvm_workspace/mycpu-toolchain
./scripts/setup-llvm.sh E:/llvm_workspace/llvm-project E:/llvm_workspace/llvm-project/build

# 2. 增量编译 (只改 .td 或少量 .cpp 的话，几分钟即可)
ninja -C E:/llvm_workspace/llvm-project/build clang llvm-mc

# 3. 更新独立工具链
cp E:/llvm_workspace/llvm-project/build/bin/clang.exe dist/bin/
cp E:/llvm_workspace/llvm-project/build/bin/llvm-mc.exe dist/bin/
cp E:/llvm_workspace/llvm-project/build/bin/llc.exe dist/bin/
cp E:/llvm_workspace/llvm-project/build/bin/llvm-objdump.exe dist/bin/
```

### 1.2 TableGen 生成的 .inc 文件

修改 `.td` 文件后，构建系统会自动调用 `llvm-tblgen` 重新生成以下 `.inc` 文件:

| 生成的 .inc 文件 | 来源 .td 文件 | 用途 |
|-----------------|---------------|------|
| `MyCPUGenRegisterInfo.inc` | `MyCPURegisterInfo.td` | 寄存器枚举和描述 |
| `MyCPUGenInstrInfo.inc` | `MyCPUInstrInfo.td` + `MyCPUInstrFormats.td` | 指令枚举和 MC 描述 |
| `MyCPUGenSubtargetInfo.inc` | `MyCPUFeatures.td` + `MyCPUProcessors.td` | Subtarget 信息 |
| `MyCPUGenDAGISel.inc` | 所有 .td 中的 `Pat<>` | TableGen 自动 DAG 匹配器 |
| `MyCPUGenAsmWriter.inc` | 所有 .td 中的 `AsmString` | 汇编输出格式化 |
| `MyCPUGenMCCodeEmitter.inc` | — (当前手动编码，未使用) | 二进制编码 |
| `MyCPUGenDisassemblerTables.inc` | — (当前手动解码，未使用) | 反汇编表 |

> **注意**：当前 `MyCPUMCCodeEmitter.cpp` 和 `MyCPUDissassembler.cpp` 使用**手动编码/解码**，并非 TableGen 自动生成。这意味着修改指令格式时需要**手动同步** C++ 编码器。

### 1.3 核心修改原则

```
修改范围          影响文件数      难度      注意事项
────────────────────────────────────────────────────
加一条新指令       3-5 个文件      ★★       编码器必须手动更新
改指令编码格式      5-8 个文件      ★★★      格式 .td + 编码器 C++ + 反汇编器必须一致
加/改寄存器         4-7 个文件      ★★★      CSR/Prologue/调用约定都要联动
改调用约定          3-5 个文件      ★★★      LowerFormalArguments 与 LowerCall 必须对称
改协处理器           3-4 个文件      ★★       COPLD/COPST 共用字段布局
```

---

## 2. 增加一条新指令

### 场景A: 增加一条 RRR 格式指令 (寄存器-寄存器)

**假设：增加一条 `MUL` 指令，操作码 `0x2A`，语义 `rd = rd * rs2[msb:lsb]`。**

#### 步骤 1: TableGen 指令定义

**文件:** `backend/MyCPU/MyCPUInstrInfo.td`

在文件末尾（`// ★ 添加新指令的模板示例 ★` 附近）添加：

```tablegen
// MULW: rd = rd * rs2[msb:lsb]
def MULW : InstW_RRR<0x2A, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u5imm:$msb, u5imm:$lsb),
  "mul.w\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0;
  let c_bit = 0;
}
```

#### 步骤 2: 添加 DAG 模式匹配 (让编译器自动选择)

**文件:** `backend/MyCPU/MyCPUInstrInfo.td`

```tablegen
// Pattern: 当 LLVM IR 中出现 (mul i32, i32) 时自动匹配 MULW
def : Pat<(mul i32:$rd, i32:$rs2), (MULW GPR:$rd, GPR:$rs2, 31, 0)>;
```

#### 步骤 3: 更新 ISelLowering — 将 MUL 设为 Legal

**文件:** `backend/MyCPU/MyCPUISelLowering.cpp`

找到构造函数中的 `setOperationAction` 区域，将 MUL 从 `Expand` 改为 `Legal`：

```cpp
// 原代码 (行 117):
setOperationAction(ISD::MUL,    MVT::i32, Expand);

// 修改为:
setOperationAction(ISD::MUL,    MVT::i32, Legal);
```

如果还要支持 i8/i16 的乘法，也添加:
```cpp
setOperationAction(ISD::MUL, MVT::i8,  Legal);
setOperationAction(ISD::MUL, MVT::i16, Legal);
```

#### 步骤 4: 更新手动编码器

**文件:** `backend/MyCPU/MCTargetDesc/MyCPUMCCodeEmitter.cpp`

新指令 0x2A 属于 RRR 格式，编码器中的 **DEFAULT format** 分支（行 311）应该能自动处理。但如果 0x2A 需要特殊处理，在 `getBinaryCodeForInstr()` 中添加：

```cpp
// MUL 指令 (0x2A): 使用默认 RRR 格式，无需特殊处理
// 如果操作数布局不同，在此添加特殊 case:
// if (HWOpcode == 0x2A) {
//   ... 自定义编码逻辑
//   return Binary;
// }
```

由于 0x2A 是标准 RRR 格式，**不需要**修改编码器。

#### 步骤 5: 更新操作码注释表

**文件:** `backend/MyCPU/MyCPUInstrInfo.td`

在文件顶部的操作码分配注释中添加：

```
//   0x2A MUL    ...
```

#### 步骤 6: 更新反汇编器

**文件:** `backend/MyCPU/Disassembler/MyCPUDissassembler.cpp`

在反汇编表中添加新指令的识别逻辑。

#### 步骤 7: 增加 Half/Byte 粒度版本 (可选)

在 `MyCPUInstrInfo.td` 中：

```tablegen
def MULH : InstH_RRR<0x2A, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u4imm:$msb, u4imm:$lsb),
  "mul.h\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(mul i16:$rd, i16:$rs2), (MULH GPR:$rd, GPR:$rs2, 15, 0)>;

def MULB : InstB_RRR<0x2A, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u3imm:$msb, u3imm:$lsb),
  "mul.b\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(mul i8:$rd, i8:$rs2), (MULB GPR:$rd, GPR:$rs2, 7, 0)>;
```

---

### 场景B: 增加一条 RRI 格式指令 (寄存器-立即数)

**假设：增加 `MULI` 指令，操作码 `0x2B`，语义 `rd = rd * sext(imm15)`。**

**文件:** `backend/MyCPU/MyCPUInstrInfo.td`

```tablegen
def MULIW : InstW_RRI<0x2B, (outs GPR:$rd), (ins GPR:$rd_in, simm15:$imm),
  "muli.w\t$rd, $rd, $imm", []> {
  let f = 0;
  let c_bit = 0;
}
def : Pat<(mul i32:$rd, imm:$imm), (MULIW GPR:$rd, simm15:$imm)>;
```

RRI 格式由编码器的 DEFAULT 分支（split immediate across fieldA/B/C）自动处理，无需修改编码器。

---

### 场景C: 增加一条带 Freg 结果的指令

**假设：增加 `TST` 指令，操作码 `0x2C`，语义 `Freg[f] = flags(rd & rs2)`。**

**文件:** `backend/MyCPU/MyCPUInstrInfo.td`

```tablegen
def TSTW : InstW_RRR<0x2C, (outs), (ins GPR:$rd, GPR:$rs2, u5imm:$msb, u5imm:$lsb, u3imm:$f),
  "tst.w\t$rd, $rs2[$msb:$lsb], $f", []> {
  let c_bit = 0;
  let Constraints = "";  // ★ 无 rd 输出
}
```

---

### 场景D: 增加一条新的分支指令

**假设：增加 `BN` (负标志跳转)，操作码 `0x1C`。**

需要修改的文件多达 **6 个**:

#### D1. TableGen 指令定义

**文件:** `backend/MyCPU/MyCPUInstrInfo.td`

```tablegen
def BN : InstW_BR<0x1C, (outs), (ins u3imm:$f, brtarget:$target),
  "bn\t$f, $target", []> {
  let c_bit = 0;
  let isBranch = 1;
  let isTerminator = 1;
}
```

#### D2. 编码器

**文件:** `backend/MyCPU/MCTargetDesc/MyCPUMCCodeEmitter.cpp`

在 BR 格式分支 (行 297) 的 `HWOpcode == 0x1B` 后面添加 `|| HWOpcode == 0x1C`：

```cpp
if (HWOpcode == 0x18 || HWOpcode == 0x1A ||
    HWOpcode == 0x1B || HWOpcode == 0x1C ||  // ← 添加这行
    HWOpcode == 0x1E) {
```

#### D3. 分支分析 (analyzeBranch)

**文件:** `backend/MyCPU/MyCPUInstrInfo.cpp:135-168`

在 `analyzeBranch()` 函数中识别新的分支指令：

```cpp
if (I->getOpcode() == MyCPU::BZW || I->getOpcode() == MyCPU::BNZ
    || I->getOpcode() == MyCPU::BN)  // ← 添加
  return true;
```

#### D4. 分支移除 (removeBranch)

**文件:** `backend/MyCPU/MyCPUInstrInfo.cpp:229-231`

```cpp
if (Opc != MyCPU::JMP && Opc != MyCPU::BZW && Opc != MyCPU::BNZ &&
    Opc != MyCPU::BN &&  // ← 添加
    Opc != MyCPU::CMPW)
```

#### D5. ISel 条件码映射

**文件:** `backend/MyCPU/MyCPUISelLowering.cpp:457-473`

在 `getBranchOpcodeForCond()` 中添加新的条件码映射（如果 BN 对应 SETLT 等）。

#### D6. ISelDAGToDAG BR_CC 处理

**文件:** `backend/MyCPU/MyCPUISelDAGToDAG.cpp:409-414`

在 `ISD::BR_CC` case 的 switch 中添加新条件码 → 新指令的映射。

---

## 3. 删除/禁用一条指令

### 方式A: 仅移除编译器自动生成 (保留汇编器支持)

**文件:** `backend/MyCPU/MyCPUInstrInfo.td`

删除对应的 `def : Pat<...>` 行，但保留 `def XXXW : ...` 指令定义。

```tablegen
// 只删除这行 Pattern，指令定义保留:
// def : Pat<(add i32:$rd, imm:$imm), (ADDIW GPR:$rd, simm15:$imm)>;
```

这样 LLVM 不再自动生成此指令，但手工汇编仍可使用。

### 方式B: 同时移除指令定义和 Pattern

**文件:** `backend/MyCPU/MyCPUInstrInfo.td`

删除整个指令定义块:
```tablegen
// 完全删除:
// def ADDIW : InstW_RRI<0x01, ...> { ... }
// def : Pat<...>;
```

同时更新 ISelLowering 策略:
**文件:** `backend/MyCPU/MyCPUISelLowering.cpp`

如果之前设为 `Legal`（如 ADD + imm 场景），LLVM 仍可能对该操作生成此指令。确保对应操作为 `Expand`。

### 方式C: 释放操作码供新指令使用

如果确定某个操作码不再使用，可以将其标记为"预留"。

**文件:** `backend/MyCPU/MyCPUInstrInfo.td` 顶部注释中更新操作码分配表。

---

## 4. 修改指令编码格式

### 4.1 修改字段布局 (如调整位域位置)

这是**影响最大**的修改。需要同步修改 **4 个文件**:

#### ① TableGen 格式定义

**文件:** `backend/MyCPU/MyCPUInstrFormats.td`

假设要把 Word 格式的 `f` 字段从 `[16:14]` 移到 `[19:17]`:

```tablegen
// 原代码 (InstW 基类):
// let Inst{16-14} = f;

// 修改后:
let Inst{19-17} = f;
```

同时调整其他字段的位分配。

#### ② 基础常量头文件

**文件:** `backend/MyCPU/MCTargetDesc/MyCPUBaseInfo.h`

如果修改了 size 或 opcode 的位掩码，更新 `getSizeField()` 和 `getOpcode()`。

#### ③ 编码器

**文件:** `backend/MyCPU/MCTargetDesc/MyCPUMCCodeEmitter.cpp`

更新 `getBinaryCodeForInstr()` 中**所有格式**的位偏移常量（`FShift`, `CBitShift` 等）：

```cpp
// Word 格式 (行 110-117):
case 2:
  RdMask = 0x1F; RdShift = 8;
  RsMask = 0x1F; RsShift = 27;
  MsbMask = 0x1F; MsbShift = 22;
  LsbMask = 0x1F; LsbShift = 17;
  FShift = 17;     // ← 修改: 原 14 → 17
  CBitShift = 13;
  break;
```

#### ④ 反汇编器

**文件:** `backend/MyCPU/Disassembler/MyCPUDissassembler.cpp`

同步更新字段提取逻辑。

#### ⑤ 模拟器 (如果使用)

**文件:** `sim/mycpu-sim.c`

同步更新指令解码逻辑。

### 4.2 修改 Opcode 位宽 (如从 6 位扩展到 7 位)

**文件:** `backend/MyCPU/MCTargetDesc/MyCPUBaseInfo.h:47-49`

```cpp
// 原代码:
inline unsigned getOpcode(uint32_t Encoding) {
  return Encoding & 0x3F;   // 6 位
}

// 修改为 7 位 (bit[6:0]):
inline unsigned getOpcode(uint32_t Encoding) {
  return Encoding & 0x7F;   // 7 位
}
```

同时修改:
- `MyCPUInstrFormats.td:101-102` — opcode 的位置和宽度
- `MyCPUMCCodeEmitter.cpp:99` — TSFlags 中的 opcode 提取掩码
- `MyCPUInstrInfo.td` — 所有操作码值（0x00-0x7F）

### 4.3 修改 Size 字段编码

**文件:** `backend/MyCPU/MCTargetDesc/MyCPUBaseInfo.h:32-37`

```cpp
// 当前编码:
enum SizeField {
  SF_Byte     = 0,  // 00
  SF_Half     = 1,  // 01
  SF_Word     = 2,  // 10
  SF_Reserved = 3,  // 11
};

// 例如增加 DoubleWord (64-bit):
enum SizeField {
  SF_Byte       = 0,
  SF_Half       = 1,
  SF_Word       = 2,
  SF_DoubleWord = 3,  // 11 — 原来是预留
};
```

同步修改:
- `MyCPUInstrFormats.td` — 所有 `Inst{7-6}` 和 `TSFlags{7-6}` 赋值
- `MyCPUMCCodeEmitter.cpp` — 编码器中的 `SizeField → 位宽映射`
- `MyCPUDissassembler.cpp` — 反汇编器

---

## 5. 修改寄存器信息

### 5.1 增加新寄存器

**假设：增加 8 个协处理器数据寄存器 `D0-D7` (用于 COP 直接存取)。**

#### 步骤 1: TableGen 定义

**文件:** `backend/MyCPU/MyCPURegisterInfo.td`

```tablegen
// 新寄存器定义
def D0 : MyCPUReg<32, "d0">;
def D1 : MyCPUReg<33, "d1">;  // HWEncoding 从 32 开始，避免与 GPR 冲突
def D2 : MyCPUReg<34, "d2">;
def D3 : MyCPUReg<35, "d3">;
def D4 : MyCPUReg<36, "d4">;
def D5 : MyCPUReg<37, "d5">;
def D6 : MyCPUReg<38, "d6">;
def D7 : MyCPUReg<39, "d7">;

// 新寄存器类
def DREG : MyCPURegClass<[i32], (add D0, D1, D2, D3, D4, D5, D6, D7)>;
```

#### 步骤 2: 注册到 ISelLowering (如果可分配)

**文件:** `backend/MyCPU/MyCPUISelLowering.cpp:81-85`

```cpp
addRegisterClass(MVT::i32, &MyCPU::GPRRegClass);
addRegisterClass(MVT::i32, &MyCPU::DREGRegClass);  // ← 添加
```

#### 步骤 3: 添加寄存器复制支持

**文件:** `backend/MyCPU/MyCPUInstrInfo.cpp:57-73`

在 `copyPhysReg()` 中添加 DREG ↔ GPR 的复制逻辑：

```cpp
// DREG → GPR: 使用 COPLDW 或专用传送指令
if (MyCPU::DREGRegClass.contains(SrcReg) &&
    MyCPU::GPRRegClass.contains(DestReg)) {
  BuildMI(MBB, MI, DL, get(MyCPU::COPLDW), DestReg)
      .addImm(0).addImm(SrcReg - MyCPU::D0);  // cop_id=0, addr=reg_index
  return;
}
```

#### 步骤 4: 更新预留寄存器 (如果是系统寄存器)

**文件:** `backend/MyCPU/MyCPURegisterInfo.cpp:95-104`

```cpp
markSuperRegs(Reserved, MyCPU::D0);  // 如果 DREG 不应被分配器使用
```

#### 步骤 5: 更新 Clang 前端寄存器名

**文件:** `frontend/MyCPU.cpp:10-15`

```cpp
const char *const MyCPUTargetInfo::GCCRegNames[] = {
    "r0", ..., "r31",
    "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",  // ← 添加
};
```

### 5.2 修改预留寄存器列表

**文件:** `backend/MyCPU/MyCPURegisterInfo.cpp:96-104`

```cpp
BitVector MyCPURegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  markSuperRegs(Reserved, MyCPU::ZERO); // 硬连线零
  markSuperRegs(Reserved, MyCPU::SP);   // 栈指针
  // markSuperRegs(Reserved, MyCPU::FP);  // 如果不再需要帧指针，可以放开 FP
  // markSuperRegs(Reserved, MyCPU::GP);  // 如果不需要全局指针，可以放开 GP
  markSuperRegs(Reserved, MyCPU::FP);
  markSuperRegs(Reserved, MyCPU::GP);
  markSuperRegs(Reserved, MyCPU::LR);   // 链接寄存器
  return Reserved;
}
```

> **注意**: 解除 SP 或 ZERO 的预留会导致编译器崩溃。

### 5.3 修改 Callee-Saved 寄存器列表

**文件:** `backend/MyCPU/MyCPURegisterInfo.cpp:52-55`

```cpp
static const MCPhysReg CSR_MyCPU_SaveList[] = {
    MyCPU::S0, MyCPU::S1, MyCPU::S2, MyCPU::S3,
    MyCPU::S4, MyCPU::S5, MyCPU::S6, MyCPU::S7,
    MyCPU::FP,
    // MyCPU::T8, MyCPU::T9,  // ← 如果想把 T8/T9 也设为 callee-saved
    0};  // ★ 必须以 0 结尾
```

同时更新 **RegMask**（位掩码，每个 bit 对应一个寄存器）：

```cpp
// 当前: R16-R23(bit 16-23) + FP(bit 30) + ZERO(bit 0) + LR(bit 28) = 0x1FE0002
static const uint32_t CSR_MyCPU_RegMask[] = {0x1FE0002, 0x00000000};
// 如果要加 T8(bit 24) 和 T9(bit 25):
// = 0x1FE0002 | (1<<24) | (1<<25) = 0x1FE0002 | 0x3000000 = 0x31FE0002
```

**文件:** `backend/MyCPU/MyCPUCallingConv.td`

```tablegen
// TableGen 侧的 CSR 也需同步:
def CSR_MyCPU : CalleeSavedRegs<(add S0, S1, S2, S3, S4, S5, S6, S7, FP)>;
// 添加 T8, T9:
// def CSR_MyCPU : CalleeSavedRegs<(add S0, S1, S2, S3, S4, S5, S6, S7, FP, T8, T9)>;
```

### 5.4 修改参数寄存器列表

**文件:** `backend/MyCPU/MyCPUISelLowering.cpp:42-45`

```cpp
// 当前: A0-A6 (7 个参数寄存器)
static const MCPhysReg ArgRegs[] = {
    MyCPU::A0, MyCPU::A1, MyCPU::A2, MyCPU::A3,
    MyCPU::A4, MyCPU::A5, MyCPU::A6
};

// 扩展: 增加 A7 (R8=T0)
// static const MCPhysReg ArgRegs[] = {
//     MyCPU::A0, MyCPU::A1, MyCPU::A2, MyCPU::A3,
//     MyCPU::A4, MyCPU::A5, MyCPU::A6, MyCPU::T0
// };
```

> 注意：增加参数寄存器意味着更少的临时寄存器，可能增加 spill。

---

## 6. 修改协处理器指令 / 扩大 cop_id 范围

### 6.1 当前协处理器指令格式

```
COPLD (opcode 0x14):   rd = coproc[cop_id][addr]   (CPU ← 协处理器)
COPST (opcode 0x15):   coproc[cop_id][addr] = src   (CPU → 协处理器)

Word  (0x14/0x15):   cop_id=5b (fieldA),   addr=10b (fieldB+C)
Half  (0x14/0x15):   cop_id=6b (fieldA),   addr=8b  (fieldB+C)
Byte  (0x14/0x15):   cop_id=7b (fieldA),   addr=6b  (fieldB+C)
```

### 6.2 扩大 cop_id 的识别范围

**假设：需要将 Word 格式的 cop_id 从 5 位 (0-31) 扩展到 8 位 (0-255)。**

#### ① 修改 TableGen 格式

**文件:** `backend/MyCPU/MyCPUInstrFormats.td:530-558`

Word COPLD 需要借用 `f` 字段的高位来扩展 cop_id：

```tablegen
class InstW_COPLD<bits<6> opcode, dag outs, dag ins,
                   string asmstr, list<dag> pattern>
    : InstW<opcode, outs, ins, asmstr, pattern> {
  bits<8> cop_id;      // ← 扩展: 原 5b → 8b
  bits<10> addr;
  let fieldA = cop_id{4-0};  // cop_id 低 5 位 → fieldA
  let f      = cop_id{7-5};  // cop_id 高 3 位 → f 字段
  let fieldB = addr{9-5};
  let fieldC = addr{4-0};
  let c_bit = 0;
  let TSFlags{14} = 0;
}
```

#### ② 修改编码器

**文件:** `backend/MyCPU/MCTargetDesc/MyCPUMCCodeEmitter.cpp:208-233`

在 `getBinaryCodeForInstr()` 的 `HWOpcode == 0x14` 分支中更新 cop_id 编码：

```cpp
if (HWOpcode == 0x14) {
  for (unsigned i = 0; i < NumOps; ++i) {
    const MCOperand &MO = MI.getOperand(i);
    if (MO.isImm() && i == 1) {
      int64_t CopID = MO.getImm();
      // 5-bit 在 fieldA
      Binary |= ((CopID & 0x1F) << 27);
      // 高 3-bit 在 Freg 字段
      Binary |= (((CopID >> 5) & 0x7) << 14);
    } else if (MO.isImm() && i == 2) {
      // ... addr 不变
    }
  }
}
```

#### ③ 更新 Clang 前端内建函数头文件

**文件:** `dist/include/mycpu_cop.h` 和 `examples/mycpu_cop.h`

```c
// 原: #define MYCPU_COP_MAX_ID  31
// 改: #define MYCPU_COP_MAX_ID  255
```

### 6.3 增加新的协处理器操作码

**假设：增加 `COPEXEC` (协处理器执行命令)，操作码 `0x2D`。**

**文件:** `backend/MyCPU/MyCPUInstrInfo.td`

```tablegen
// COPEXEC: 向协处理器发送执行命令 (无数据传送)
def COPEXECW : InstW<0x2D, (outs), (ins i32imm:$cop_id, i32imm:$cmd),
  "copexec\t$cop_id, $cmd", []> {
  bits<5> cop_id;
  bits<10> cmd;
  let f = 0;
  let c_bit = 0;
  let fieldA = cop_id;
  let fieldB = cmd{9-5};
  let fieldC = cmd{4-0};
}
```

**文件:** `backend/MyCPU/MCTargetDesc/MyCPUMCCodeEmitter.cpp`

在 `getBinaryCodeForInstr()` 中添加编码逻辑。

---

## 7. 修改调用约定

### 7.1 增加参数寄存器数量

**文件:** `backend/MyCPU/MyCPUISelLowering.cpp:42-45`

```cpp
// 从 7 个扩展到 10 个参数寄存器:
static const MCPhysReg ArgRegs[] = {
    MyCPU::A0, MyCPU::A1, MyCPU::A2, MyCPU::A3,
    MyCPU::A4, MyCPU::A5, MyCPU::A6,
    MyCPU::T0, MyCPU::T1, MyCPU::T2   // ← 新增 3 个
};
static const unsigned NumArgRegs = std::size(ArgRegs);  // 自动计算
```

**代价**: 减少 caller-saved 临时寄存器，可能增加 spill。

### 7.2 支持多返回值

**文件:** `backend/MyCPU/MyCPUISelLowering.cpp:268-282`

在 `LowerReturn()` 中支持多个返回值：

```cpp
SDValue MyCPUTargetLowering::LowerReturn(...) const {
  // 多个返回值: A0, A1, A2, ...
  for (unsigned i = 0; i < Outs.size(); ++i) {
    Register RetReg = (i == 0) ? MyCPU::A0 :
                      (i == 1) ? MyCPU::A1 :
                      (i == 2) ? MyCPU::A2 :
                      MyCPU::A0;  // fallback
    Chain = DAG.getCopyToReg(Chain, DL, RetReg, OutVals[i], SDValue());
  }
  return DAG.getNode(MyCPUISD::RetFlag, DL, MVT::Other, Chain);
}
```

同时在 `LowerCall()` 中（行 345-349）读取多个返回值。

### 7.3 改变返回值寄存器

**文件:** `backend/MyCPU/MyCPUISelLowering.cpp:277` (LowerReturn) 和行 347 (LowerCall)

```cpp
// 将返回值从 A0 改为 T0:
Chain = DAG.getCopyToReg(Chain, DL, MyCPU::T0, RetVal, SDValue());  // LowerReturn
SDValue Copy = DAG.getCopyFromReg(Chain, DL, MyCPU::T0, MVT::i32, ...); // LowerCall
```

---

## 8. 修改栈帧布局

### 8.1 改变栈帧对齐

**文件:** `backend/MyCPU/MyCPUFrameLowering.cpp:58`

```cpp
// 从 4 字节对齐改为 8 字节对齐:
MyCPUFrameLowering::MyCPUFrameLowering(const MyCPUSubtarget &STI)
    : TargetFrameLowering(TargetFrameLowering::StackGrowsDown, Align(8), 0,
                          Align(8)) {}
```

### 8.2 强制所有函数使用帧指针 (调试友好)

**文件:** `backend/MyCPU/MyCPUFrameLowering.cpp:252-256`

```cpp
bool MyCPUFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  return true;  // ← 始终使用 FP
  // 原代码:
  // const MachineFrameInfo &MFI = MF.getFrameInfo();
  // return MFI.isFrameAddressTaken() || MFI.hasVarSizedObjects() || ...;
}
```

### 8.3 在 Prologue 中保存更多寄存器

**文件:** `backend/MyCPU/MyCPUFrameLowering.cpp:75-133`

参考现有的 LR 和 FP 保存逻辑，添加新寄存器的 save/restore。

### 8.4 支持大栈帧 (>32767 字节)

**文件:** `backend/MyCPU/MyCPUFrameLowering.cpp:125-131`

当前大栈帧会触发 `llvm_unreachable`。实现大栈帧支持：

```cpp
if (StackSize <= 32767) {
  BuildMI(MBB, MBBI, DL, TII->get(MyCPU::SUBIW), MyCPU::SP)
      .addReg(MyCPU::SP).addImm(StackSize);
} else {
  // 大栈帧: MOVIW T0, StackSize_lo; SHLIW T0, T0, 15; ORIW T0, T0, StackSize_hi; SUBW SP, SP, T0
  Register TmpReg = MRI.createVirtualRegister(&MyCPU::GPRRegClass);
  // ... 实现细节
}
```

---

## 9. 增加 Subtarget 特性

### 9.1 定义新特性

**文件:** `backend/MyCPU/MyCPUFeatures.td`

```tablegen
// 硬件乘法器特性
def FeatureMulDiv  : SubtargetFeature<"muldiv", "HasMulDiv", "true",
                                      "Hardware multiply/divide support">;

// 浮点支持 (未来)
def FeatureFPU     : SubtargetFeature<"fpu", "HasFPU", "false",
                                      "Floating-point unit support">;
```

### 9.2 在 Subtarget 类中添加特性查询

**文件:** `backend/MyCPU/MyCPUSubtarget.h`

```cpp
// 特性查询接口
bool hasMulDiv() const { return HasMulDiv; }
bool hasFPU() const { return HasFPU; }

private:
  // 特性 bool 字段
  bool HasMulDiv;
  bool HasFPU;
```

### 9.3 根据特性改变编译行为

**文件:** `backend/MyCPU/MyCPUISelLowering.cpp:77-179`

在构造函数中根据 Subtarget 特性设置 Legal/Expand:

```cpp
// 如果有硬件乘法器，将 MUL 设为 Legal
if (STI.hasMulDiv()) {
  setOperationAction(ISD::MUL,  MVT::i32, Legal);
  setOperationAction(ISD::SDIV, MVT::i32, Legal);
} else {
  setOperationAction(ISD::MUL,  MVT::i32, Expand);
  setOperationAction(ISD::SDIV, MVT::i32, Expand);
}
```

### 9.4 使用特性

```bash
# 启用乘法器:
clang -target mycpu-unknown-elf -mllvm -mcpu=generic-mycpu+muldiv -c test.c -o test.o

# 禁用乘法器 (默认):
clang -target mycpu-unknown-elf -c test.c -o test.o
```

---

## 10. 修改操作合法化策略

这是调整编译器行为最常用且最安全的修改 — **只改一个文件**。

**文件:** `backend/MyCPU/MyCPUISelLowering.cpp:77-179`

### 10.1 四种策略

```cpp
// Legal   — 指令集直接支持，用 Pat<> 模式匹配或用 ISel Select() 选择
setOperationAction(ISD::ADD, MVT::i32, Legal);

// Expand  — LLVM 自动展开为更小/更简单的操作序列
setOperationAction(ISD::MUL, MVT::i32, Expand);

// Custom  — 需要手写 LowerXXX() 函数进行降级
setOperationAction(ISD::GlobalAddress, MVT::i32, Custom);

// Promote — 提升到更宽的位宽 (如 i8 → i32)，由 LLVM 自动插入截断/扩展
setOperationAction(ISD::ADD, MVT::i8, Promote);  // 当前实际是 Legal
```

### 10.2 常见修改场景

```cpp
// 场景1: 增加了硬件除法器 → 将 SDIV/UDIV 从 Expand 改为 Legal
setOperationAction(ISD::SDIV, MVT::i32, Legal);
setOperationAction(ISD::UDIV, MVT::i32, Legal);

// 场景2: 增加了条件 MOV 指令 → 将 SELECT_CC 从 Expand 改为 Custom
//        (然后在 LowerSELECT_CC 中实现)

// 场景3: 支持 64-bit 类型 → 移除 Expand，但需要新增 64-bit 寄存器类和指令
// setOperationAction(ISD::ADD, MVT::i64, Legal);  // 需要配套修改

// 场景4: 不想让 LLVM 展开乘法为移位加法序列 → 设为 LibCall
setOperationAction(ISD::MUL, MVT::i32, LibCall);
```

### 10.3 新增 Custom 降级操作

**步骤 1**: 在构造函数中设为 Custom:
```cpp
setOperationAction(ISD::CTPOP, MVT::i32, Custom);  // popcount
```

**步骤 2**: 在 `.h` 中声明 Lower 方法:
**文件:** `backend/MyCPU/MyCPUISelLowering.h`
```cpp
SDValue LowerCTPOP(SDValue Op, SelectionDAG &DAG) const;
```

**步骤 3**: 在 `LowerOperation()` 的 switch 中添加 case:
**文件:** `backend/MyCPU/MyCPUISelLowering.cpp:364-379`
```cpp
case ISD::CTPOP:
  return LowerCTPOP(Op, DAG);
```

**步骤 4**: 实现 `LowerCTPOP()` 方法 (可以用多条指令序列实现 popcount)。

**步骤 5**: 在 `getTargetNodeName()` 中添加调试名称 (如果有自定义 SDNode)。

---

## 11. 完整修改检查清单

### 增加新指令 ✓

- [ ] `MyCPUInstrInfo.td` — 添加 `def XXXW : InstW_YYY<...>`
- [ ] `MyCPUInstrInfo.td` — 添加 `def : Pat<...>` 模式 (可选)
- [ ] `MyCPUInstrFormats.td` — 如需新指令格式类，在此处定义
- [ ] `MyCPUISelLowering.cpp` — 将相关的 ISD 操作设为 Legal
- [ ] `MyCPUMCCodeEmitter.cpp` — 如非标准格式，添加编码逻辑
- [ ] `MyCPUISelDAGToDAG.cpp` — 如需要手动选择，在 Select() 中添加 case
- [ ] `MyCPUInstrInfo.cpp` — 如是分支指令，更新 analyzeBranch/insertBranch/removeBranch
- [ ] `MyCPUDissassembler.cpp` — 添加反汇编识别
- [ ] `MyCPUAsmParser.cpp` — 添加汇编解析 (如需要自定义解析)
- [ ] `MyCPUInstrInfo.td` — 添加 Half/Byte 粒度版本 (ADDH/ADDB 等)
- [ ] `sim/mycpu-sim.c` — 更新模拟器

### 修改指令格式 ✓

- [ ] `MyCPUInstrFormats.td` — 更新格式基类的位域分配
- [ ] `MyCPUBaseInfo.h` — 更新 size/opcode 提取函数
- [ ] `MyCPUMCCodeEmitter.cpp` — 更新所有格式的编码逻辑 (至少 6 个分支)
- [ ] `MyCPUInstrInfo.td` — 更新所有受影响的指令的 `let fieldX = ...`
- [ ] `MyCPUDissassembler.cpp` — 更新反汇编解码
- [ ] `MyCPUAsmParser.cpp` — 更新汇编解析
- [ ] `sim/mycpu-sim.c` — 更新模拟器解码

### 修改寄存器 ✓

- [ ] `MyCPURegisterInfo.td` — 定义新寄存器 + RegisterClass
- [ ] `MyCPURegisterInfo.cpp` — 更新 getReservedRegs() / CSR_SaveList / RegMask
- [ ] `MyCPUCallingConv.td` — 更新 CalleeSavedRegs 列表
- [ ] `MyCPUISelLowering.cpp` — 注册 RegisterClass, 更新 ArgRegs[]
- [ ] `MyCPUFrameLowering.cpp` — 如需在 Prologue/Epilogue 中保存/恢复
- [ ] `MyCPUInstrInfo.cpp` — copyPhysReg() 中添加新寄存器类复制逻辑
- [ ] `frontend/MyCPU.cpp` — 更新 GCCRegNames 和 GCCRegAliases
- [ ] `MyCPUMCCodeEmitter.cpp` — 如寄存器编码方式改变

### 修改协处理器 ✓

- [ ] `MyCPUInstrFormats.td` — COPLD/COPST 格式类 (InstW_COPLD 等)
- [ ] `MyCPUInstrInfo.td` — COPLDW/COPSTW 等指令定义
- [ ] `MyCPUMCCodeEmitter.cpp` — COP Load (0x14) 和 COP Store (0x15) 编码分支
- [ ] `dist/include/mycpu_cop.h` — 更新内建函数头文件
- [ ] `examples/mycpu_cop.h` — 同步更新

### 修改调用约定 ✓

- [ ] `MyCPUISelLowering.cpp:42` — 参数寄存器列表 ArgRegs[]
- [ ] `MyCPUISelLowering.cpp:49` — 参数分配函数 CC_MyCPU_Assign()
- [ ] `MyCPUISelLowering.cpp:222` — LowerFormalArguments() (入参)
- [ ] `MyCPUISelLowering.cpp:268` — LowerReturn() (返回)
- [ ] `MyCPUISelLowering.cpp:297` — LowerCall() (调用)
- [ ] `MyCPURegisterInfo.cpp:52` — CSR_SaveList 和 RegMask

### 快速验证

修改完成后:

```bash
# 1. 编译后端
ninja -C build LLVMMyCPUCodeGen

# 2. 运行测试
./scripts/test.sh build

# 3. 检查单条指令编码
echo 'add.w r1, r2[31:0]' | ./dist/bin/llvm-mc -triple=mycpu-unknown-elf -show-encoding

# 4. 测试完整编译
cat > /tmp/t.c << 'EOF'
int add(int a, int b) { return a + b; }
EOF
./dist/bin/clang -target mycpu-unknown-elf -S -O2 /tmp/t.c -o /tmp/t.s
cat /tmp/t.s
```

---

> **文档版本**: 2026-07-01
> **适用版本**: LLVM 23.x + MyCPU backend
