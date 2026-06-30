# MyCPU 指令集架构技术方案

> 版本: 1.0 | 日期: 2026-06-06 | 基于 LLVM 23.0.0git

---

## 1. 编码总纲

### 1.1 32 位定长指令字

所有指令固定 4 字节，小端序。最低 8 位为公共头：

```
bits[5:0]   = opcode  (6-bit 硬件操作码, 0x00–0x3F)
bits[7:6]   = size    (2-bit 操作宽度)
                00 = Byte   (8-bit)
                01 = Half   (16-bit)
                10 = Word   (32-bit)
                11 = 保留
```

opcode 决定 **做什么**（加/减/移位/访存/协处理器…），size 决定 **操作多宽**（字段位宽、寄存器数量随 size 变化）。

### 1.2 三种宽度的字段布局

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

| 字段 | Word | Half | Byte | 说明 |
|------|------|------|------|------|
| fieldA | 5b [31:27] | 6b [31:26] | 7b [31:25] | 语义由 opcode 决定 |
| fieldB | 5b [26:22] | 4b [25:22] | 3b [24:22] | 语义由 opcode 决定 |
| fieldC | 5b [21:17] | 4b [21:18] | 3b [21:19] | 语义由 opcode 决定 |
| f | 3b [16:14] | 3b [17:15] | 3b [18:16] | Freg 索引 (F0–F3) |
| c | 1b [13] | 1b [14] | 1b [15] | 进位输入 / R/W 标志 |
| rd | 5b [12:8] | 6b [13:8] | 7b [14:8] | 目的寄存器 |
| size | 2b [7:6] | 2b [7:6] | 2b [7:6] | 操作宽度 |
| opcode | 6b [5:0] | 6b [5:0] | 6b [5:0] | 硬件操作码 |

---

## 2. fieldA/B/C 语义分配

fieldA/B/C 是三个通用槽位，**语义完全由 opcode 决定**，不是物理上固定为 rs2/msb/lsb：

| 指令类型 | fieldA | fieldB | fieldC |
|---------|--------|--------|--------|
| RRR 算术 (ADD/SUB/AND/OR/XOR/CMP) | rs2 | msb | lsb |
| RRI 立即数 (ADDI/SUBI/ANDI/ORI/XORI/MOVI) | imm[14:10] | imm[9:5] | imm[4:0] |
| SHIFT 寄存器移位 | rs2 | 0 | 0 |
| SHIFTI 立即移位 | shamt | 0 | 0 |
| BITOP 位操作 | bitpos | 0 | 0 |
| MOV 寄存器传送 | rs2 | max_bit | 0 |
| LD 加载 | base | offset[9:5] | offset[4:0] |
| ST 存储 | src | base/offset | offset |
| BR 分支 | offset[14:10] | offset[9:5] | offset[4:0] |
| **COP 协处理器** | **cop_id** | **addr[hi]** | **addr[lo]** |

---

## 3. COP 协处理器访存指令

### 3.1 设计动机

将 `opcode=0x12` 从纯寄存器传送 (MOV) 改造为协处理器访存指令，用 `c_bit` 区分读写方向，复用 fieldA/B/C 编码协处理器编号和内部地址。

### 3.2 编码

```
c_bit = 0  →  cop.ld    协处理器读: CPU寄存器 ← 协处理器
c_bit = 1  →  cop.st    协处理器写: CPU寄存器 → 协处理器
```

```
COP Word (size=10):                      COP Half (size=01):                      COP Byte (size=00):
 31  27 26  22 21  17 16 13 12 8          31  26 25  22 21  18 17 14 13 8          31  25 24  22 21  19 18 15 14 8
┌──────┬──────┬──────┬──┬──┬────┐        ┌──────┬──────┬──────┬──┬──┬────┐        ┌──────┬──────┬──────┬──┬──┬────┐
│cop_id│addr_h│addr_l│0 │RW│ rd │        │cop_id│addr_h│addr_l│0 │RW│ rd │        │cop_id│addr_h│addr_l│0 │RW│ rd │
│ 5b   │ 5b   │ 5b   │3b│1b│ 5b │        │ 6b   │ 4b   │ 4b   │3b│1b│ 6b │        │ 7b   │ 3b   │ 3b   │3b│1b│ 7b │
└──────┴──────┴──────┴──┴──┴────┘        └──────┴──────┴──────┴──┴──┴────┘        └──────┴──────┴──────┴──┴──┴────┘
 27     22     17     14 13  8             26     22     18     15 14  8             25     22     19     16 15  8
 opcode=0x12                              opcode=0x12                              opcode=0x12
```

### 3.3 指令规格

| 指令 | cop_id 宽度 | addr 宽度 | 地址空间 | 语义 |
|------|-----------|----------|---------|------|
| `cop.ld.w rd, cop_id, addr` | 5b | 10b | 1024 | rd = coproc[cop_id][addr] (32-bit) |
| `cop.st.w src, cop_id, addr` | 5b | 10b | 1024 | coproc[cop_id][addr] = src |
| `cop.ld.h rd, cop_id, addr` | 6b | 8b | 256 | rd = coproc[cop_id][addr] (16-bit) |
| `cop.st.h src, cop_id, addr` | 6b | 8b | 256 | coproc[cop_id][addr] = src |
| `cop.ld.b rd, cop_id, addr` | 7b | 6b | 64 | rd = coproc[cop_id][addr] (8-bit) |
| `cop.st.b src, cop_id, addr` | 7b | 6b | 64 | coproc[cop_id][addr] = src |

### 3.4 与 demo.cpp 的映射

```
demo.cpp 操作                              COP 指令
─────────────────────────────────────────────────────────────
read_linear_table(IPAT, port_id, &ipat)    cop.ld.w rX, 0, port_id

create_icib_key(port_id, vlan_id)          cop.st.w rX, 1, 0
  → key->port_id = ...                       (src 寄存器预装 key 值)
  → key->vlan_id = ...

create_station_key(eth_hdr, vlan_id)       cop.st.w rX, 2, 0
  → key->dmac[0..5] = ...                    (src 寄存器预装 DMAC+VLAN)
  → key->vlan_id = ...
```

### 3.5 TSFlags 编码

```
TSFlags[13:8]  = 0x12   (硬件 opcode)
TSFlags[7:6]   = size   (0=Byte, 1=Half, 2=Word)
TSFlags[14]    = 0=LD, 1=ST  (读写标志)
```

Emitter 在运行时读取 TSFlags[14] 决定 `c_bit`：

```cpp
bool IsStore = (Desc.TSFlags >> 14) & 1;
if (IsStore) Binary |= (1 << CBitShift);
```

---

## 4. 完整指令集

### 4.1 操作码分配表

```
0x00 ADD    0x01 ADDI   0x02 SUB    0x03 SUBI
0x04 AND    0x05 ANDI   0x06 OR     0x07 ORI
0x08 XOR    0x09 XORI   0x0A NOT
0x0C SHL    0x0D SHLI   0x0E SHR    0x0F SHRI
0x10 SAR    0x11 SARI
0x12 COP    0x13 MOVI   0x14 LD     0x15 ST
0x16 CMP    0x17 CMPI
0x18 JMP    0x19 JALR   0x1A BZ     0x1B BNZ
0x1E CALL   0x1F RET
0x22 BSET   0x23 BCLR   0x24 BNOT   0x25 BTST
0x26 NOP    0x27 HALT
0x28 MOV
0x1C 0x1D 0x29–0x3F  ★ 可用 (22 个)
```

### 4.2 三种粒度支持矩阵

```
                        Word           Half           Byte
──────────────────────────────────────────────────────────
算术 RRR                ✅ ADDW...     ✅ ADDH...     ✅ ADDB...
算术 RRI                ✅ ADDIW...    ❌             ❌
取反 NOT                ✅ NOTW       ✅ NOTH        ✅ NOTB
移位 (寄存器)            ✅ SHLW/SHRW  ✅ SHLH/SHRH   ✅ SHLB/SHRB
                        /SARW          /SARH          /SARB
移位 (立即数)            ✅ SHLIW...   ✅ SHLIH...    ✅ SHLIB...
协处理器 COP             ✅ COPLDW/STW ✅ COPLDH/STH  ✅ COPLDB/STB
传送 MOV (0x28)          ✅ MOVW       ✅ MOVH        ✅ MOVB
传送 MOVI               ✅ MOVIW      ❌             ❌
访存 LD/ST              ✅ LDW/STW    ✅ LDH/STH     ✅ LDB/STB
比较 CMP                ✅ CMPW       ✅ CMPH        ✅ CMPB
比较 CMPI               ✅ CMPIW      ❌             ❌
位操作 BSET/BCLR/BNOT/BTST ✅         ✅             ✅
跳转 JMP/JALR/BZ/BNZ/CALL/RET ✅      ❌             ❌
```

### 4.3 设计原则

1. **opcode 复用**：同一条 opcode + 不同 size = 不同粒度的同一操作。硬件按 `{opcode[5:0], size[1:0]}` 解码。
2. **二操作数算术**：`rd = rd OP rs2[msb:lsb]`，rd 既是源又是目标（TableGen `Constraints = "$rd = $rd_in"`）。
3. **fieldA/B/C 语义可变**：同一 bit 位置，opcode 不同则解释不同（rs2/cop_id/base/offset/bitpos…）。
4. **ST/COPST 复用 rd 字段**：存储指令无目标寄存器，rd 位置被复用作源寄存器或地址高位。

---

## 5. LLVM 后端实现

### 5.1 关键文件

| 文件 | 职责 | 改动频率 |
|------|------|---------|
| `MyCPUInstrFormats.td` | TableGen 格式定义（字段布局、基类） | 高 |
| `MyCPUInstrInfo.td` | TableGen 指令定义（def + Pat） | 高 |
| `MyCPUMCCodeEmitter.cpp` | C++ 手工编码器（按 opcode 分支） | 高 |
| `MyCPUAsmParser.cpp` | C++ 手写汇编器（助记符→MCInst） | 中 |
| `MyCPUDissassembler.cpp` | C++ 手写反汇编器（机器码→MCInst） | 中 |
| `MyCPUAsmPrinter.cpp` | C++ 汇编打印（伪指令展开） | 中 |
| `MyCPUISelDAGToDAG.cpp` | C++ 指令选择（SDNode→MachineInstr） | 中 |
| `MyCPUISelLowering.cpp` | C++ 合法化（类型转换、调用约定） | 中 |

### 5.2 添加新指令的步骤

1. **`MyCPUInstrFormats.td`** — 如需新字段布局，定义新 base class
2. **`MyCPUInstrInfo.td`** — `def` 指令 + `Pat<>` 模式
3. **`MyCPUMCCodeEmitter.cpp`** — 如果操作数布局与现有模式不同，加 opcode 分支
4. **`MyCPUAsmParser.cpp`** — 加助记符 + 解析/emit case
5. **`MyCPUDissassembler.cpp`** — 加反汇编 case
6. **编译验证**：`ninja -C build LLVMMyCPUCodeGen LLVMMyCPUDesc llvm-mc`

### 5.3 Emitter 编码流程

```
1. 从 TSFlags 提取 HWOpcode (bits[13:8]) 和 SizeField (bits[7:6])
2. Binary = HWOpcode | (SizeField << 6)
3. 根据 SizeField 选择字段掩码和位移 (RdMask/RsMask/MsbMask/LsbMask/…)
4. 按 HWOpcode 分发不同编码逻辑:
   ├── 0x15 (ST):     src→fieldA, base→fieldB/rd, offset 拆分
   ├── 0x14 (LD):     rd→rd, base→fieldA, offset→fieldB+C
   ├── 0x12 (COP):    reg→rd, cop_id→fieldA, addr→fieldB+C, c_bit=R/W
   ├── 0x25 (BTST):   rd→rd, bitpos→fieldA, f→f_field
   ├── 0x18/0x1A/0x1B/0x1E (BR): offset 或 f+offset
   └── default (RRR/RRI/SHIFT/…): 按操作数索引处理
5. 对表达式操作数: 创建 MCFixup
6. 返回 32-bit Binary
```

### 5.4 调试命令

```bash
# 汇编单条指令
echo 'cop.ld.w r1, 3, 256' | ./build/bin/llvm-mc -triple=mycpu-unknown-elf -show-encoding

# 反汇编
echo '0x92 0x06 0x00 0x1a' | ./build/bin/llvm-mc -triple=mycpu-unknown-elf -disassemble

# 编译 C 代码
echo 'int f(int x){return x+1;}' | ./build/bin/clang -target mycpu-unknown-elf -S -o - -x c -

# 编译单个组件
ninja -C build LLVMMyCPUCodeGen
```

---

## 6. 已知限制

| # | 问题 | 影响 |
|---|------|------|
| 1 | Half/Byte 的 RRI 指令（ADDIH/ADDIB 等）未实现 | i16/i8 立即数运算需 Word 展开 |
| 2 | Half/Byte 的分支指令未实现 | sub-word 条件分支需 Word 展开 |
| 3 | GPRDecodeTable 仅 32 项，Byte 格式 Rd 为 7 位（编码 >31 的寄存器在反汇编时 crash） | 使用 r15+ 的 Byte 指令无法反汇编 |
| 4 | 帧栈分配大偏移量 bug | 部分函数栈帧分配异常 |
| 5 | sub-word store trunc 模式不全 | 部分 i8/i16 store 无法 ISel |

---

## 7. 示例：完整编码解剖

```
cop.st.b  r1, 2, 63
  hex: 12 86 f8 05      → 32-bit LE: 0x05f88612

  0000 0101  1111 1000  1000 0110  0001 0010
  └────┬───┘└──┬──┘└─┬──┘└─┬──┘└──┬──┘ └─┬──┘└─┬──┘
  cop_id=2  111   111   000    1   0000110  00  010010
  [31:25]  [24:22][21:19][18:16][15] [14:8] [7:6][5:0]
   fieldA=2 fieldB=7 fieldC=7 f=0  c=1  rd=6=A0=r1 size=00 opcode=0x12
         addr = (7<<3)|7 = 63             ↑ STORE (c_bit=1)
```

字段逐一对应，无歧义。
