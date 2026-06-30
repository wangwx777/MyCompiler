# MyCPU LLVM 后端 — 逐文件功能与算法流程

> 版本: 2.0 | 日期: 2026-06-06 | LLVM 23.0.0git

---

## 0. 编译流水线全景

```
C/C++ 源码
  │
  ▼
[Clang Frontend] ─── clang/lib/Basic/Targets/MyCPU.cpp
  │                    定义目标三元组、预处理器宏、内置类型宽度
  ▼
LLVM IR
  │
  ▼
[ISelLowering] ─── MyCPUISelLowering.cpp
  │                    类型合法化、调用约定降级、Custom SDNode 生成
  ▼
SelectionDAG (Legalized)
  │
  ▼
[ISelDAGToDAG] ── MyCPUISelDAGToDAG.cpp + MyCPUInstrInfo.td (Pat<>)
  │                    SDNode → MachineInstr (指令选择)
  ▼
MachineInstr (SSA form, virtual registers)
  │
  ├─[ExpandPseudo] ── MyCPUTargetMachine.cpp (MyCPUExpandPseudo pass)
  │                    LOAD_IMM/BEQ/BNE/PseudoCMP → 真实指令序列
  ├─[Register Allocation] ── MyCPURegisterInfo.cpp + MyCPUInstrInfo.cpp
  │                    virtual reg → physical reg, spill/reload
  ├─[Frame Lowering] ── MyCPUFrameLowering.cpp
  │                    Prologue/Epilogue 插入, FrameIndex 消除
  ▼
MachineInstr (physical registers)
  │
  ▼
[AsmPrinter] ─── MyCPUAsmPrinter.cpp
  │                    MachineInstr → MCInst, 伪指令最后一次展开
  ▼
MCInst
  │
  ▼
[MCCodeEmitter] ─ MyCPUMCCodeEmitter.cpp
  │                    MCInst → 32-bit 二进制指令字
  ▼
[MCAsmBackend] ── MyCPUAsmBackend.cpp + MyCPUELFStreamer.cpp
                      Fixup 修正, ELF .o 文件写入
  ▼
.o / .s 文件
```

---

## 1. Clang 前端层

### 1.1 `clang/lib/Basic/Targets/MyCPU.cpp` / `.h`

**职责**: 向 Clang 注册 MyCPU 目标，定义类型系统和预处理器宏。

**算法流程**:
```
1. MyCPUTargetInfo 构造函数:
   - 设置指针宽度 32-bit, size_t 为 unsigned int
   - 设置内置类型: int=32, long=32, long long=64
   - 设置预处理器宏: __mycpu__=1
2. getTargetDefines(): 向预处理器注入平台宏
3. getCallingConv(): 返回默认调用约定
```

---

## 2. SelectionDAG 降级层 (ISelLowering)

### 2.1 `MyCPUISelLowering.cpp` — 类型合法化与调用约定

**职责**: 声明每条 IR 操作的处理策略 + 实现 Custom 操作的降级。

**核心数据结构**:
```cpp
static const MCPhysReg ArgRegs[] = {A0, A1, A2, A3, A4, A5, A6}; // 7 个参数寄存器
```

**构造函数 — 操作策略声明表**:

| ISD 操作 | i8 | i16 | i32 | i64 | 策略 |
|----------|----|-----|-----|-----|------|
| ADD/SUB/AND/OR/XOR | Legal | Legal | Legal | Expand | 直接匹配 |
| SHL/SRL/SRA | Legal | Legal | Legal | Expand | 直接匹配 |
| LOAD/STORE | Legal | Legal | Legal | Expand | 直接匹配 |
| MUL/DIV/REM | — | — | Expand | — | 展开为库调用 |
| BR_CC | Legal | Legal | Legal | — | Custom 降级 |
| GlobalAddress | — | — | Custom | — | Wrapper 节点 |
| SETCC/SELECT_CC | — | — | Expand | — | LLVM 自动展开 |

**算法: LowerFormalArguments()**:
```
for each 函数参数:
  1. CC_MyCPU_Assign() 分配位置:
     - 有可用 A0-A6? → 寄存器位置 (RegLoc)
     - 无可用寄存器? → 栈位置 (MemLoc)
  2. 寄存器参数: createVirtualRegister() + addLiveIn()
  3. 栈参数:   CreateFixedObject() + getFrameIndex() + Load
```

**算法: LowerReturn()**:
```
1. 返回值 CopyToReg(A0)
2. 发射 RetFlag SDNode → ISel 替换为 RET
```

**算法: LowerCall()**:
```
1. AnalyzeCallOperands() 分配参数位置 (同 FormalArguments 逻辑)
2. for each 实参:
     RegLoc → CopyToReg(An)
     MemLoc → Store(FrameIndex)
3. 发射 MyCPUISD::Call 节点
4. 从 A0 CopyFromReg 读取返回值
```

**算法: LowerBR_CC() — 条件分支降级**:
```
BR_CC(chain, cond, lhs, rhs, dest)
  │
  ├─ Cmp(lhs, rhs, msb=31, lsb=0, F0)  // 设置 F0 标志位
  └─ BrZ/BrNZ(F0, dest)                 // 根据 F0.Z 跳转
```

**自定义 SDNode 类型** (在 `MyCPUISelLowering.h`):
```
MyCPUISD::Wrapper  — 全局地址包装 (→ MOVIW)
MyCPUISD::RetFlag  — 返回标记 (→ RET)
MyCPUISD::Call     — 函数调用 (→ CALL)
MyCPUISD::Cmp      — 比较 (→ CMPW)
MyCPUISD::BrZ/BrNZ — 条件跳转 (→ BZW/BNZ)
```

---

## 3. 指令选择层 (ISelDAGToDAG)

### 3.1 `MyCPUISelDAGToDAG.cpp` — SDNode → MachineInstr

**职责**: 将合法化的 SelectionDAG 节点转换为 MyCPU 机器指令。

**算法: Select()**:
```
switch (SDNode->getOpcode()):
  case FrameIndex:     → TargetFrameIndex (延迟到 eliminateFrameIndex 解析)
  case Wrapper:        → MOVIW (全局地址加载)
  case RetFlag:        → RET
  case Call:           → CALL (提取 callee, 支持 Wrapper/GlobalAddress/ExternalSymbol)
  case Constant:       → isInt<15>? MOVIW : 留给 ExpandPseudo 展开
  case ADD/SUB:        → 按值类型选 ADDB/ADDH/ADDW (手动选择)
  case BR:             → JMP
  case BR_CC:          → CMP + BZW/BNZ (根据 cond 选 Bcc)
  default:             → SelectCode() — TableGen 自动匹配 Pat<> 模式
```

**ADD/SUB 手动选择的原因**: TableGen Pat<> 无法根据值类型 (i8/i16/i32) 动态选择不同 opcode 的指令，所以在 Select() 手写分支。

---

## 4. 机器码优化层

### 4.1 `MyCPUInstrInfo.cpp` — 指令操作支持

**职责**: 寄存器复制、分支分析/插入/删除、调度边界。

**copyPhysReg()**:
```
GPR → GPR: MOVW (opcode 0x28)
```

**analyzeBranch() 分支分析**:
```
基本块末尾指令:
  ┌─ JMP 单独?        → TBB = target, 返回 false (BranchFolder 可优化)
  ├─ JMP 前面有 Bcc?   → 返回 true (复合分支，不让 BranchFolder 处理)
  ├─ BZW/BNZ 单独?     → 返回 true (保守)
  └─ RET/其他?         → 返回 true
```

**insertBranch()**:
```
Cond 为空 → 发射 JMP
Cond 非空 → 
  1. 如果 Cond.size >= 6: 先发射 CMPW (从 Cond 重建)
  2. 发射 BZW/BNZ
  3. 如果有 FBB: 追加 JMP
```

**removeBranch()**:
```
从后向前遍历:
  跳过 debug 指令
  删除 JMP/BZW/BNZ/CMPW (含前置 CMPW，防止孤立的非 Terminator)
```

**storeRegToStackSlot / loadRegFromStackSlot**: STW / LDW + FrameIndex

### 4.2 `MyCPURegisterInfo.cpp` — 寄存器管理

**预留寄存器**:
```
ZERO (R0)  — 硬连线零
SP   (R29) — 栈指针
FP   (R30) — 帧指针
GP   (R31) — 全局指针
LR   (R28) — 链接寄存器
```

**Callee-Saved 寄存器**: S0–S7 (R17–R24), FP (R30)

**eliminateFrameIndex() — 帧索引消除**:
```
输入: LDW rd, FrameIndex(fi), 0
算法:
  1. FrameIndex fi → getObjectOffset() → 初始偏移
  2. getFrameIndexReference() → FrameReg (FP/SP) + 附加偏移
  3. 累计最终偏移 = 初始 + 附加
  4. 替换: FrameIndex → FrameReg, 占位Imm → 真实Offset
输出: LDW rd, FP, -16
```

### 4.3 `MyCPUFrameLowering.cpp` — 栈帧管理

**emitPrologue()**:
```
if (有子调用):
  SUBIW SP, SP, 4;  STW LR, [SP+0]    // 保存返回地址
if (需要帧指针):
  SUBIW SP, SP, 4;  STW FP, [SP+0]    // 保存旧 FP
  MOVW FP, SP                           // 建立新 FP
if (StackSize > 0):
  SUBIW SP, SP, StackSize              // 分配局部变量空间
```

**emitEpilogue()** (与 Prologue 对称逆序):
```
if (StackSize > 0):
  ADDIW SP, SP, StackSize             // 回收栈空间
if (需要帧指针):
  LDW FP, [SP+0];  ADDIW SP, SP, 4   // 恢复 FP
if (有子调用):
  LDW LR, [SP+0];  ADDIW SP, SP, 4   // 恢复 LR
```

**栈帧布局**:
```
高地址
+----------------+
| 调用者栈帧      |
+----------------+
| LR (可选)       |  ← 仅当函数有调用
+----------------+
| 旧 FP (可选)    |  ← 仅当 hasFP()
+----------------+  ← FP 指向此处
| 局部变量        |
| spilled regs    |
| ...             |
+----------------+  ← SP 指向此处
低地址
```

### 4.4 `MyCPUSubtarget.cpp` — 目标特性管理

**职责**: 解析 CPU/Feature 字串，缓存子组件实例。

```
构造函数:
  1. MyCPUGenSubtargetInfo(TT, CPU, FS)
  2. FrameLowering(*this)
  3. InstrInfo(*this)
  4. TLInfo(TM, *this)  // TargetLowering
  5. ParseSubtargetFeatures(CPU, FS)  // 解析 "+feature1,-feature2"
```

### 4.5 `MyCPUTargetMachine.cpp` — Pass 流水线

**Pass 注册顺序**:
```
IR → addIRPasses()
   → addInstSelector()      — createMyCPUISelDag()
   → addPreRegAlloc()       — MyCPUExpandPseudo pass (伪指令展开)
   → [Register Allocation]
   → addPreEmitPass()       — (预留: peephole, 延迟槽)
   → AsmPrinter
```

**MyCPUExpandPseudo — 伪指令展开**:

| 伪指令 | 展开 |
|--------|------|
| LOAD_IMM rd, imm | isInt<15>? MOVIW rd, imm : MOVIW + SHLIW + MOVIW + ADDW 拆分 |
| LOAD_ADDR rd, addr | 同上 |
| BEQ rs1, rs2, TBB | CMPW rs1, rs2[31:0], F0; BZW F0, TBB |
| BNE rs1, rs2, TBB | CMPW rs1, rs2[31:0], F0; BNZ F0, TBB |
| BLT rs1, rs2, TBB | CMPW rs1, rs2[31:0], F0; BNZ F0, TBB (fallback) |
| BGE rs1, rs2, TBB | CMPW rs1, rs2[31:0], F0; BZW F0, TBB |
| PseudoCMP rd,rs1,rs2 | CMPW rs1, rs2[31:0], F0; MOVIW rd, 0 |
| SELECT | (暂未实现) |
| COMPILER_BARRIER | 直接删除 (调度屏障) |

**LOAD_IMM 大立即数拆分算法**:
```
32-bit Value V:
  Lo = V[14:0]           // 最低 15 位
  SE_Lo = sign_extend_15(Lo)
  Remaining = V - SE_Lo  // 去掉最低 15 位符号扩展后的剩余
  Mid = (Remaining >> 15)[14:0]
  Top = (Remaining >> 30)[1:0]

  MOVIW rd, Lo
  if Mid|Top != 0:
    MOVIW tmp, Mid
    if Mid[14]: MOVIW fix, 1; SHLIW fix, fix, 15; ADDW tmp, tmp, fix[31:0]
    SHLIW tmp, tmp, 15
    ADDW rd, rd, tmp[31:0]
    if Top != 0:
      MOVIW tmp2, Top; SHLIW tmp2, tmp2, 30; ADDW rd, rd, tmp2[31:0]
```

---

## 5. MC 层 (MachineInstr → 二进制)

### 5.1 `MyCPUAsmPrinter.cpp` — 汇编输出

**职责**: MachineInstr → MCInst 降级 + 伪指令最后展开。

**emitInstruction() 流程**:
```
1. lowerPseudoInstExpansion(MI, Inst):
   LOAD_IMM/LOAD_ADDR (isInt<15>) → MOVIW
   其他 → 返回 false
2. 如果展开成功 → EmitToStreamer(Inst)
3. 如果未展开 → lowerToMCInst(MI, Inst) → EmitToStreamer(Inst)
```

**lowerToMCInst()**:
```
for each MachineOperand:
  跳过 tied USE 操作数 (避免 MCInst 中出现重复)
  MO_Register       → createReg()
  MO_Immediate      → createImm()
  MO_MBB/Global/Block/External/CPI → createExpr(symbol)
```

### 5.2 `MyCPUInstPrinter.cpp` — 文本格式

**职责**: 将 MCInst 打印为汇编文本。通过 TableGen 生成的 `MyCPUGenAsmWriter.inc` 自动完成。手写部分仅提供 `printRegName()` 和各类立即数打印回调 (`printU5Imm`, `printSImm15` 等)。

### 5.3 `MyCPUMCCodeEmitter.cpp` — 二进制编码 (核心)

**职责**: MCInst → 32-bit 指令字。这是**手写编码器**，按 opcode 分支处理。

**入口**: `getBinaryCodeForInstr(MI, Fixups, STI) → uint32_t`

**编码流程**:
```
1. 从 TSFlags 提取 HWOpcode (bits[13:8]) 和 SizeField (bits[7:6])
2. Binary = HWOpcode | (SizeField << 6)   // 设定 opcode + size
3. 根据 SizeField 配置字段常量:
     Word: RdMask=0x1F RdShift=8  RsMask=0x1F RsShift=27
           MsbMask=0x1F MsbShift=22 LsbMask=0x1F LsbShift=17
           FShift=14 CBitShift=13 LsbWidth=5
     Half: RdMask=0x3F RdShift=8  RsMask=0x3F RsShift=26
           MsbMask=0xF MsbShift=22  LsbMask=0xF LsbShift=18
           FShift=15 CBitShift=14 LsbWidth=4
     Byte: RdMask=0x7F RdShift=8  RsMask=0x7F RsShift=25
           MsbMask=0x7 MsbShift=22  LsbMask=0x7 LsbShift=19
           FShift=16 CBitShift=15 LsbWidth=3
4. 按 HWOpcode 分发编码逻辑:
   ├── 0x15 (ST):  src→fieldA, base→fieldB(Word)/rd(Half/Byte), offset 拆分
   ├── 0x14 (LD):  rd→rd, base→fieldA, offset→fieldB+C
   ├── 0x12 (COP): rd→rd, cop_id→fieldA, addr→fieldB+C, c_bit=TSFlags[14]
   ├── 0x25 (BTST): rd→rd, bitpos→fieldA, f→FMask<<FShift
   ├── 0x18/0x1A/0x1B/0x1E (BR): f(对于 BZ/BNZ)→f 字段 + PCRel fixup
   └── default (RRR/RRI/SHIFT/BITOP/CMP/...):
         i=0 reg → rd
         i=1: IsFieldAOnly? fieldA : 拆分 fieldA+B+C (RRI 立即数)
         i=2 imm → fieldB
         i=3 imm → fieldC
         i=4 imm → f 字段
         i=5 imm → c_bit
5. 0x28 (MOV) 后处理: fieldB = maxBit (31/15/7)
6. 返回 32-bit Binary
```

**encodeInstruction()**: 小端序输出 4 字节。

### 5.4 `MyCPUAsmBackend.cpp` — Fixup 与 ELF 写入

**applyFixup()** — 将编译时未知的值填入指令:
```
fixup_MyCPU_16 / PCRel_16:
  V[14:0] 映射到 bits[31:17]:
    Data[2] |= (V[6:0] << 1) & 0xFE   // bit0=f[2] 保留
    Data[3] |= V[14:7]

fixup_MyCPU_32:
  Data[0..3] |= Value[31:0]

PCRel: Value -= 2 (PC 偏置)
```

### 5.5 `MyCPUELFStreamer.cpp` — ELF 重定位

Fixup → ELF 重定位类型映射:
```
fixup_MyCPU_32       → R_MYCPU_32      (1)
fixup_MyCPU_16       → R_MYCPU_16      (2)
fixup_MyCPU_PCRel_16 → R_MYCPU_PCRel_16 (3)
```

ELF machine ID: `EM_MYCPU = 0x8472`

---

## 6. 汇编器 / 反汇编器

### 6.1 `MyCPUAsmParser.cpp` — .s → MCInst

**架构**: 手写解析器，助记符 → FormatClass 查表驱动。

**MnemonicTable**: `StringMap<{Opcode, FormatClass}>`，约 70 条助记符。

**FormatClass 一览**:
```
 0=RRR     → rd, rs2[msb:lsb]
 1=RRI     → rd, [rd_in,] imm
 2=MOV     → rd, rs2
 3=LD      → rd, [base+off]
 4=ST      → src, [base+off]
 5=BR      → target (JMP/CALL)
 6=BRF     → f, target (BZ/BNZ)
 7=RET/NOP/HALT → 无操作数
 9=BSET/BCLR/BNOT → rd, bitpos
10=BTST    → rd, bitpos, f
12=CMP     → rd, rs2[msb:lsb], f
13=NOT     → rd, rs2[msb:lsb]
14=JALR    → rd, rs2
15=SHIFT   → rd, rd_in, rs2
16=SHIFTI  → rd, rd_in, shamt
17=COPLD   → rd, cop_id, addr
18=COPST   → src, cop_id, addr
```

**parseInstruction() 流程**:
```
1. 查 MnemonicTable → 获取 Opcode + FormatClass
2. 根据 FormatClass 解析操作数:
   - 寄存器: parseRegister() → 识别 "r0"–"r31"
   - 位域: parseRegOrBitfield() → reg 可选 [msb:lsb]
   - 内存: parseMemOperand() → [base + offset]
   - 立即数: parseExpression()
   - 分支目标: parseExpression()
3. 吃入行尾 EndOfStatement
```

**matchAndEmitInstruction() 流程**:
```
1. 查 MnemonicTable → 获取 Opcode + FormatClass
2. 根据 FormatClass 构建 MCInst:
   - 跳过 mnemonic token (Operands[0])
   - 按操作数索引提取 reg/imm/expr
   - 注意 tied operand 约束 (如 SHIFT: rd_in 在 Operands[2] 但不入 MCInst)
3. Out.emitInstruction(Inst)
```

### 6.2 `MyCPUDissassembler.cpp` — 机器码 → MCInst

**架构**: 手写解码器。按 size(Word/Half/Byte) 分段，每段按 opcode switch。

**getInstruction() 流程**:
```
1. 读 4 字节 LE → uint32_t Insn
2. 提取 Sz=bits[7:6], Opc=bits[5:0]
3. 提取 Rd, Fa, Fb, Fc, FIdx (字段位置随 Sz 变化)
4. 按 Sz 进入对应 switch(Opc):
   - COP (0x12): c_bit 区分 LD/ST, 构造 addr
   - LD (0x14): 构造 base+offset
   - ST (0x15): 构造 src+base+offset
   - SHIFT (0x0C/0x0E/0x10): rd, rs2
   - SHIFTI (0x0D/0x0F/0x11): rd, shamt
   - BITOP (0x22-0x25): rd, bitpos [, f]
5. 通过 GPRDecodeTable[] 将编码号转换为 MCRegister
```

---

## 7. 编码字段生命周期追踪

以 `ADDW r8, r9[31:0]` 为例，展示 rd/fieldA/fieldB/fieldC 在各层的流转:

```
层级                  表示形式
────────────────────────────────────────────────────
LLVM IR              %0 = add i32 %x, %y

ISelLowering         (add i32:%rd, i32:%rs2)
                     setOperationAction(ADD, i32, Legal)

Pat<> 匹配           Pat<(add i32:$rd, i32:$rs2),
                          (ADDW GPR:$rd, GPR:$rs2, 31, 0)>

ISelDAGToDAG         SDNode → MachineInstr
                     case ADD: Opc = ADDW; MSB=31; LSB=0

MachineInstr         操作数: [rd:r8, rs2:r9, msb:31, lsb:0]

AsmPrinter           MCInst: [Reg:T0, Reg:T1, Imm:31, Imm:0]

MCCodeEmitter        HWOpcode=0x00 SizeField=0x2
  RRR default path:  i=0: rd=T0(25) → bits[12:8]=25
                     i=1: rs2=T1(26) → fieldA[31:27]=26
                     i=2: msb=31 → fieldB[26:22]=31
                     i=3: lsb=0  → fieldC[21:17]=0
  Binary = 0x00 | (2<<6) | (25<<8) | (26<<27) | (31<<22) | 0
         = 0x80 | 0x1900 | 0xD0000000 | 0x7C00000
         = 0xD7C01980

encodeInstruction:   小端字节 → CB = [0x80, 0x19, 0xC0, 0xD7]
```

---

## 8. 文件修改速查表

| 要做什么 | 改什么文件 |
|---------|-----------|
| 添加新 opcode 指令 | `MyCPUInstrInfo.td` → def NEWW : InstW_XXX<0x29, ...> |
| 添加新指令格式 | `MyCPUInstrFormats.td` → class InstW_NEW : InstW<...> |
| 修改字段位位置 | `MyCPUInstrFormats.td` + `MyCPUMCCodeEmitter.cpp` (两处同步) |
| 新指令特殊编码逻辑 | `MyCPUMCCodeEmitter.cpp` → 加 opcode case |
| 新助记符 | `MyCPUAsmParser.cpp` → MnemonicTable + parse/emit case |
| 反汇编支持 | `MyCPUDissassembler.cpp` → Sz 段加 case |
| 新伪指令 | `MyCPUInstrInfo.td` → Pseudo + `MyCPUTargetMachine.cpp` → ExpandPseudo |
| 修改调用约定 | `MyCPUISelLowering.cpp` → ArgRegs[] + LowerFormalArguments |
| 修改栈帧布局 | `MyCPUFrameLowering.cpp` → Prologue/Epilogue |
| 修改预留寄存器 | `MyCPURegisterInfo.cpp` → getReservedRegs() |
| 新 ELF 重定位类型 | `MyCPUAsmBackend.cpp` + `MyCPUELFStreamer.cpp` + `MyCPUFixupKinds.h` |
| 新 Subtarget 特性 | `MyCPUFeatures.td` + `MyCPUSubtarget.cpp` |

## 9. 调试命令

```bash
# 汇编
echo 'cop.ld.w r1, 0, 0' | llvm-mc -triple=mycpu-unknown-elf -show-encoding
# 反汇编
echo '0x92 0x06 0x00 0x1a' | llvm-mc -triple=mycpu-unknown-elf -disassemble
# C → 汇编
echo 'int f(int x){return x+1;}' | clang -target mycpu-unknown-elf -S -o - -x c -
# C → 目标文件
echo 'int x;' | clang -target mycpu-unknown-elf -c -o test.o -x c -
# 查看重定位
llvm-readobj --relocations test.o
# 反汇编 .o
llvm-objdump -d --triple=mycpu-unknown-elf test.o
```
