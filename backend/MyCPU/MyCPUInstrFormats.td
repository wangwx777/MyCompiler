//===-- MyCPUInstrFormats.td - MyCPU Instruction Formats -----*- tablegen -*-===//
//
// ★★ 这是修改指令格式/编码时最重要的文件 ★★
//
// 所有指令都是 32 位定长，bits[7:6] 为 size，bits[5:0] 为 opcode。
// size 编码: 00=Byte, 01=Half, 10=Word, 11=Reserved
//
// =========================== Word 格式 32 位布局 ===========================
// [31:27] fieldA (5b)  : reg版=rs2索引 / imm版=imm[14:10]
// [26:22] fieldB (5b)  : reg版=msb / imm版=imm[9:5]
// [21:17] fieldC (5b)  : reg版=lsb / imm版=imm[4:0]
// [16:14] f     (3b)   : Freg 索引 (F0-F7)，标志寄存器
// [13]    c_bit (1b)   : 进位输入位
// [12:8]  rd    (5b)   : 目的寄存器 (同时也是算术运算的源操作数)
// [7:6]   size  (2b)   : 操作宽度 00=Byte 01=Half 10=Word
// [5:0]   opcode(6b)   : 操作码 0x00-0x3F
//
// =========================== Half 格式 32 位布局 ===========================
// [31:26] fieldA (6b)  : reg版=rs2索引 / imm版=imm[13:8]
// [25:22] fieldB (4b)  : reg版=msb / imm版=imm[7:4]
// [21:18] fieldC (4b)  : reg版=lsb / imm版=imm[3:0]
// [17:15] f     (3b)   : Freg 索引
// [14]    c_bit (1b)   : 进位输入位
// [13:8]  rd    (6b)   : 目的寄存器 (64个半字寄存器)
// [7:6]   size  (2b)   : 操作宽度 = 01
// [5:0]   opcode(6b)   : 操作码
//
// =========================== Byte 格式 32 位布局 ===========================
// [31:25] fieldA (7b)  : reg版=rs2索引 / imm版=imm[13:7]
// [24:22] fieldB (3b)  : reg版=msb / imm版=imm[6:4]
// [21:19] fieldC (3b)  : reg版=lsb / imm版=imm[3:1]
// [18:16] f     (3b)   : Freg 索引
// [15]    c_bit (1b)   : 进位输入位
// [14:8]  rd    (7b)   : 目的寄存器 (128个字节寄存器)
// [7:6]   size  (2b)   : 操作宽度 = 00
// [5:0]   opcode(6b)   : 操作码
//
// ★ 关键设计：算术指令是二操作数格式 rd = rd OP rs2[msb:lsb]
//   - rd 既做源又做目标，TableGen 中通过 Constraints = "$rd = $rd_in" 绑定
//   - msb/lsb 用于位域提取，全字操作时 msb=31, lsb=0
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// 操作数类型定义
//===----------------------------------------------------------------------===//

// 5 位无符号立即数 (用于 msb/lsb/bitpos 字段)
def u5imm : Operand<i32> {
  let PrintMethod = "printU5Imm";
}

// 4 位无符号立即数 (Half 格式 msb/lsb, 0-15)
def u4imm : Operand<i32> {
  let PrintMethod = "printU4Imm";
}

// 3 位无符号立即数 (Byte 格式 msb/lsb 0-7 或 Freg 索引 0-7)
def u3imm : Operand<i32> {
  let PrintMethod = "printU3Imm";
}

// 15 位有符号立即数 (算术指令立即数, 范围 -16384..16383)
def simm15 : Operand<i32> {
  let PrintMethod = "printSImm15";
}

// 10 位有符号立即数 (LD/ST 偏移量, 范围 -512..511)
def simm10 : Operand<i32> {
  let PrintMethod = "printSImm10";
}

// 分支目标地址 (PC 相对偏移，由汇编器计算)
def brtarget : Operand<OtherVT> {
  let PrintMethod = "printBranchTarget";
}

// 函数调用目标地址
def calltarget : Operand<i32> {
  let PrintMethod = "printCallTarget";
}

//===----------------------------------------------------------------------===//
// 指令基类 — 所有 MyCPU 指令的祖先
//===----------------------------------------------------------------------===//

class MyCPUInst<bits<6> opcode, dag outs, dag ins,
                string asmstr, list<dag> pattern>
    : Instruction {
  field bits<32> Inst;          // 32 位指令编码
  field bits<32> SoftFail = 0;  // 反汇编容错位

  let Namespace = "MyCPU";      // C++ 命名空间前缀

  dag OutOperandList = outs;    // 输出操作数列表
  dag InOperandList = ins;      // 输入操作数列表
  let AsmString   = asmstr;     // 汇编文本格式
  let Pattern     = pattern;    // SelectionDAG 匹配模式

  // 操作码和尺寸始终在固定位置
  bits<2> size_bits = 0b10;          // ★ 默认 Word 格式 (0b10)
  let Inst{5-0}   = opcode;          // ★ opcode 占低 6 位
  let Inst{7-6}   = size_bits;       // ★ size 占 bits[7:6]

  // ★ 将硬件操作码存入 TSFlags，供 MCCodeEmitter 使用
  // MI.getOpcode() 返回 TableGen 内部分配的枚举 ID，不是硬件操作码
  let TSFlags{13-8} = opcode;

  let Itinerary = NoItinerary;       // 默认无调度流水线
}

//===----------------------------------------------------------------------===//
// Word 格式基类 — 32 位操作 (size=10)
//
// ★ 字段布局 (匹配指令集.txt 规格):
//   [31:27] fieldA — rs2 或 imm[14:10]
//   [26:22] fieldB — msb 或 imm[9:5]
//   [21:17] fieldC — lsb 或 imm[4:0]
//   [16:14] f      — Freg 索引 (3b, 0-7)
//   [13]    c_bit  — 进位输入
//   [12:8]  rd     — 目的寄存器 (5b)
//===----------------------------------------------------------------------===//

class InstW<bits<6> opcode, dag outs, dag ins,
            string asmstr, list<dag> pattern>
    : MyCPUInst<opcode, outs, ins, asmstr, pattern> {
  let Inst{7-6} = 0b10;  // size = Word
  let TSFlags{7-6} = 0b10;  // ★ 供 MCCodeEmitter 运行时识别格式

  bits<5>  fieldA;        // [31:27]  rs2 或 imm[14:10]
  bits<5>  fieldB;        // [26:22]  msb 或 imm[9:5]
  bits<5>  fieldC;        // [21:17]  lsb 或 imm[4:0]
  bits<3>  f = 0;         // [16:14]  Freg 索引 (0-7)
  bit      c_bit = 0;     // [13]     进位输入
  bits<5>  rd = 0;        // [12:8]   目的寄存器

  let Inst{31-27} = fieldA;
  let Inst{26-22} = fieldB;
  let Inst{21-17} = fieldC;
  let Inst{16-14} = f;
  let Inst{13}    = c_bit;
  let Inst{12-8}  = rd;
}

//===----------------------------------------------------------------------===//
// Half 格式基类 — 16 位操作 (size=01)
//
// ★ 字段布局:
//   [31:26] fieldA — rs2 或 imm[13:8]   (6b)
//   [25:22] fieldB — msb 或 imm[7:4]    (4b)
//   [21:18] fieldC — lsb 或 imm[3:0]    (4b)
//   [17:15] f      — Freg 索引          (3b)
//   [14]    c_bit  — 进位输入           (1b)
//   [13:8]  rd     — 目的寄存器          (6b, 64个半字寄存器)
//===----------------------------------------------------------------------===//

class InstH<bits<6> opcode, dag outs, dag ins,
            string asmstr, list<dag> pattern>
    : MyCPUInst<opcode, outs, ins, asmstr, pattern> {
  let Inst{7-6} = 0b01;  // size = Half
  let TSFlags{7-6} = 0b01;  // ★ 供 MCCodeEmitter 运行时识别格式

  bits<6>  fieldA;        // [31:26]  rs2 或 imm[13:8]
  bits<4>  fieldB;        // [25:22]  msb 或 imm[7:4]
  bits<4>  fieldC;        // [21:18]  lsb 或 imm[3:0]
  bits<3>  f = 0;         // [17:15]  Freg 索引
  bit      c_bit = 0;     // [14]     进位输入
  bits<6>  rd = 0;        // [13:8]   目的寄存器

  let Inst{31-26} = fieldA;
  let Inst{25-22} = fieldB;
  let Inst{21-18} = fieldC;
  let Inst{17-15} = f;
  let Inst{14}    = c_bit;
  let Inst{13-8}  = rd;
}

//===----------------------------------------------------------------------===//
// Byte 格式基类 — 8 位操作 (size=00)
//
// ★ 字段布局:
//   [31:25] fieldA — rs2 或 imm[13:7]   (7b)
//   [24:22] fieldB — msb 或 imm[6:4]    (3b)
//   [21:19] fieldC — lsb 或 imm[3:1]    (3b)
//   [18:16] f      — Freg 索引          (3b)
//   [15]    c_bit  — 进位输入           (1b)
//   [14:8]  rd     — 目的寄存器          (7b, 128个字节寄存器)
//===----------------------------------------------------------------------===//

class InstB<bits<6> opcode, dag outs, dag ins,
            string asmstr, list<dag> pattern>
    : MyCPUInst<opcode, outs, ins, asmstr, pattern> {
  let Inst{7-6} = 0b00;  // size = Byte
  let TSFlags{7-6} = 0b00;  // ★ 供 MCCodeEmitter 运行时识别格式

  bits<7>  fieldA;        // [31:25]  rs2 或 imm[13:7]
  bits<3>  fieldB;        // [24:22]  msb 或 imm[6:4]
  bits<3>  fieldC;        // [21:19]  lsb 或 imm[3:1]
  bits<3>  f = 0;         // [18:16]  Freg 索引
  bit      c_bit = 0;     // [15]     进位输入
  bits<7>  rd = 0;        // [14:8]   目的寄存器

  let Inst{31-25} = fieldA;
  let Inst{24-22} = fieldB;
  let Inst{21-19} = fieldC;
  let Inst{18-16} = f;
  let Inst{15}    = c_bit;
  let Inst{14-8}  = rd;
}

//===----------------------------------------------------------------------===//
// Word 格式子类 — 具体指令格式
//===----------------------------------------------------------------------===//

// --- 寄存器-寄存器算术格式 (RRR) ---
// 用于: ADD, SUB, AND, OR, XOR
// 语义: rd = rd OP rs2[msb:lsb]
class InstW_RRR<bits<6> opcode, dag outs, dag ins,
                string asmstr, list<dag> pattern>
    : InstW<opcode, outs, ins, asmstr, pattern> {
  bits<5> rs2;   // 源寄存器 2
  bits<5> msb;   // 位域提取高位索引
  bits<5> lsb;   // 位域提取低位索引

  let Constraints = "$rd = $rd_in";  // ★ 二操作数约束
  let fieldA = rs2;
  let fieldB = msb;
  let fieldC = lsb;
}

// --- 寄存器-立即数算术格式 (RRI) ---
// 用于: ADDI, SUBI, ANDI, ORI, XORI, MOVI
// 语义: rd = rd OP sign_extend(imm15)
class InstW_RRI<bits<6> opcode, dag outs, dag ins,
                string asmstr, list<dag> pattern>
    : InstW<opcode, outs, ins, asmstr, pattern> {
  bits<15> imm;   // 15 位有符号立即数

  let Constraints = "$rd = $rd_in";  // ★ 二操作数约束
  let fieldA = imm{14-10};  // imm 高位 → [31:27]
  let fieldB = imm{9-5};    // imm 中位 → [26:22]
  let fieldC = imm{4-0};    // imm 低位 → [21:17]
}

// --- 移位格式 (寄存器移位量) ---
// 用于: SHL, SHR, SAR
// 语义: rd = rd OP rs2     (移位量来自 rs2 低 5 位)
class InstW_SHIFT<bits<6> opcode, dag outs, dag ins,
                  string asmstr, list<dag> pattern>
    : InstW<opcode, outs, ins, asmstr, pattern> {
  bits<5> rs2;   // 移位量来源寄存器

  let Constraints = "$rd = $rd_in";
  let fieldA = rs2;
  let fieldB = 0;
  let fieldC = 0;
}

// --- 移位格式 (立即数移位量) ---
// 用于: SHLI, SHRI, SARI
// 语义: rd = rd OP shamt   (shamt 为 5 位立即数)
class InstW_SHIFTI<bits<6> opcode, dag outs, dag ins,
                   string asmstr, list<dag> pattern>
    : InstW<opcode, outs, ins, asmstr, pattern> {
  bits<5> shamt;           // 5 位移位量 (0-31)
  bits<10> rsv_bits = 0;   // 预留位

  let Constraints = "$rd = $rd_in";
  let fieldA = shamt;             // 移位量放在 rs2 字段位置
  let fieldB = rsv_bits{9-5};    // 预留
  let fieldC = rsv_bits{4-0};    // 预留
}

// --- 寄存器传送格式 (opcode 0x12) ---
// 用于: MOV (rd = rs2[31:0]，全字传送)
class InstW_MOV<bits<6> opcode, dag outs, dag ins,
                string asmstr, list<dag> pattern>
    : InstW<opcode, outs, ins, asmstr, pattern> {
  bits<5> rs2;   // 源寄存器

  let fieldA = rs2;
  let fieldB = 31;  // msb=31 全字
  let fieldC = 0;   // lsb=0
}

// --- 分支格式 ---
// 用于: JMP, BZ, BNZ, CALL
// 15 位 PC 相对偏移（fieldA+B+C 合并）
class InstW_BR<bits<6> opcode, dag outs, dag ins,
               string asmstr, list<dag> pattern>
    : InstW<opcode, outs, ins, asmstr, pattern> {
  bits<15> offset = 0;  // PC 相对偏移, 汇编时填充

  let fieldA = offset{14-10};
  let fieldB = offset{9-5};
  let fieldC = offset{4-0};
}

// --- 访存加载格式 (MOV opcode 0x28, c_bit=0) ---
// rd = mem[base + offset]
class InstW_LD<bits<6> opcode, dag outs, dag ins,
               string asmstr, list<dag> pattern>
    : InstW<opcode, outs, ins, asmstr, pattern> {
  bits<5> base;      // 基址寄存器
  bits<10> offset;   // 10 位偏移量

  let c_bit = 0;     // MOV 访存: 0=Load
  let TSFlags{14} = 0;  // ★ 标记为内存 load
  let fieldA = base;
  let fieldB = offset{9-5};
  let fieldC = offset{4-0};
}

// --- 访存存储格式 (MOV opcode 0x28, c_bit=1) ---
// mem[base + offset] = src
// ★ src 放 fieldA，base 放 fieldB，offset_lo 放 fieldC，offset_hi 放 rd
class InstW_ST<bits<6> opcode, dag outs, dag ins,
               string asmstr, list<dag> pattern>
    : InstW<opcode, outs, ins, asmstr, pattern> {
  bits<5> src;        // 源寄存器 (要存储的值)
  bits<5> base;       // 基址寄存器
  bits<10> offset;    // 10 位偏移量

  let c_bit = 1;     // MOV 访存: 1=Store
  let TSFlags{14} = 1;  // ★ 标记为内存 store
  let fieldA = src;
  let fieldB = base;
  let fieldC = offset{4-0};   // 偏移量低 5 位
  let rd    = offset{9-5};    // 偏移量高 5 位 (复用 rd 字段)
}

//===----------------------------------------------------------------------===//
// Half 格式子类 — 16 位操作
//
// ★ Half 格式的寄存器宽度为 6 位 (64 个半字寄存器)
// ★ 立即数为 14 位有符号 (fieldA[6] + fieldB[4] + fieldC[4])
//===----------------------------------------------------------------------===//

class InstH_RRR<bits<6> opcode, dag outs, dag ins,
                string asmstr, list<dag> pattern>
    : InstH<opcode, outs, ins, asmstr, pattern> {
  bits<6> rs2;
  bits<4> msb;
  bits<4> lsb;

  let Constraints = "$rd = $rd_in";
  let fieldA = rs2;
  let fieldB = msb;
  let fieldC = lsb;
}

class InstH_RRI<bits<6> opcode, dag outs, dag ins,
                string asmstr, list<dag> pattern>
    : InstH<opcode, outs, ins, asmstr, pattern> {
  bits<14> imm;

  let Constraints = "$rd = $rd_in";
  let fieldA = imm{13-8};
  let fieldB = imm{7-4};
  let fieldC = imm{3-0};
}

//===----------------------------------------------------------------------===//
// Byte 格式子类 — 8 位操作
//
// ★ Byte 格式的寄存器宽度为 7 位 (128 个字节寄存器)
// ★ 立即数为 14 位有符号 (fieldA[7] + fieldB[3] + fieldC[3] + 隐含低位)
//===----------------------------------------------------------------------===//

class InstB_RRR<bits<6> opcode, dag outs, dag ins,
                string asmstr, list<dag> pattern>
    : InstB<opcode, outs, ins, asmstr, pattern> {
  bits<7> rs2;
  bits<3> msb;
  bits<3> lsb;

  let Constraints = "$rd = $rd_in";
  let fieldA = rs2;
  let fieldB = msb;
  let fieldC = lsb;
}

class InstB_RRI<bits<6> opcode, dag outs, dag ins,
                string asmstr, list<dag> pattern>
    : InstB<opcode, outs, ins, asmstr, pattern> {
  bits<14> imm;

  let Constraints = "$rd = $rd_in";
  let fieldA = imm{13-7};
  let fieldB = imm{6-4};
  let fieldC = imm{3-1};
}

// --- Half 寄存器传送格式 (opcode 0x12) ---
class InstH_MOV<bits<6> opcode, dag outs, dag ins,
                string asmstr, list<dag> pattern>
    : InstH<opcode, outs, ins, asmstr, pattern> {
  bits<6> rs2;
  let fieldA = rs2;
  let fieldB = 15;  // msb=15 全半字
  let fieldC = 0;   // lsb=0
}

// --- Half 移位格式 (寄存器移位量) ---
class InstH_SHIFT<bits<6> opcode, dag outs, dag ins,
                   string asmstr, list<dag> pattern>
    : InstH<opcode, outs, ins, asmstr, pattern> {
  bits<6> rs2;
  let Constraints = "$rd = $rd_in";
  let fieldA = rs2;
  let fieldB = 0;
  let fieldC = 0;
}

// --- Half 移位格式 (立即数移位量) ---
class InstH_SHIFTI<bits<6> opcode, dag outs, dag ins,
                    string asmstr, list<dag> pattern>
    : InstH<opcode, outs, ins, asmstr, pattern> {
  bits<4> shamt;
  bits<8> rsv_bits = 0;
  let Constraints = "$rd = $rd_in";
  let fieldA = {0b00, shamt};  // zero-extend 4b→6b
  let fieldB = rsv_bits{7-4};
  let fieldC = rsv_bits{3-0};
}

// --- Half 访存加载/存储格式 (MOV opcode 0x28) ---
class InstH_LD<bits<6> opcode, dag outs, dag ins,
               string asmstr, list<dag> pattern>
    : InstH<opcode, outs, ins, asmstr, pattern> {
  bits<6> base;
  bits<8> offset = 0;   // 8 位偏移量 (fieldB 4b + fieldC 4b)
  let c_bit = 0;        // MOV 访存: 0=Load
  let TSFlags{14} = 0;  // ★ 标记为内存 load
  let fieldA = base;
  let fieldB = offset{7-4};
  let fieldC = offset{3-0};
}

class InstH_ST<bits<6> opcode, dag outs, dag ins,
               string asmstr, list<dag> pattern>
    : InstH<opcode, outs, ins, asmstr, pattern> {
  bits<6> src;
  bits<6> base;
  bits<8> offset = 0;
  let c_bit = 1;        // MOV 访存: 1=Store
  let TSFlags{14} = 1;  // ★ 标记为内存 store
  let fieldA = src;
  let fieldB = offset{7-4};
  let fieldC = offset{3-0};
  let rd    = base;     // ★ ST 无目的寄存器，rd 字段复用为 base
}

// --- Byte 寄存器传送格式 (opcode 0x12) ---
class InstB_MOV<bits<6> opcode, dag outs, dag ins,
                string asmstr, list<dag> pattern>
    : InstB<opcode, outs, ins, asmstr, pattern> {
  bits<7> rs2;
  let fieldA = rs2;
  let fieldB = 7;  // msb=7 全字节
  let fieldC = 0;  // lsb=0
}

// --- Byte 移位格式 (寄存器移位量) ---
class InstB_SHIFT<bits<6> opcode, dag outs, dag ins,
                   string asmstr, list<dag> pattern>
    : InstB<opcode, outs, ins, asmstr, pattern> {
  bits<7> rs2;
  let Constraints = "$rd = $rd_in";
  let fieldA = rs2;
  let fieldB = 0;
  let fieldC = 0;
}

// --- Byte 移位格式 (立即数移位量) ---
class InstB_SHIFTI<bits<6> opcode, dag outs, dag ins,
                    string asmstr, list<dag> pattern>
    : InstB<opcode, outs, ins, asmstr, pattern> {
  bits<3> shamt;
  bits<6> rsv_bits = 0;
  let Constraints = "$rd = $rd_in";
  let fieldA = {0b0000, shamt};  // zero-extend 3b→7b
  let fieldB = rsv_bits{5-3};
  let fieldC = rsv_bits{2-0};
}

// --- Byte 访存加载/存储格式 (MOV opcode 0x28) ---
class InstB_LD<bits<6> opcode, dag outs, dag ins,
               string asmstr, list<dag> pattern>
    : InstB<opcode, outs, ins, asmstr, pattern> {
  bits<7> base;
  bits<6> offset = 0;   // 6 位偏移量 (fieldB 3b + fieldC 3b)
  let c_bit = 0;        // MOV 访存: 0=Load
  let TSFlags{14} = 0;  // ★ 标记为内存 load
  let fieldA = base;
  let fieldB = offset{5-3};
  let fieldC = offset{2-0};
}

class InstB_ST<bits<6> opcode, dag outs, dag ins,
               string asmstr, list<dag> pattern>
    : InstB<opcode, outs, ins, asmstr, pattern> {
  bits<7> src;
  bits<7> base;
  bits<6> offset = 0;
  let c_bit = 1;        // MOV 访存: 1=Store
  let TSFlags{14} = 1;  // ★ 标记为内存 store
  let fieldA = src;
  let fieldB = offset{5-3};
  let fieldC = offset{2-0};
  let rd    = base;     // ★ ST 无目的寄存器，rd 字段复用为 base
}

//===----------------------------------------------------------------------===//
// COP 协处理器访存格式 — Word/Half/Byte
//
// ★ 使用 LD (0x14) / ST (0x15) 操作码区分读写
//   opcode 0x14 → cop.ld: rd = coproc[cop_id][addr]
//   opcode 0x15 → cop.st: coproc[cop_id][addr] = src
//
// 字段布局 (复用标准 InstW/InstH/InstB):
//   fieldA = cop_id    (协处理器编号, 5/6/7b)
//   fieldB = addr_hi   (协处理器内部地址高位)
//   fieldC = addr_lo   (协处理器内部地址低位)
//   f      = 0         (预留)
//   c_bit  = 0=LD / 1=ST
//   rd     = 寄存器    (LD=目标, ST=源数据)
//===----------------------------------------------------------------------===//

// --- Word COP Load ---
class InstW_COPLD<bits<6> opcode, dag outs, dag ins,
                   string asmstr, list<dag> pattern>
    : InstW<opcode, outs, ins, asmstr, pattern> {
  bits<5> cop_id;
  bits<10> addr;
  let fieldA = cop_id;
  let fieldB = addr{9-5};
  let fieldC = addr{4-0};
  let f = 0;
  let c_bit = 0;
  let TSFlags{14} = 0;
}

// --- Word COP Store ---
class InstW_COPST<bits<6> opcode, dag outs, dag ins,
                   string asmstr, list<dag> pattern>
    : InstW<opcode, outs, ins, asmstr, pattern> {
  bits<5> src;
  bits<5> cop_id;
  bits<10> addr;
  let fieldA = cop_id;
  let fieldB = addr{9-5};
  let fieldC = addr{4-0};
  let rd = src;
  let f = 0;
  let c_bit = 1;
  let TSFlags{14} = 1;
}

// --- Half COP Load ---
class InstH_COPLD<bits<6> opcode, dag outs, dag ins,
                   string asmstr, list<dag> pattern>
    : InstH<opcode, outs, ins, asmstr, pattern> {
  bits<6> cop_id;
  bits<8> addr;
  let fieldA = cop_id;
  let fieldB = addr{7-4};
  let fieldC = addr{3-0};
  let f = 0;
  let c_bit = 0;
  let TSFlags{14} = 0;
}

// --- Half COP Store ---
class InstH_COPST<bits<6> opcode, dag outs, dag ins,
                   string asmstr, list<dag> pattern>
    : InstH<opcode, outs, ins, asmstr, pattern> {
  bits<6> src;
  bits<6> cop_id;
  bits<8> addr;
  let fieldA = cop_id;
  let fieldB = addr{7-4};
  let fieldC = addr{3-0};
  let rd = src;
  let f = 0;
  let c_bit = 1;
  let TSFlags{14} = 1;
}

// --- Byte COP Load ---
class InstB_COPLD<bits<6> opcode, dag outs, dag ins,
                   string asmstr, list<dag> pattern>
    : InstB<opcode, outs, ins, asmstr, pattern> {
  bits<7> cop_id;
  bits<6> addr;
  let fieldA = cop_id;
  let fieldB = addr{5-3};
  let fieldC = addr{2-0};
  let f = 0;
  let c_bit = 0;
  let TSFlags{14} = 0;
}

// --- Byte COP Store ---
class InstB_COPST<bits<6> opcode, dag outs, dag ins,
                   string asmstr, list<dag> pattern>
    : InstB<opcode, outs, ins, asmstr, pattern> {
  bits<7> src;
  bits<7> cop_id;
  bits<6> addr;
  let fieldA = cop_id;
  let fieldB = addr{5-3};
  let fieldC = addr{2-0};
  let rd = src;
  let f = 0;
  let c_bit = 1;
  let TSFlags{14} = 1;
}

//===----------------------------------------------------------------------===//
// 伪指令基类 — 所有 MyCPU 伪指令的基类
//
// ★ Pseudo 指令特征: isPseudo = 1 (不直接对应硬件编码)
//   在汇编输出或寄存器分配前被展开为真实指令序列
//===----------------------------------------------------------------------===//

class Pseudo<dag outs, dag ins, string asmstr, list<dag> pattern>
    : Instruction {
  let Namespace = "MyCPU";
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let Size = 0;
  dag OutOperandList = outs;
  dag InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;
}
