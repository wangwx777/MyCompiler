//===-- MyCPUMCCodeEmitter.cpp - MyCPU Code Emitter -----------------------===//
//
// ★ 机器码发射器 — 将 MCInst 转换为二进制字节序列 ★
//
// ★ 关键设计：当前使用手动编码方式而非 TableGen 自动生成的编码器
//   这是因为指令格式的灵活性(fieldA/B/C 含义随指令类型变化)
//   使得标准化编码模式较复杂。
//
// 编码流程：
//   1. getBinaryCodeForInstr() — 根据操作数类型构造 32 位指令字
//   2. encodeInstruction()     — 按小端序写入字节
//
// ★ 修改编码布局(如改变字段位置)时需要修改两处：
//   1. 本文件的 getBinaryCodeForInstr() — 编码逻辑
//   2. MyCPUInstrFormats.td — 字段映射声明
//   两个文件中的位位置必须保持一致！
//
// ★ 操作码 ←→ 功能映射 (真实 ISA)：
//   0x12 = MOV (寄存器传送)    0x14 = COPLD (协处理器读)
//   0x15 = COPST (协处理器写)  0x28 = LD/ST (内存访存, c_bit 区分方向)
//
// ★ 警告：编码器按 HWOpcode 手动分发，添加新指令时需要同步更新此处。
//===----------------------------------------------------------------------===//

#include "MyCPUBaseInfo.h"
#include "MCTargetDesc/MyCPUFixupKinds.h"
#include "MCTargetDesc/MyCPUMCTargetDesc.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "mycpu-emitter"

// ★ 包含 TableGen 生成的指令描述信息
#define GET_REGINFO_ENUM
#include "MyCPUGenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#include "MyCPUGenInstrInfo.inc"

namespace {

class MyCPUMCCodeEmitter : public MCCodeEmitter {
public:
  MyCPUMCCodeEmitter(const MCInstrInfo &MCII, MCContext &Ctx)
      : MCII(MCII), Ctx(Ctx) {}

  void encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;

private:
  const MCInstrInfo &MCII;
  MCContext &Ctx;

  // ★ 核心编码函数：返回 32 位指令编码
  uint32_t getBinaryCodeForInstr(const MCInst &MI,
                                  SmallVectorImpl<MCFixup> &Fixups,
                                  const MCSubtargetInfo &STI) const;
};

} // namespace

MCCodeEmitter *llvm::createMyCPUMCCodeEmitter(const MCInstrInfo &MCII,
                                                MCContext &Ctx) {
  return new MyCPUMCCodeEmitter(MCII, Ctx);
}

//===----------------------------------------------------------------------===//
// getBinaryCodeForInstr — 32 位指令字编码
//
// ★ 根据指令格式(Word/Half/Byte)动态选择编码布局
//   格式由 TSFlags[7:6] 标识 (与指令编码的 size 字段一致)
//
// Word 布局: rd[12:8](5b)  rs2[31:27](5b)  msb[26:22](5b)  lsb[21:17](5b)  f[16:14](3b)  c[13]
// Half 布局: rd[13:8](6b)  rs2[31:26](6b)  msb[25:22](4b)  lsb[21:18](4b)  f[17:15](3b)  c[14]
// Byte 布局: rd[14:8](7b)  rs2[31:25](7b)  msb[24:22](3b)  lsb[21:19](3b)  f[18:16](3b)  c[15]
//
// 操作数索引在所有格式中一致:
//   [0]=rd  [1]=rs2/base  [2]=msb/imm  [3]=lsb/imm  [4]=f  [5]=c_bit
//===----------------------------------------------------------------------===//

uint32_t
MyCPUMCCodeEmitter::getBinaryCodeForInstr(const MCInst &MI,
                                            SmallVectorImpl<MCFixup> &Fixups,
                                            const MCSubtargetInfo &STI) const {
  const MCInstrDesc &Desc = MCII.get(MI.getOpcode());
  unsigned NumOps = MI.getNumOperands();

  // Extract hardware opcode and size from TSFlags
  // TSFlags[7:6] = size field, TSFlags[13:8] = hardware opcode
  uint8_t HWOpcode = (Desc.TSFlags >> 8) & 0x3F;
  unsigned SizeField = (Desc.TSFlags >> 6) & 0x3;
  uint32_t Binary = HWOpcode | (SizeField << 6);

  // Field widths and shifts by size
  unsigned RdMask, RdShift, RsMask, RsShift;
  unsigned MsbMask, MsbShift, LsbMask, LsbShift;
  unsigned FMask = 0x7, FShift, CBitShift;
  unsigned LsbWidth; // bits per immediate sub-field

  switch (SizeField) {
  case 2: // Word: 5b reg, 5b sub-fields
    RdMask = 0x1F; RdShift = 8;
    RsMask = 0x1F; RsShift = 27;
    MsbMask = 0x1F; MsbShift = 22;
    LsbMask = 0x1F; LsbShift = 17;
    FShift = 14; CBitShift = 13;
    LsbWidth = 5;
    break;
  case 1: // Half: 6b reg, 4b sub-fields
    RdMask = 0x3F; RdShift = 8;
    RsMask = 0x3F; RsShift = 26;
    MsbMask = 0xF; MsbShift = 22;
    LsbMask = 0xF; LsbShift = 18;
    FShift = 15; CBitShift = 14;
    LsbWidth = 4;
    break;
  case 0: // Byte: 7b reg, 3b sub-fields
  default:
    RdMask = 0x7F; RdShift = 8;
    RsMask = 0x7F; RsShift = 25;
    MsbMask = 0x7; MsbShift = 22;
    LsbMask = 0x7; LsbShift = 19;
    FShift = 16; CBitShift = 15;
    LsbWidth = 3;
    break;
  }
  unsigned MsbWidth = LsbWidth; // symmetric for all formats

  // Lambda: add a fixup for an expression operand
  auto addFixup = [&](unsigned OpIdx, MCFixupKind Kind) {
    Fixups.push_back(
        MCFixup::create(0, MI.getOperand(OpIdx).getExpr(), Kind));
  };


  // === Format dispatch based on hardware opcode ===

  // MOV memory access (0x28): c_bit=0→Load, c_bit=1→Store
  //   TSFlags[14]: 0=Load, 1=Store
  if (HWOpcode == 0x28) {
    bool IsStore = (Desc.TSFlags >> 14) & 1;
    if (IsStore)
      Binary |= (1 << CBitShift);

    if (!IsStore) {
      // === Memory Load: rd→rd, base→fieldA, offset split→fieldB+C ===
      for (unsigned i = 0; i < NumOps; ++i) {
        const MCOperand &MO = MI.getOperand(i);
        if (MO.isReg()) {
          unsigned Reg = MO.getReg();
          if (i == 0)        // rd
            Binary |= ((Reg & RdMask) << RdShift);
          else if (i == 1)   // base → fieldA
            Binary |= ((Reg & RsMask) << RsShift);
        } else if (MO.isImm() && i == 2) {
          int64_t Off = MO.getImm();
          Binary |= ((Off & LsbMask) << LsbShift);
          Binary |= (((Off >> LsbWidth) & MsbMask) << MsbShift);
        } else if (MO.isExpr()) {
          addFixup(i, MyCPU::fixup_MyCPU_16);
        }
      }
    } else {
      // === Memory Store ===
      //   Word:  src→fieldA, base→fieldB, offset[4:0]→fieldC, offset[9:5]→rd
      //   Half/Byte: src→fieldA, base→rd, offset split→fieldB+C
      for (unsigned i = 0; i < NumOps; ++i) {
        const MCOperand &MO = MI.getOperand(i);
        if (MO.isReg()) {
          unsigned Reg = MO.getReg();
          if (i == 0)        // src → fieldA
            Binary |= ((Reg & RsMask) << RsShift);
          else if (i == 1) { // base
            if (SizeField == 2) // Word: base → fieldB
              Binary |= ((Reg & MsbMask) << MsbShift);
            else                 // Half/Byte: base → rd
              Binary |= ((Reg & RdMask) << RdShift);
          }
        } else if (MO.isImm() && i == 2) {
          int64_t Off = MO.getImm();
          if (SizeField == 2) {
            // Word: offset[4:0]→fieldC, offset[9:5]→rd
            Binary |= ((Off & LsbMask) << LsbShift);
            Binary |= (((Off >> LsbWidth) & RdMask) << RdShift);
          } else {
            // Half/Byte: offset split fieldB+C
            Binary |= ((Off & LsbMask) << LsbShift);
            Binary |= (((Off >> LsbWidth) & MsbMask) << MsbShift);
          }
        } else if (MO.isExpr()) {
          addFixup(i, MyCPU::fixup_MyCPU_16);
        }
      }
    }
    return Binary;
  }

  // COP Load (0x14): coprocessor read — reg→rd, cop_id→fieldA, addr→fieldB+C
  if (HWOpcode == 0x14) {
    for (unsigned i = 0; i < NumOps; ++i) {
      const MCOperand &MO = MI.getOperand(i);
      if (MO.isReg()) {
        unsigned Reg = MO.getReg();
        if (Reg != 0)
          Binary |= ((Reg & RdMask) << RdShift);
      } else if (MO.isImm() || MO.isExpr()) {
        int64_t Val;
        if (MO.isExpr()) {
          const MCExpr *E = MO.getExpr();
          if (!E->evaluateAsAbsolute(Val))
            continue;
        } else {
          Val = MO.getImm();
        }
        if (i == 1) {
          Binary |= ((Val & RsMask) << RsShift);
        } else if (i == 2) {
          Binary |= ((Val & LsbMask) << LsbShift);
          Binary |= (((Val >> LsbWidth) & MsbMask) << MsbShift);
        }
      }
    }
    return Binary;
  }

  // COP Store (0x15): coprocessor write — src→rd, cop_id→fieldA, addr→fieldB+C
  if (HWOpcode == 0x15) {
    Binary |= (1 << CBitShift);
    for (unsigned i = 0; i < NumOps; ++i) {
      const MCOperand &MO = MI.getOperand(i);
      if (MO.isReg()) {
        unsigned Reg = MO.getReg();
        if (Reg != 0)
          Binary |= ((Reg & RdMask) << RdShift);
      } else if (MO.isImm() || MO.isExpr()) {
        int64_t Val;
        if (MO.isExpr()) {
          const MCExpr *E = MO.getExpr();
          if (!E->evaluateAsAbsolute(Val))
            continue;
        } else {
          Val = MO.getImm();
        }
        if (i == 1) {
          Binary |= ((Val & RsMask) << RsShift);
        } else if (i == 2) {
          Binary |= ((Val & LsbMask) << LsbShift);
          Binary |= (((Val >> LsbWidth) & MsbMask) << MsbShift);
        }
      }
    }
    return Binary;
  }

  // BTST format (0x25): rd→rd, bitpos→fieldA, f→f field
  //   Layout differs from default RRR path (f is at operand[2], not [4]).
  if (HWOpcode == 0x25) {
    for (unsigned i = 0; i < NumOps; ++i) {
      const MCOperand &MO = MI.getOperand(i);
      if (MO.isReg() && i == 0) {
        unsigned Reg = MO.getReg();
        if (Reg != 0)
          Binary |= ((Reg & RdMask) << RdShift);
      } else if (MO.isImm() || MO.isExpr()) {
        int64_t Val;
        if (MO.isExpr()) {
          const MCExpr *E = MO.getExpr();
          if (!E->evaluateAsAbsolute(Val))
            continue;
        } else {
          Val = MO.getImm();
        }
        if (i == 1) {
          // bitpos → fieldA
          Binary |= ((Val & RsMask) << RsShift);
        } else if (i == 2) {
          // f → Freg index field
          Binary |= ((Val & FMask) << FShift);
        }
      }
    }
    return Binary;
  }

  // BR format (0x18=JMP, 0x1A=BZ, 0x1B=BNZ, 0x1E=CALL):
  //   2 ops: [f, offset_expr]  1 op: [offset_expr]
  // CALL uses PC-relative 15-bit offset (same field layout as JMP).
  if (HWOpcode == 0x18 || HWOpcode == 0x1A ||
      HWOpcode == 0x1B || HWOpcode == 0x1E) {
    for (unsigned i = 0; i < NumOps; ++i) {
      const MCOperand &MO = MI.getOperand(i);
      if (MO.isImm() && i == 0 && NumOps == 2) {
        // BZ/BNZ: first operand is Freg index
        Binary |= ((MO.getImm() & FMask) << FShift);
      } else if (MO.isExpr()) {
        addFixup(i, MyCPU::fixup_MyCPU_PCRel_16);
      }
    }
    return Binary;
  }

  // === DEFAULT format (RRR, RRI, SHIFT, SHIFTI, BITOP, CMP, CMPI, JALR, etc.) ===
  // Operand layout: [0]=rd(reg), [1]=fieldA(reg or imm), [2]=fieldB(imm),
  //                 [3]=fieldC(imm), [4]=f(imm), [5]=c(imm)
  //
  // RRI:  operand[1] is a 15/14/13-bit immediate split across fieldA+B+C
  // SHIFTI/BITOP: operand[1] is a 5-bit immediate → fieldA only
  // MOV (0x12): operand[1] is rs2 → fieldA, fieldB=max, fieldC=0 (handled below)

  bool IsFieldAOnly = (HWOpcode == 0x0D || HWOpcode == 0x0F || // SHLI, SHRI
                       HWOpcode == 0x11 ||                      // SARI
                       HWOpcode == 0x22 || HWOpcode == 0x23 ||  // BSET, BCLR
                       HWOpcode == 0x24);                       // BNOT

  for (unsigned i = 0; i < NumOps; ++i) {
    const MCOperand &MO = MI.getOperand(i);

    if (MO.isReg()) {
      unsigned Reg = MO.getReg();
      switch (i) {
      case 0: // rd
        if (Reg != 0)
          Binary |= ((Reg & RdMask) << RdShift);
        break;
      case 1: // rs2 / fieldA value
        Binary |= ((Reg & RsMask) << RsShift);
        break;
      }
    } else if (MO.isImm()) {
      int64_t Imm = MO.getImm();
      switch (i) {
      case 1: // RRI immediate or SHIFTI/BITOP immediate
        if (IsFieldAOnly) {
          Binary |= ((Imm & RsMask) << RsShift);
        } else {
          // Split immediate across fieldC, fieldB, fieldA
          Binary |= ((Imm & LsbMask) << LsbShift);
          Binary |= (((Imm >> LsbWidth) & MsbMask) << MsbShift);
          Binary |= (((Imm >> (LsbWidth + MsbWidth)) & RsMask) << RsShift);
        }
        break;
      case 2: // fieldB (msb)
        Binary |= ((Imm & MsbMask) << MsbShift);
        break;
      case 3: // fieldC (lsb)
        Binary |= ((Imm & LsbMask) << LsbShift);
        break;
      case 4: // Freg index
        Binary |= ((Imm & FMask) << FShift);
        break;
      case 5: // c_bit
        if (Imm)
          Binary |= (1 << CBitShift);
        break;
      }
    } else if (MO.isExpr()) {
      MCFixupKind Kind;
      switch (i) {
      case 0: case 1:
        Kind = MyCPU::fixup_MyCPU_16;
        break;
      default:
        Kind = MyCPU::fixup_MyCPU_16;
        break;
      }
      addFixup(i, Kind);
    }
  }

  // MOV register copy (0x12): fieldB = max bit index, fieldC = 0
  // These aren't explicit operands, so set them here
  if (HWOpcode == 0x12) {
    unsigned MaxBit = (SizeField == 2) ? 31 : (SizeField == 1) ? 15 : 7;
    Binary |= ((MaxBit & MsbMask) << MsbShift);
  }

  return Binary;
}

//===----------------------------------------------------------------------===//
// encodeInstruction — 输出小端字节序列
//
// ★ MyCPU 是小端架构，低位字节在前
//
// 示例：指令 0x12345678 的输出为 [78, 56, 34, 12]
//   Binary         = 00010010 00110100 01010110 01111000
//   Bits[7:0]   78
//   Bits[15:8]  56
//   Bits[23:16] 34
//   Bits[31:24] 12
//   输出: CB = [0x78, 0x56, 0x34, 0x12]
//===----------------------------------------------------------------------===//

void MyCPUMCCodeEmitter::encodeInstruction(const MCInst &MI,
                                             SmallVectorImpl<char> &CB,
                                             SmallVectorImpl<MCFixup> &Fixups,
                                             const MCSubtargetInfo &STI) const {
  uint32_t Bits = getBinaryCodeForInstr(MI, Fixups, STI);

  // Little-endian output: low byte first
  for (unsigned i = 0; i < 4; ++i)
    CB.push_back(static_cast<char>((Bits >> (i * 8)) & 0xFF));
}
