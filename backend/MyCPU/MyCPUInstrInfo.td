//===-- MyCPUInstrInfo.td - MyCPU Instruction Definitions -----*- tablegen -*-===//
//
// ★★ 这是添加新指令的主要文件 ★★
//
// 每条指令分为两部分:
//   1. 指令定义 (def XXXW : InstW_XXX<...>) — 声明汇编格式和编码
//   2. 模式匹配 (def : Pat<...>) — 将 LLVM IR 操作映射到该指令
//
// ★ 添加新指令的步骤:
//   1. 在本文档中选择合适的 InstW_XXX class
//   2. 编写 def 定义操作码、操作数和汇编格式
//   3. 添加 Pat<> 让 LLVM 自动生成该指令(可选，也可以手写 lowering)
//   4. 如果是新 opcode，更新 MyCPUBaseInfo.h 中的注释表
//   5. 重新编译: ninja LLVMMyCPUCodeGen
//
// ★ 操作码分配 (opcode 6 位 = 0x00-0x3F):
//   0x00 ADD    0x01 ADDI    0x02 SUB    0x03 SUBI
//   0x04 AND    0x05 ANDI    0x06 OR     0x07 ORI
//   0x08 XOR    0x09 XORI    0x0A NOT
//   0x0C SHL    0x0D SHLI    0x0E SHR    0x0F SHRI
//   0x10 SAR    0x11 SARI
//   0x12 MOVAB  0x13 MOVI    0x14 LD      0x15 ST
//   (寄存器传送)             (协处理器读)  (协处理器写)
//   0x16 CMP    0x17 CMPI
//   0x18 JMP    0x19 JALR    0x1A BZ     0x1B BNZ
//   (BC/BNC 预留)            0x1E CALL   0x1F RET
//   0x20 (预留) 0x21 (预留)
//   0x22 BSET   0x23 BCLR    0x24 BNOT   0x25 BTST
//   0x26 NOP    0x27 HALT
//   0x28 MOV    ★ 内存访存 (c_bit=0→Load, c_bit=1→Store)
//   0x29-0x3F  ★ 预留，可添加新指令
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// ★ 伪指令 (Pseudo) — 仅用于代码生成，不对应真实硬件指令
//    这些指令会在寄存器分配前被展开
//===----------------------------------------------------------------------===//

let isPseudo = 1 in {
  // PseudoCMP: SelectionDAG 中的 setcc 节点通过此伪指令产生 0/1 结果
  // 展开后在 ISelDAGToDAG 或 ExpandPseudo pass 中被替换
  def PseudoCMP : Pseudo<(outs GPR:$rd),
    (ins GPR:$rs1, GPR:$rs2, i32imm:$cond),
    "", []>;
}

//===----------------------------------------------------------------------===//
// 算术指令 — 寄存器版 (RRR 格式)
//
// ★ 特点: rd = rd OP rs2[msb:lsb] (二操作数)
//   ADD 举例: R8 = R8 + R9[31:0] → 需要先 MOV R8, R10 再 ADD R8, R9[31:0]
//   全字运算: msb=31, lsb=0
//   位域运算: msb/lsb 指定 rs2 中参与运算的位范围
//
// ★ 修改 msb/lsb 默认值: 在 Pat<> 的第三个/第四个参数中修改
//===----------------------------------------------------------------------===//

// ADD: rd = rd + rs2[msb:lsb] + c_bit
// ★ 要修改操作码: 改第一个参数 0x00
// ★ 要修改汇编格式: 改 asmstr 字符串，$rd/$rs2 是操作数占位符
def ADDW : InstW_RRR<0x00, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u5imm:$msb, u5imm:$lsb),
  "add.w\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0;      // ★ 不使用 Freg 标志
  let c_bit = 0;  // ★ 无进位输入
}
// ★ Pattern: 当 LLVM IR 中出现 (add x, y) 时自动匹配此指令
//   第三个参数 31 = msb (全字高位), 第四个参数 0 = lsb (全字低位)
def : Pat<(add i32:$rd, i32:$rs2), (ADDW GPR:$rd, GPR:$rs2, 31, 0)>;

// SUB: rd = rd - rs2[msb:lsb] - c_bit
def SUBW : InstW_RRR<0x02, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u5imm:$msb, u5imm:$lsb),
  "sub.w\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0;
  let c_bit = 0;
}
def : Pat<(sub i32:$rd, i32:$rs2), (SUBW GPR:$rd, GPR:$rs2, 31, 0)>;

// AND: rd = rd & rs2[msb:lsb]
def ANDW : InstW_RRR<0x04, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u5imm:$msb, u5imm:$lsb),
  "and.w\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0;
  let c_bit = 0;
}
def : Pat<(and i32:$rd, i32:$rs2), (ANDW GPR:$rd, GPR:$rs2, 31, 0)>;

// OR: rd = rd | rs2[msb:lsb]
def ORW : InstW_RRR<0x06, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u5imm:$msb, u5imm:$lsb),
  "or.w\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0;
  let c_bit = 0;
}
def : Pat<(or i32:$rd, i32:$rs2), (ORW GPR:$rd, GPR:$rs2, 31, 0)>;

// XOR: rd = rd ^ rs2[msb:lsb]
def XORW : InstW_RRR<0x08, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u5imm:$msb, u5imm:$lsb),
  "xor.w\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0;
  let c_bit = 0;
}
def : Pat<(xor i32:$rd, i32:$rs2), (XORW GPR:$rd, GPR:$rs2, 31, 0)>;

// NOT: rd = ~rs2[msb:lsb]
// ★ 注意: NOT 不使用 rd 的原值，所以不用 InstW_RRR (无 tied operand 约束)
//   而是直接用 InstW 手动设置字段
def NOTW : InstW<0x0A, (outs GPR:$rd), (ins GPR:$rs2, u5imm:$msb, u5imm:$lsb),
  "not.w\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0;
  let c_bit = 0;
  bits<5> rs2;
  bits<5> msb;
  bits<5> lsb;
  let fieldA = rs2;
  let fieldB = msb;
  let fieldC = lsb;
}
def : Pat<(not i32:$rs2), (NOTW GPR:$rs2, 31, 0)>;

//===----------------------------------------------------------------------===//
// 算术指令 — 立即数版 (RRI 格式)
//
// ★ 15 位有符号立即数: 范围 -16384 ~ 16383
//   超出范围的立即数由 LLVM 自动拆分为多条指令
//   例如: 加载 0x12345 会变成 MOVIW + SHLI + ORIW 序列
//===----------------------------------------------------------------------===//

// ADDI: rd = rd + sign_extend(imm15) + c_bit
def ADDIW : InstW_RRI<0x01, (outs GPR:$rd), (ins GPR:$rd_in, simm15:$imm),
  "addi.w\t$rd, $rd, $imm", []> {
  let f = 0;
  let c_bit = 0;
}
def : Pat<(add i32:$rd, imm:$imm), (ADDIW GPR:$rd, simm15:$imm)>;

// SUBI: rd = rd - sign_extend(imm15) - c_bit
def SUBIW : InstW_RRI<0x03, (outs GPR:$rd), (ins GPR:$rd_in, simm15:$imm),
  "subi.w\t$rd, $rd, $imm", []> {
  let f = 0;
  let c_bit = 0;
}

// ANDI: rd = rd & zero_extend(imm15)
def ANDIW : InstW_RRI<0x05, (outs GPR:$rd), (ins GPR:$rd_in, simm15:$imm),
  "andi.w\t$rd, $rd, $imm", []> {
  let f = 0;
  let c_bit = 0;
}
def : Pat<(and i32:$rd, imm:$imm), (ANDIW GPR:$rd, simm15:$imm)>;

// ORI: rd = rd | zero_extend(imm15)
def ORIW : InstW_RRI<0x07, (outs GPR:$rd), (ins GPR:$rd_in, simm15:$imm),
  "ori.w\t$rd, $rd, $imm", []> {
  let f = 0;
  let c_bit = 0;
}
def : Pat<(or i32:$rd, imm:$imm), (ORIW GPR:$rd, simm15:$imm)>;

// XORI: rd = rd ^ zero_extend(imm15)
def XORIW : InstW_RRI<0x09, (outs GPR:$rd), (ins GPR:$rd_in, simm15:$imm),
  "xori.w\t$rd, $rd, $imm", []> {
  let f = 0;
  let c_bit = 0;
}
def : Pat<(xor i32:$rd, imm:$imm), (XORIW GPR:$rd, simm15:$imm)>;

//===----------------------------------------------------------------------===//
// 移位指令
//
// ★ SHL/SHR/SAR: rd = rd OP rs2 (rs2 低 5 位作为移位量)
// ★ SHLI/SHRI/SARI: rd = rd OP shamt (shamt 为 5 位立即数)
//===----------------------------------------------------------------------===//

def SHLW : InstW_SHIFT<0x0C, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2),
  "shl.w\t$rd, $rd, $rs2", []> {
  let f = 0;
  let c_bit = 0;
}
def : Pat<(shl i32:$rd, i32:$rs2), (SHLW GPR:$rd, GPR:$rs2)>;

def SHRW : InstW_SHIFT<0x0E, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2),
  "shr.w\t$rd, $rd, $rs2", []> {
  let f = 0;
  let c_bit = 0;
}
def : Pat<(srl i32:$rd, i32:$rs2), (SHRW GPR:$rd, GPR:$rs2)>;

def SARW : InstW_SHIFT<0x10, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2),
  "sar.w\t$rd, $rd, $rs2", []> {
  let f = 0;
  let c_bit = 0;
}
def : Pat<(sra i32:$rd, i32:$rs2), (SARW GPR:$rd, GPR:$rs2)>;

// --- Half 寄存器移位 (size=01) ---
def SHLH : InstH_SHIFT<0x0C, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2),
  "shl.h\t$rd, $rd, $rs2", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(shl i16:$rd, i16:$rs2), (SHLH GPR:$rd, GPR:$rs2)>;

def SHRH : InstH_SHIFT<0x0E, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2),
  "shr.h\t$rd, $rd, $rs2", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(srl i16:$rd, i16:$rs2), (SHRH GPR:$rd, GPR:$rs2)>;

def SARH : InstH_SHIFT<0x10, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2),
  "sar.h\t$rd, $rd, $rs2", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(sra i16:$rd, i16:$rs2), (SARH GPR:$rd, GPR:$rs2)>;

// --- Byte 寄存器移位 (size=00) ---
def SHLB : InstB_SHIFT<0x0C, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2),
  "shl.b\t$rd, $rd, $rs2", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(shl i8:$rd, i8:$rs2), (SHLB GPR:$rd, GPR:$rs2)>;

def SHRB : InstB_SHIFT<0x0E, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2),
  "shr.b\t$rd, $rd, $rs2", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(srl i8:$rd, i8:$rs2), (SHRB GPR:$rd, GPR:$rs2)>;

def SARB : InstB_SHIFT<0x10, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2),
  "sar.b\t$rd, $rd, $rs2", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(sra i8:$rd, i8:$rs2), (SARB GPR:$rd, GPR:$rs2)>;

def SHLIW : InstW_SHIFTI<0x0D, (outs GPR:$rd), (ins GPR:$rd_in, u5imm:$shamt),
  "shli.w\t$rd, $rd, $shamt", []> {
  let f = 0;
  let c_bit = 0;
}

def SHRIW : InstW_SHIFTI<0x0F, (outs GPR:$rd), (ins GPR:$rd_in, u5imm:$shamt),
  "shri.w\t$rd, $rd, $shamt", []> {
  let f = 0;
  let c_bit = 0;
}

def SARIW : InstW_SHIFTI<0x11, (outs GPR:$rd), (ins GPR:$rd_in, u5imm:$shamt),
  "sari.w\t$rd, $rd, $shamt", []> {
  let f = 0;
  let c_bit = 0;
}

// --- Half 立即移位 (size=01) ---
def SHLIH : InstH_SHIFTI<0x0D, (outs GPR:$rd), (ins GPR:$rd_in, u4imm:$shamt),
  "shli.h\t$rd, $rd, $shamt", []> {
  let f = 0; let c_bit = 0;
}
def SHRIH : InstH_SHIFTI<0x0F, (outs GPR:$rd), (ins GPR:$rd_in, u4imm:$shamt),
  "shri.h\t$rd, $rd, $shamt", []> {
  let f = 0; let c_bit = 0;
}
// (srl i16 imm handled in ISel)
def SARIH : InstH_SHIFTI<0x11, (outs GPR:$rd), (ins GPR:$rd_in, u4imm:$shamt),
  "sari.h\t$rd, $rd, $shamt", []> {
  let f = 0; let c_bit = 0;
}
// (sra i16 imm handled in ISel)

// --- Byte 立即移位 (size=00) ---
def SHLIB : InstB_SHIFTI<0x0D, (outs GPR:$rd), (ins GPR:$rd_in, u3imm:$shamt),
  "shli.b\t$rd, $rd, $shamt", []> {
  let f = 0; let c_bit = 0;
}
// (shl i8 imm handled in ISel)
def SHRIB : InstB_SHIFTI<0x0F, (outs GPR:$rd), (ins GPR:$rd_in, u3imm:$shamt),
  "shri.b\t$rd, $rd, $shamt", []> {
  let f = 0; let c_bit = 0;
}
// (srl i8 imm handled in ISel)
def SARIB : InstB_SHIFTI<0x11, (outs GPR:$rd), (ins GPR:$rd_in, u3imm:$shamt),
  "sari.b\t$rd, $rd, $shamt", []> {
  let f = 0; let c_bit = 0;
}
// (sra i8 imm handled in ISel)

//===----------------------------------------------------------------------===//
// 数据传送指令
//
// ★ COP:  cop.ld/cop.st 协处理器访存 (opcode 0x12, c_bit 区分读写)
// ★ MOV:  rd = rs2[msb:lsb]  寄存器传送 (opcode 0x28)
// ★ MOVI: rd = imm15         加载 15 位立即数到寄存器 (opcode 0x13)
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// 协处理器读写指令 (LD/ST) — opcode 0x14=LD, 0x15=ST
//
// ★ opcode 0x14 (LD) → 协处理器读: CPU寄存器 ← 协处理器
// ★ opcode 0x15 (ST) → 协处理器写: CPU寄存器 → 协处理器
//
// 操作数: cop_id=协处理器编号, addr=协处理器内部地址
//===----------------------------------------------------------------------===//

def COPLDW : InstW_COPLD<0x14, (outs GPR:$rd), (ins i32imm:$cop_id, i32imm:$addr),
  "ld.w\t$rd, $cop_id, $addr", []>;
def COPSTW : InstW_COPST<0x15, (outs), (ins GPR:$src, i32imm:$cop_id, i32imm:$addr),
  "st.w\t$src, $cop_id, $addr", []>;

//===----------------------------------------------------------------------===//
// 寄存器传送指令 (MOVAB) — opcode 0x12
//===----------------------------------------------------------------------===//

def MOVABW : InstW_MOV<0x12, (outs GPR:$rd), (ins GPR:$rs2),
  "movab.w\t$rd, $rs2", []>;
def : Pat<(i32 GPR:$rs2), (MOVABW GPR:$rs2)>;

// MOVIW: rd = imm15  (Word 立即数加载, opcode 0x13)
def MOVIW : InstW_RRI<0x13, (outs GPR:$rd), (ins simm15:$imm),
  "movi.w\t$rd, $imm", [(set i32:$rd, imm:$imm)]> {
  let f = 0;
  let c_bit = 0;
  let Constraints = "";
}
// MOVIH: rd = imm14  (Half 立即数加载, opcode 0x13, size=01)
def MOVIH : InstH_RRI<0x13, (outs GPR:$rd), (ins i32imm:$imm),
  "movi.h\t$rd, $imm", []> {
  let f = 0;
  let c_bit = 0;
  let Constraints = "";
}
// MOVIB: rd = imm13  (Byte 立即数加载, opcode 0x13, size=00)
def MOVIB : InstB_RRI<0x13, (outs GPR:$rd), (ins i32imm:$imm),
  "movi.b\t$rd, $imm", []> {
  let f = 0;
  let c_bit = 0;
  let Constraints = "";
}

//===----------------------------------------------------------------------===//
// 内存访存指令 — opcode 0x28 (MOV), c_bit 区分方向
//
// ★ LD (c_bit=0): rd = mem[base + offset10]   从内存加载
// ★ ST (c_bit=1): mem[base + offset10] = src  存储到内存
// ★ offset 为 10 位有符号立即数，由 LLVM 的 frame lowering 和合法化处理
//===----------------------------------------------------------------------===//

def LDW : InstW_LD<0x28, (outs GPR:$rd), (ins GPR:$base, simm10:$offset),
  "mov.w\t$rd, [$base + $offset]", []> {
  let f = 0;
}
def : Pat<(i32 (load (add GPR:$base, imm:$offset))), (LDW GPR:$base, imm:$offset)>;
def : Pat<(i32 (load GPR:$base)), (LDW GPR:$base, 0)>;

def STW : InstW_ST<0x28, (outs), (ins GPR:$src, GPR:$base, simm10:$offset),
  "mov.w\t$src, [$base + $offset]", []> {
  let f = 0;
}
def : Pat<(store (i32 GPR:$src), (add GPR:$base, imm:$offset)), (STW GPR:$src, GPR:$base, imm:$offset)>;
def : Pat<(store (i32 GPR:$src), GPR:$base), (STW GPR:$src, GPR:$base, 0)>;
// ★ 编译器输出用 mov.w，手工汇编 store 也用 mov.w（同语法）

//===----------------------------------------------------------------------===//
// 比较指令 (CMP/CMPI)
//
// ★ CMP:  Freg[f] = flags(rd - rs2[msb:lsb])
//   比较结果写入指定 Freg 的 Z/C/N/V 标志位
//   Z=1 表示相等, C/N/V 用于大小比较
//===----------------------------------------------------------------------===//

def CMPW : InstW_RRR<0x16, (outs), (ins GPR:$rd, GPR:$rs2, u5imm:$msb, u5imm:$lsb, u3imm:$f),
  "cmp.w\t$rd, $rs2[$msb:$lsb], $f", []> {
  let c_bit = 0;
  // ★ CMP 没有 GPR 输出，不需要 Constraints
  let Constraints = "";
}

def CMPIW : InstW_RRI<0x17, (outs), (ins GPR:$rd, simm15:$imm, u3imm:$f),
  "cmpi.w\t$rd, $imm, $f", []> {
  let c_bit = 0;
  let Constraints = "";
}

//===----------------------------------------------------------------------===//
// 分支指令
//
// ★ JMP:  无条件跳转, 15 位 PC 相对偏移
// ★ JALR: 间接跳转并链接 (rd = PC+4, PC = rs2)
// ★ BZ/BNZ: 条件跳转, 检查 Freg[f].Z 标志
//
// ★ 当前只实现了 BZ/BNZ (相等/不等)
//   大小比较 (BLT/BGE 等) 在 ISelLowering 中通过 CMP+Freg 标志组合实现
//===----------------------------------------------------------------------===//

def JMP : InstW_BR<0x18, (outs), (ins brtarget:$target),
  "jmp\t$target", [(br bb:$target)]> {
  let f = 0;
  let c_bit = 0;
  let isBranch = 1;       // 标记为分支指令
  let isTerminator = 1;   // 标记为基本块终结指令
  let isBarrier = 1;      // 标记为控制流屏障
}

def JALR : InstW_RRR<0x19, (outs GPR:$rd), (ins GPR:$rs2),
  "jalr\t$rd, $rs2", []> {
  let f = 0;
  let c_bit = 0;
  let msb = 0;
  let lsb = 0;
  let Constraints = "";
}

def BZW : InstW_BR<0x1A, (outs), (ins u3imm:$f, brtarget:$target),
  "bz\t$f, $target", []> {
  let c_bit = 0;
  let isBranch = 1;
  let isTerminator = 1;
}

def BNZ : InstW_BR<0x1B, (outs), (ins u3imm:$f, brtarget:$target),
  "bnz\t$f, $target", []> {
  let c_bit = 0;
  let isBranch = 1;
  let isTerminator = 1;
}

//===----------------------------------------------------------------------===//
// 子程序调用/返回
//
// ★ CALL: LR = PC+4; PC = target   (函数调用)
// ★ RET:  PC = LR                  (函数返回)
//===----------------------------------------------------------------------===//

def CALL : InstW_BR<0x1E, (outs), (ins calltarget:$target),
  "call\t$target", []> {
  let f = 0;
  let c_bit = 0;
  let isCall = 1;
  // 注意: CALL 不设 isTerminator，否则 epilogue 会被插入到 call 之前
}

def RET : InstW<0x1F, (outs), (ins),
  "ret", []> {
  let f = 0;
  let c_bit = 0;
  let isReturn = 1;
  let isTerminator = 1;
  let isBarrier = 1;
  let Uses = [A0];   // RET 隐式使用返回值寄存器，阻止返回值的计算被消除
  let fieldA = 0;  let fieldB = 0;  let fieldC = 0;
  let rd = 0;
}

//===----------------------------------------------------------------------===//
// 栈操作
//
// ★ 无专用 PUSH/POP 指令，栈操作通过 SUBIW/ADDIW + STW/LDW 组合实现：
//   PUSH rd → SUBIW SP, SP, 4; STW rd, [SP + 0]
//   POP  rd → LDW rd, [SP + 0]; ADDIW SP, SP, 4
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// 单比特操作指令
//
// ★ BSET: rd[bitpos] = 1          (置位)
// ★ BCLR: rd[bitpos] = 0          (清零)
// ★ BNOT: rd[bitpos] = ~rd[bitpos] (翻转)
// ★ BTST: Freg[f].Z = (rd[bitpos] == 0)  (位测试)
//
// ★ bitpos 范围: 0-31 (5-bit 编码)
//===----------------------------------------------------------------------===//

def BSET : InstW<0x22, (outs GPR:$rd), (ins GPR:$rd_in, u5imm:$bitpos),
  "bset\t$rd, $bitpos", []> {
  bits<5> rd_in;
  bits<5> bitpos;
  let f = 0;
  let c_bit = 0;
  let Constraints = "$rd = $rd_in";   // rd 既是源也是目标
  let fieldA = bitpos;  // bitpos 放在 rs2 字段位置
  let fieldB = 0;
  let fieldC = 0;
}

def BCLR : InstW<0x23, (outs GPR:$rd), (ins GPR:$rd_in, u5imm:$bitpos),
  "bclr\t$rd, $bitpos", []> {
  bits<5> rd_in;
  bits<5> bitpos;
  let f = 0;
  let c_bit = 0;
  let Constraints = "$rd = $rd_in";
  let fieldA = bitpos;
  let fieldB = 0;
  let fieldC = 0;
}

def BNOT : InstW<0x24, (outs GPR:$rd), (ins GPR:$rd_in, u5imm:$bitpos),
  "bnot\t$rd, $bitpos", []> {
  bits<5> rd_in;
  bits<5> bitpos;
  let f = 0;
  let c_bit = 0;
  let Constraints = "$rd = $rd_in";
  let fieldA = bitpos;
  let fieldB = 0;
  let fieldC = 0;
}

def BTST : InstW<0x25, (outs), (ins GPR:$rd, u5imm:$bitpos, u3imm:$f),
  "btst\t$rd, $bitpos, $f", []> {
  bits<5> bitpos;
  let c_bit = 0;
  let fieldA = bitpos;
  let fieldB = 0;
  let fieldC = 0;
}

// --- Half 位操作 (size=01, bitpos: 0-15) ---
def BSETH : InstH<0x22, (outs GPR:$rd), (ins GPR:$rd_in, u4imm:$bitpos),
  "bset.h\t$rd, $bitpos", []> {
  bits<6> rd_in; bits<4> bitpos;
  let f = 0; let c_bit = 0;
  let Constraints = "$rd = $rd_in";
  let fieldA = {0b00, bitpos}; let fieldB = 0; let fieldC = 0;
}
def BCLRH : InstH<0x23, (outs GPR:$rd), (ins GPR:$rd_in, u4imm:$bitpos),
  "bclr.h\t$rd, $bitpos", []> {
  bits<6> rd_in; bits<4> bitpos;
  let f = 0; let c_bit = 0;
  let Constraints = "$rd = $rd_in";
  let fieldA = {0b00, bitpos}; let fieldB = 0; let fieldC = 0;
}
def BNOTH : InstH<0x24, (outs GPR:$rd), (ins GPR:$rd_in, u4imm:$bitpos),
  "bnot.h\t$rd, $bitpos", []> {
  bits<6> rd_in; bits<4> bitpos;
  let f = 0; let c_bit = 0;
  let Constraints = "$rd = $rd_in";
  let fieldA = {0b00, bitpos}; let fieldB = 0; let fieldC = 0;
}
def BTSTH : InstH<0x25, (outs), (ins GPR:$rd, u4imm:$bitpos, u3imm:$f),
  "btst.h\t$rd, $bitpos, $f", []> {
  bits<4> bitpos;
  let c_bit = 0;
  let fieldA = {0b00, bitpos}; let fieldB = 0; let fieldC = 0;
}

// --- Byte 位操作 (size=00, bitpos: 0-7) ---
def BSETB : InstB<0x22, (outs GPR:$rd), (ins GPR:$rd_in, u3imm:$bitpos),
  "bset.b\t$rd, $bitpos", []> {
  bits<7> rd_in; bits<3> bitpos;
  let f = 0; let c_bit = 0;
  let Constraints = "$rd = $rd_in";
  let fieldA = {0b0000, bitpos}; let fieldB = 0; let fieldC = 0;
}
def BCLRB : InstB<0x23, (outs GPR:$rd), (ins GPR:$rd_in, u3imm:$bitpos),
  "bclr.b\t$rd, $bitpos", []> {
  bits<7> rd_in; bits<3> bitpos;
  let f = 0; let c_bit = 0;
  let Constraints = "$rd = $rd_in";
  let fieldA = {0b0000, bitpos}; let fieldB = 0; let fieldC = 0;
}
def BNOTB : InstB<0x24, (outs GPR:$rd), (ins GPR:$rd_in, u3imm:$bitpos),
  "bnot.b\t$rd, $bitpos", []> {
  bits<7> rd_in; bits<3> bitpos;
  let f = 0; let c_bit = 0;
  let Constraints = "$rd = $rd_in";
  let fieldA = {0b0000, bitpos}; let fieldB = 0; let fieldC = 0;
}
def BTSTB : InstB<0x25, (outs), (ins GPR:$rd, u3imm:$bitpos, u3imm:$f),
  "btst.b\t$rd, $bitpos, $f", []> {
  bits<3> bitpos;
  let c_bit = 0;
  let fieldA = {0b0000, bitpos}; let fieldB = 0; let fieldC = 0;
}

//===----------------------------------------------------------------------===//
// 系统指令
//===----------------------------------------------------------------------===//

def NOP : InstW<0x26, (outs), (ins), "nop", []> {
  let f = 0;  let c_bit = 0;
  let fieldA = 0;  let fieldB = 0;  let fieldC = 0;
  let rd = 0;
}

def HALT : InstW<0x27, (outs), (ins), "halt", []> {
  let f = 0;  let c_bit = 0;
  let fieldA = 0;  let fieldB = 0;  let fieldC = 0;
  let rd = 0;
  let isTerminator = 1;
  let isBarrier = 1;
}

//===----------------------------------------------------------------------===//
// 代码生成辅助伪指令
// ★ 这些伪指令在 ExpandPseudo pass 中被展开为真实指令序列
//===----------------------------------------------------------------------===//

// ★ LOAD_IMM: 加载任意 32 位立即数
//   展开策略: 如果 imm 在 15 位范围内 → MOVIW
//            如果超出范围 → MOVIW + SHLIW + ORIW 序列
let isReMaterializable = 1, isAsCheapAsAMove = 1, isMoveImm = 1 in
def LOAD_IMM : Pseudo<(outs GPR:$rd), (ins i32imm:$imm),
  "li\t$rd, $imm", [(set i32:$rd, imm:$imm)]>;

// ★ LOAD_ADDR: 加载全局变量/函数地址
//   展开为 MOVIW 或 MOVIW + ORIW 序列
def LOAD_ADDR : Pseudo<(outs GPR:$rd), (ins i32imm:$addr),
  "la\t$rd, $addr", []>;

// ★ LD_FI: 引用栈帧中的变量 (Frame Index)
//   在 eliminateFrameIndex pass 中被替换为 LDW [SP/FP + offset]
def LD_FI : Pseudo<(outs GPR:$rd), (ins i32imm:$fi),
  "ld.fi\t$rd, $fi", []>;

// ★ 条件分支伪指令 — 展开为 CMP + BZ/BNZ 序列
let isBranch = 1, isTerminator = 1 in {
  def BEQ : Pseudo<(outs), (ins GPR:$rs1, GPR:$rs2, brtarget:$target),
    "beq\t$rs1, $rs2, $target", []>;
  def BNE : Pseudo<(outs), (ins GPR:$rs1, GPR:$rs2, brtarget:$target),
    "bne\t$rs1, $rs2, $target", []>;
  def BLT : Pseudo<(outs), (ins GPR:$rs1, GPR:$rs2, brtarget:$target),
    "blt\t$rs1, $rs2, $target", []>;
  def BGE : Pseudo<(outs), (ins GPR:$rs1, GPR:$rs2, brtarget:$target),
    "bge\t$rs1, $rs2, $target", []>;
}

// ★ SELECT 伪指令 — 展开为 CMP + 条件 MOV 序列
def SELECT : Pseudo<(outs GPR:$rd), (ins GPR:$cond, GPR:$rs1, GPR:$rs2),
  "select\t$rd, $cond, $rs1, $rs2", []>;

// ★ ADJCALLSTACK: 调用前后调整栈指针
//   由 LLVM 框架自动插入，在 eliminateCallFramePseudoInstr 中处理
let Defs = [SP], Uses = [SP] in {
  def ADJCALLSTACKDOWN : Pseudo<(outs), (ins i32imm:$amt),
    "adjcallstackdown\t$amt", []>;
  def ADJCALLSTACKUP : Pseudo<(outs), (ins i32imm:$amt),
    "adjcallstackup\t$amt", []>;
}

// ★ 编译器屏障: 阻止指令调度跨越此点
def COMPILER_BARRIER : Pseudo<(outs), (ins), "", []> {
  let hasSideEffects = 1;
}

//===----------------------------------------------------------------------===//
// Half 操作指令 (size=01, 16-bit)
//
// ★ Half 格式特点:
//   - rd/rs2 为 6 位 (64 个半字寄存器, 当前只用低 32 个)
//   - msb/lsb 各 4 位 (0-15)
//   - 立即数 14 位有符号
//===----------------------------------------------------------------------===//

// ADDH: rd = rd + rs2[msb:lsb] + c_bit  (16-bit halfword)
def ADDH : InstH_RRR<0x00, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u4imm:$msb, u4imm:$lsb),
  "add.h\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(add i16:$rd, i16:$rs2), (ADDH GPR:$rd, GPR:$rs2, 15, 0)>;

// SUBH: rd = rd - rs2[msb:lsb] - c_bit  (16-bit halfword)
def SUBH : InstH_RRR<0x02, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u4imm:$msb, u4imm:$lsb),
  "sub.h\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(sub i16:$rd, i16:$rs2), (SUBH GPR:$rd, GPR:$rs2, 15, 0)>;

// ANDH: rd = rd & rs2[msb:lsb]
def ANDH : InstH_RRR<0x04, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u4imm:$msb, u4imm:$lsb),
  "and.h\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(and i16:$rd, i16:$rs2), (ANDH GPR:$rd, GPR:$rs2, 15, 0)>;

// ORH: rd = rd | rs2[msb:lsb]
def ORH : InstH_RRR<0x06, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u4imm:$msb, u4imm:$lsb),
  "or.h\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(or i16:$rd, i16:$rs2), (ORH GPR:$rd, GPR:$rs2, 15, 0)>;

// XORH: rd = rd ^ rs2[msb:lsb]
def XORH : InstH_RRR<0x08, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u4imm:$msb, u4imm:$lsb),
  "xor.h\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(xor i16:$rd, i16:$rs2), (XORH GPR:$rd, GPR:$rs2, 15, 0)>;

// NOTH: rd = ~rs2[msb:lsb]
def NOTH : InstH<0x0A, (outs GPR:$rd), (ins GPR:$rs2, u4imm:$msb, u4imm:$lsb),
  "not.h\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0; let c_bit = 0;
  bits<6> rs2; bits<4> msb; bits<4> lsb;
  let fieldA = rs2; let fieldB = msb; let fieldC = lsb;
}

// CMPH: Freg[f] = flags(rd - rs2[msb:lsb])
def CMPH : InstH_RRR<0x16, (outs), (ins GPR:$rd, GPR:$rs2, u4imm:$msb, u4imm:$lsb, u3imm:$f),
  "cmp.h\t$rd, $rs2[$msb:$lsb], $f", []> {
  let c_bit = 0; let Constraints = "";
}

// COPLDH: rd = coproc[cop_id][addr]  (16-bit read)
def COPLDH : InstH_COPLD<0x14, (outs GPR:$rd), (ins i32imm:$cop_id, i32imm:$addr),
  "ld.h\t$rd, $cop_id, $addr", []>;
def COPSTH : InstH_COPST<0x15, (outs), (ins GPR:$src, i32imm:$cop_id, i32imm:$addr),
  "st.h\t$src, $cop_id, $addr", []>;

// MOVABH: rd = rs2[15:0]  (halfword move — opcode 0x12)
def MOVABH : InstH_MOV<0x12, (outs GPR:$rd), (ins GPR:$rs2),
  "movab.h\t$rd, $rs2", []>;

// LDH: rd = mem16[base + offset]
def LDH : InstH_LD<0x28, (outs GPR:$rd), (ins GPR:$base, i32imm:$offset),
  "mov.h\t$rd, [$base + $offset]", []> {
  let f = 0;
}
def : Pat<(i16 (load (add GPR:$base, imm:$offset))), (LDH GPR:$base, imm:$offset)>;
def : Pat<(i16 (load GPR:$base)), (LDH GPR:$base, 0)>;

// STH: mem16[base + offset] = src
def STH : InstH_ST<0x28, (outs), (ins GPR:$src, GPR:$base, i32imm:$offset),
  "mov.h\t$src, [$base + $offset]", []> {
  let f = 0;
}
def : Pat<(store (i16 GPR:$src), (add GPR:$base, imm:$offset)), (STH GPR:$src, GPR:$base, imm:$offset)>;
def : Pat<(store (i16 GPR:$src), GPR:$base), (STH GPR:$src, GPR:$base, 0)>;

//===----------------------------------------------------------------------===//
// Byte 操作指令 (size=00, 8-bit)
//
// ★ Byte 格式特点:
//   - rd/rs2 为 7 位 (128 个字节寄存器, 当前只用低 32 个)
//   - msb/lsb 各 3 位 (0-7)
//   - 立即数 14 位有符号
//===----------------------------------------------------------------------===//

// ADDB: rd = rd + rs2[msb:lsb] + c_bit  (8-bit byte)
def ADDB : InstB_RRR<0x00, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u3imm:$msb, u3imm:$lsb),
  "add.b\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(add i8:$rd, i8:$rs2), (ADDB GPR:$rd, GPR:$rs2, 7, 0)>;

// SUBB: rd = rd - rs2[msb:lsb] - c_bit  (8-bit byte)
def SUBB : InstB_RRR<0x02, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u3imm:$msb, u3imm:$lsb),
  "sub.b\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(sub i8:$rd, i8:$rs2), (SUBB GPR:$rd, GPR:$rs2, 7, 0)>;

// ANDB: rd = rd & rs2[msb:lsb]
def ANDB : InstB_RRR<0x04, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u3imm:$msb, u3imm:$lsb),
  "and.b\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(and i8:$rd, i8:$rs2), (ANDB GPR:$rd, GPR:$rs2, 7, 0)>;

// ORB: rd = rd | rs2[msb:lsb]
def ORB : InstB_RRR<0x06, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u3imm:$msb, u3imm:$lsb),
  "or.b\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(or i8:$rd, i8:$rs2), (ORB GPR:$rd, GPR:$rs2, 7, 0)>;

// XORB: rd = rd ^ rs2[msb:lsb]
def XORB : InstB_RRR<0x08, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u3imm:$msb, u3imm:$lsb),
  "xor.b\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0; let c_bit = 0;
}
def : Pat<(xor i8:$rd, i8:$rs2), (XORB GPR:$rd, GPR:$rs2, 7, 0)>;

// NOTB: rd = ~rs2[msb:lsb]
def NOTB : InstB<0x0A, (outs GPR:$rd), (ins GPR:$rs2, u3imm:$msb, u3imm:$lsb),
  "not.b\t$rd, $rs2[$msb:$lsb]", []> {
  let f = 0; let c_bit = 0;
  bits<7> rs2; bits<3> msb; bits<3> lsb;
  let fieldA = rs2; let fieldB = msb; let fieldC = lsb;
}

// CMPB: Freg[f] = flags(rd - rs2[msb:lsb])
def CMPB : InstB_RRR<0x16, (outs), (ins GPR:$rd, GPR:$rs2, u3imm:$msb, u3imm:$lsb, u3imm:$f),
  "cmp.b\t$rd, $rs2[$msb:$lsb], $f", []> {
  let c_bit = 0; let Constraints = "";
}

// COPLDB: rd = coproc[cop_id][addr]  (8-bit read)
def COPLDB : InstB_COPLD<0x14, (outs GPR:$rd), (ins i32imm:$cop_id, i32imm:$addr),
  "ld.b\t$rd, $cop_id, $addr", []>;
def COPSTB : InstB_COPST<0x15, (outs), (ins GPR:$src, i32imm:$cop_id, i32imm:$addr),
  "st.b\t$src, $cop_id, $addr", []>;

// MOVABB: rd = rs2[7:0]  (byte move — opcode 0x12)
def MOVABB : InstB_MOV<0x12, (outs GPR:$rd), (ins GPR:$rs2),
  "movab.b\t$rd, $rs2", []>;

// LDB: rd = mem8[base + offset]
def LDB : InstB_LD<0x28, (outs GPR:$rd), (ins GPR:$base, i32imm:$offset),
  "mov.b\t$rd, [$base + $offset]", []> {
  let f = 0;
}
def : Pat<(i8 (load (add GPR:$base, imm:$offset))), (LDB GPR:$base, imm:$offset)>;
def : Pat<(i8 (load GPR:$base)), (LDB GPR:$base, 0)>;

// STB: mem8[base + offset] = src
def STB : InstB_ST<0x28, (outs), (ins GPR:$src, GPR:$base, i32imm:$offset),
  "mov.b\t$src, [$base + $offset]", []> {
  let f = 0;
}
def : Pat<(store (i8 GPR:$src), (add GPR:$base, imm:$offset)), (STB GPR:$src, GPR:$base, imm:$offset)>;
def : Pat<(store (i8 GPR:$src), GPR:$base), (STB GPR:$src, GPR:$base, 0)>;

//===--- Truncation patterns ---===//
// i32 → i8/i16 截断: MOVABB/MOVABH 只取低位，自然实现截断
def : Pat<(i8 (trunc i32:$src)), (MOVABB GPR:$src)>;
def : Pat<(i8 (trunc i16:$src)), (MOVABB GPR:$src)>;
def : Pat<(i16 (trunc i32:$src)), (MOVABH GPR:$src)>;

//===--- Extending load patterns ---===//
// zext/anyext from i8/i16 memory → i16/i32 register
// LDB loads i8, LDH loads i16; upper bits zero in hardware
def : Pat<(i16 (zext (i8 (load (add GPR:$base, imm:$offset))))),
          (LDB GPR:$base, imm:$offset)>;
def : Pat<(i16 (zext (i8 (load GPR:$base)))), (LDB GPR:$base, 0)>;
def : Pat<(i16 (anyext (i8 (load (add GPR:$base, imm:$offset))))),
          (LDB GPR:$base, imm:$offset)>;
def : Pat<(i16 (anyext (i8 (load GPR:$base)))), (LDB GPR:$base, 0)>;
def : Pat<(i32 (zext (i8 (load (add GPR:$base, imm:$offset))))),
          (LDB GPR:$base, imm:$offset)>;
def : Pat<(i32 (zext (i8 (load GPR:$base)))), (LDB GPR:$base, 0)>;
def : Pat<(i32 (zext (i16 (load (add GPR:$base, imm:$offset))))),
          (LDH GPR:$base, imm:$offset)>;
def : Pat<(i32 (zext (i16 (load GPR:$base)))), (LDH GPR:$base, 0)>;
def : Pat<(i32 (anyext (i8 (load (add GPR:$base, imm:$offset))))),
          (LDB GPR:$base, imm:$offset)>;
def : Pat<(i32 (anyext (i8 (load GPR:$base)))), (LDB GPR:$base, 0)>;
def : Pat<(i32 (anyext (i16 (load (add GPR:$base, imm:$offset))))),
          (LDH GPR:$base, imm:$offset)>;
def : Pat<(i32 (anyext (i16 (load GPR:$base)))), (LDH GPR:$base, 0)>;

//===--- Truncating store patterns ---===//
// trunc i32 → i8/i16 before store: use STB/STH
def : Pat<(truncstore (i8 GPR:$src), (add GPR:$base, imm:$offset)),
          (STB GPR:$src, GPR:$base, imm:$offset)>;
def : Pat<(truncstore (i8 GPR:$src), GPR:$base),
          (STB GPR:$src, GPR:$base, 0)>;
def : Pat<(truncstore (i16 GPR:$src), (add GPR:$base, imm:$offset)),
          (STH GPR:$src, GPR:$base, imm:$offset)>;
def : Pat<(truncstore (i16 GPR:$src), GPR:$base),
          (STH GPR:$src, GPR:$base, 0)>;

//===----------------------------------------------------------------------===//
// ★ 添加新指令的模板示例 ★
//
// 例1: 添加一条新的寄存器-寄存器指令 (操作码 0x29)
//
//   def NEWOPW : InstW_RRR<0x29, (outs GPR:$rd), (ins GPR:$rd_in, GPR:$rs2, u5imm:$msb, u5imm:$lsb),
//     "newop.w\t$rd, $rs2[$msb:$lsb]", []> {
//     let f = 0;
//     let c_bit = 0;
//   }
//
// 例2: 添加一条新的立即数指令 (操作码 0x2A)
//
//   def NEWOPIW : InstW_RRI<0x2A, (outs GPR:$rd), (ins GPR:$rd_in, simm15:$imm),
//     "newopi.w\t$rd, $rd, $imm", []> {
//     let f = 0;
//     let c_bit = 0;
//   }
//
// 例3: 带 SDAG 模式的指令 (LLVM 自动选择)
//
//   def : Pat<(llvm_ir_opcode GPR:$rd, GPR:$rs2),
//             (NEWOPW GPR:$rd, GPR:$rs2, 31, 0)>;
//
// ★ 常见的 LLVM IR SDNode 操作码 (用于 Pat 模式):
//   add, sub, and, or, xor, shl, srl, sra     — 算术逻辑
//   load, store                                 — 访存
//   br, brcond, brtwoway                       — 分支
//   setcc, select                               — 条件/选择
//   frameindex, globaladdress, constant         — 地址/常量
//   call, retflag                               — 调用/返回
//===----------------------------------------------------------------------===//
