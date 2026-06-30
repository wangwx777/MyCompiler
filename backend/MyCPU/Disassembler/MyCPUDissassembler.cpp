//===-- MyCPUDissassembler.cpp - MyCPU Disassembler -----------------------===//

#include "MCTargetDesc/MyCPUBaseInfo.h"
#include "MCTargetDesc/MyCPUMCTargetDesc.h"
#include "TargetInfo/MyCPUTargetInfo.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::MCD;

#define DEBUG_TYPE "mycpu-disassembler"

#define GET_REGINFO_ENUM
#include "MyCPUGenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#include "MyCPUGenInstrInfo.inc"

using DecodeStatus = MCDisassembler::DecodeStatus;

// Register decode table: 0..31 → GPR register
static const MCRegister GPRDecodeTable[] = {
    MyCPU::ZERO, MyCPU::A0, MyCPU::A1, MyCPU::A2,
    MyCPU::A3,   MyCPU::A4, MyCPU::A5, MyCPU::A6,
    MyCPU::T0,   MyCPU::T1, MyCPU::T2, MyCPU::T3,
    MyCPU::T4,   MyCPU::T5, MyCPU::T6, MyCPU::T7,
    MyCPU::S0,   MyCPU::S1, MyCPU::S2, MyCPU::S3,
    MyCPU::S4,   MyCPU::S5, MyCPU::S6, MyCPU::S7,
    MyCPU::T8,   MyCPU::T9, MyCPU::T10, MyCPU::T11,
    MyCPU::LR,   MyCPU::SP, MyCPU::FP, MyCPU::GP,
};

static unsigned getSizeFromInsn(uint32_t Insn) {
  return (Insn >> 6) & 0x3; // bits[7:6]
}

static unsigned getOpcode(uint32_t Insn) {
  return Insn & 0x3F; // bits[5:0]
}

// Extract field from instruction for each format
static unsigned getFieldA(uint32_t Insn, unsigned Size) {
  switch (Size) {
  case 2: return (Insn >> 27) & 0x1F; // Word: 5b
  case 1: return (Insn >> 26) & 0x3F; // Half: 6b
  case 0: return (Insn >> 25) & 0x7F; // Byte: 7b
  default: return 0;
  }
}

static unsigned getFieldB(uint32_t Insn, unsigned Size) {
  switch (Size) {
  case 2: return (Insn >> 22) & 0x1F;
  case 1: return (Insn >> 22) & 0xF;
  case 0: return (Insn >> 22) & 0x7;
  default: return 0;
  }
}

static unsigned getFieldC(uint32_t Insn, unsigned Size) {
  switch (Size) {
  case 2: return (Insn >> 17) & 0x1F;
  case 1: return (Insn >> 18) & 0xF;
  case 0: return (Insn >> 19) & 0x7;
  default: return 0;
  }
}

static unsigned getRd(uint32_t Insn, unsigned Size) {
  switch (Size) {
  case 2: return (Insn >> 8)  & 0x1F;
  case 1: return (Insn >> 8)  & 0x3F;
  case 0: return (Insn >> 8)  & 0x7F;
  default: return 0;
  }
}

static unsigned getF(uint32_t Insn, unsigned Size) {
  switch (Size) {
  case 2: return (Insn >> 14) & 0x7;
  case 1: return (Insn >> 15) & 0x7;
  case 0: return (Insn >> 16) & 0x7;
  default: return 0;
  }
}

static unsigned getCBit(uint32_t Insn, unsigned Size) {
  switch (Size) {
  case 2: return (Insn >> 13) & 0x1;
  case 1: return (Insn >> 14) & 0x1;
  case 0: return (Insn >> 15) & 0x1;
  default: return 0;
  }
}

// Reconstruct 15-bit signed immediate from fieldA+B+C
static int16_t getImm15(uint32_t Insn, unsigned Size) {
  unsigned Fa = getFieldA(Insn, Size);
  unsigned Fb = getFieldB(Insn, Size);
  unsigned Fc = getFieldC(Insn, Size);
  unsigned FaW, FbW, FcW;
  switch (Size) {
  case 2: FaW = 5; FbW = 5; FcW = 5; break;
  case 1: FaW = 6; FbW = 4; FcW = 4; break;
  case 0: FaW = 7; FbW = 3; FcW = 3; break;
  default: return 0;
  }
  unsigned Imm = (Fa << (FbW + FcW)) | (Fb << FcW) | Fc;
  // Sign-extend from 15 bits
  return (int16_t)(Imm << 1) >> 1;
}

// Reconstruct 10-bit signed immediate from fieldB+C
static int16_t getOffset10(uint32_t Insn, unsigned Size) {
  unsigned Fb = getFieldB(Insn, Size);
  unsigned Fc = getFieldC(Insn, Size);
  unsigned FbW, FcW;
  switch (Size) {
  case 2: FbW = 5; FcW = 5; break;
  default: FbW = 4; FcW = 4; break;
  }
  unsigned Imm = (Fb << FcW) | Fc;
  // Sign-extend from 10 bits
  return (int16_t)(Imm << 6) >> 6;
}

namespace {

class MyCPUDissassembler : public MCDisassembler {
public:
  MyCPUDissassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx) {}

  DecodeStatus getInstruction(MCInst &Instr, uint64_t &Size,
                               ArrayRef<uint8_t> Bytes, uint64_t Address,
                               raw_ostream &CStream) const override {
    if (Bytes.size() < 4)
      return Fail;

    uint32_t Insn = support::endian::read32le(Bytes.data());
    unsigned Sz = getSizeFromInsn(Insn);
    unsigned Opc = getOpcode(Insn);
    unsigned Rd = getRd(Insn, Sz);
    unsigned Fa = getFieldA(Insn, Sz);
    unsigned Fb = getFieldB(Insn, Sz);
    unsigned Fc = getFieldC(Insn, Sz);
    unsigned FIdx = getF(Insn, Sz);

    // Word-size instructions
    if (Sz == 2) {
      switch (Opc) {
      case 0x00: // ADD.w
        Instr.setOpcode(MyCPU::ADDW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Size = 4; return Success;
      case 0x02: // SUB.w
        Instr.setOpcode(MyCPU::SUBW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Size = 4; return Success;
      case 0x04: // AND.w
        Instr.setOpcode(MyCPU::ANDW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Size = 4; return Success;
      case 0x06: // OR.w
        Instr.setOpcode(MyCPU::ORW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Size = 4; return Success;
      case 0x08: // XOR.w
        Instr.setOpcode(MyCPU::XORW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Size = 4; return Success;
      case 0x01: // ADDI.w
        Instr.setOpcode(MyCPU::ADDIW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(getImm15(Insn, Sz)));
        Size = 4; return Success;
      case 0x03: // SUBI.w
        Instr.setOpcode(MyCPU::SUBIW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(getImm15(Insn, Sz)));
        Size = 4; return Success;
      case 0x05: // ANDI.w
        Instr.setOpcode(MyCPU::ANDIW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(getImm15(Insn, Sz)));
        Size = 4; return Success;
      case 0x07: // ORI.w
        Instr.setOpcode(MyCPU::ORIW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(getImm15(Insn, Sz)));
        Size = 4; return Success;
      case 0x09: // XORI.w
        Instr.setOpcode(MyCPU::XORIW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(getImm15(Insn, Sz)));
        Size = 4; return Success;
      case 0x12: // MOV.w (寄存器传送)
        Instr.setOpcode(MyCPU::MOVABW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Size = 4; return Success;
      case 0x13: // MOVI.w
        Instr.setOpcode(MyCPU::MOVIW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(getImm15(Insn, Sz)));
        Size = 4; return Success;
      case 0x14: { // COP LD.w (协处理器读)
        unsigned Addr = (Fb << 5) | Fc; // 10-bit addr from fieldB+fieldC
        Instr.setOpcode(MyCPU::COPLDW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));    // cop_id
        Instr.addOperand(MCOperand::createImm(Addr));
        Size = 4; return Success;
      }
      case 0x15: { // COP ST.w (协处理器写)
        unsigned Addr = (Fb << 5) | Fc; // 10-bit addr from fieldB+fieldC
        Instr.setOpcode(MyCPU::COPSTW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd])); // src
        Instr.addOperand(MCOperand::createImm(Fa));    // cop_id
        Instr.addOperand(MCOperand::createImm(Addr));
        Size = 4; return Success;
      }
      case 0x28: { // MOV 访存 — c_bit 区分 LD/ST
        unsigned CBit = getCBit(Insn, Sz);
        if (CBit == 0) {
          // LD.w: rd = mem[base + offset]
          Instr.setOpcode(MyCPU::LDW);
          Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
          Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
          Instr.addOperand(MCOperand::createImm(getOffset10(Insn, Sz)));
        } else {
          // ST.w: mem[base + offset] = src
          Instr.setOpcode(MyCPU::STW);
          Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa])); // src
          Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd])); // base (rd field)
          Instr.addOperand(MCOperand::createImm((Fb << 5) | Fc));     // offset
        }
        Size = 4; return Success;
      }
      case 0x16: // CMP.w
        Instr.setOpcode(MyCPU::CMPW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Instr.addOperand(MCOperand::createImm(FIdx));
        Size = 4; return Success;
      case 0x17: // CMPI.w
        Instr.setOpcode(MyCPU::CMPIW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(getImm15(Insn, Sz)));
        Size = 4; return Success;
      case 0x18: // JMP
        Instr.setOpcode(MyCPU::JMP);
        Instr.addOperand(MCOperand::createImm(getImm15(Insn, Sz)));
        Size = 4; return Success;
      case 0x19: // JALR
        Instr.setOpcode(MyCPU::JALR);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Size = 4; return Success;
      case 0x1A: // BZ.w
        Instr.setOpcode(MyCPU::BZW);
        Instr.addOperand(MCOperand::createImm(FIdx));
        Instr.addOperand(MCOperand::createImm(getImm15(Insn, Sz)));
        Size = 4; return Success;
      case 0x1B: // BNZ (same as BZ format)
        Instr.setOpcode(MyCPU::BNZ);
        Instr.addOperand(MCOperand::createImm(FIdx));
        Instr.addOperand(MCOperand::createImm(getImm15(Insn, Sz)));
        Size = 4; return Success;
      case 0x1E: // CALL
        Instr.setOpcode(MyCPU::CALL);
        Instr.addOperand(MCOperand::createImm(getImm15(Insn, Sz)));
        Size = 4; return Success;
      case 0x1F: // RET
        Instr.setOpcode(MyCPU::RET);
        Size = 4; return Success;
      case 0x20: // (reserved — was PUSH)
      case 0x21: // (reserved — was POP)
        Size = 4; return Fail;
      case 0x22: // BSET
        Instr.setOpcode(MyCPU::BSET);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Size = 4; return Success;
      case 0x23: // BCLR
        Instr.setOpcode(MyCPU::BCLR);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Size = 4; return Success;
      case 0x24: // BNOT
        Instr.setOpcode(MyCPU::BNOT);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Size = 4; return Success;
      case 0x25: // BTST
        Instr.setOpcode(MyCPU::BTST);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Instr.addOperand(MCOperand::createImm(FIdx));
        Size = 4; return Success;
      case 0x26: // NOP
        Instr.setOpcode(MyCPU::NOP);
        Size = 4; return Success;
      case 0x27: // HALT
        Instr.setOpcode(MyCPU::HALT);
        Size = 4; return Success;
      case 0x0C: // SHL.w
        Instr.setOpcode(MyCPU::SHLW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Size = 4; return Success;
      case 0x0E: // SHR.w
        Instr.setOpcode(MyCPU::SHRW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Size = 4; return Success;
      case 0x10: // SAR.w
        Instr.setOpcode(MyCPU::SARW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Size = 4; return Success;
      case 0x0D: // SHLI.w
        Instr.setOpcode(MyCPU::SHLIW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Size = 4; return Success;
      case 0x0F: // SHRI.w
        Instr.setOpcode(MyCPU::SHRIW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Size = 4; return Success;
      case 0x11: // SARI.w
        Instr.setOpcode(MyCPU::SARIW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Size = 4; return Success;
      case 0x0A: // NOT.w
        Instr.setOpcode(MyCPU::NOTW);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Size = 4; return Success;
      }
    }

    // Half-size instructions
    if (Sz == 1) {
      switch (Opc) {
      case 0x00:
        Instr.setOpcode(MyCPU::ADDH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Size = 4; return Success;
      case 0x02:
        Instr.setOpcode(MyCPU::SUBH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Size = 4; return Success;
      case 0x04:
        Instr.setOpcode(MyCPU::ANDH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Size = 4; return Success;
      case 0x06:
        Instr.setOpcode(MyCPU::ORH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Size = 4; return Success;
      case 0x08:
        Instr.setOpcode(MyCPU::XORH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Size = 4; return Success;
      case 0x12: // MOVAB.h (寄存器传送)
        Instr.setOpcode(MyCPU::MOVABH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Size = 4; return Success;
      case 0x13: // MOVI.h (Half 立即数加载)
        Instr.setOpcode(MyCPU::MOVIH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(getImm15(Insn, Sz)));
        Size = 4; return Success;
      case 0x14: { // COP LD.h (协处理器读)
        unsigned Addr = (Fb << 4) | Fc; // 8-bit addr
        Instr.setOpcode(MyCPU::COPLDH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));    // cop_id
        Instr.addOperand(MCOperand::createImm(Addr));
        Size = 4; return Success;
      }
      case 0x15: { // COP ST.h (协处理器写)
        unsigned Addr = (Fb << 4) | Fc; // 8-bit addr
        Instr.setOpcode(MyCPU::COPSTH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd])); // src
        Instr.addOperand(MCOperand::createImm(Fa));    // cop_id
        Instr.addOperand(MCOperand::createImm(Addr));
        Size = 4; return Success;
      }
      case 0x28: { // MOV 访存.h — c_bit 区分 LD/ST
        unsigned CBit = getCBit(Insn, Sz);
        if (CBit == 0) {
          Instr.setOpcode(MyCPU::LDH);
          Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
          Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
          Instr.addOperand(MCOperand::createImm((Fb << 4) | Fc));
        } else {
          Instr.setOpcode(MyCPU::STH);
          Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
          Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
          Instr.addOperand(MCOperand::createImm((Fb << 4) | Fc));
        }
        Size = 4; return Success;
      }
      case 0x16:
        Instr.setOpcode(MyCPU::CMPH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Instr.addOperand(MCOperand::createImm(FIdx));
        Size = 4; return Success;
      case 0x0C:
        Instr.setOpcode(MyCPU::SHLH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Size = 4; return Success;
      case 0x0E:
        Instr.setOpcode(MyCPU::SHRH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Size = 4; return Success;
      case 0x10:
        Instr.setOpcode(MyCPU::SARH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Size = 4; return Success;
      case 0x0D:
        Instr.setOpcode(MyCPU::SHLIH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Size = 4; return Success;
      case 0x0F:
        Instr.setOpcode(MyCPU::SHRIH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Size = 4; return Success;
      case 0x11:
        Instr.setOpcode(MyCPU::SARIH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Size = 4; return Success;
      case 0x22:
        Instr.setOpcode(MyCPU::BSETH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Size = 4; return Success;
      case 0x23:
        Instr.setOpcode(MyCPU::BCLRH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Size = 4; return Success;
      case 0x24:
        Instr.setOpcode(MyCPU::BNOTH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Size = 4; return Success;
      case 0x25:
        Instr.setOpcode(MyCPU::BTSTH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Instr.addOperand(MCOperand::createImm(FIdx));
        Size = 4; return Success;
      case 0x0A:
        Instr.setOpcode(MyCPU::NOTH);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Size = 4; return Success;
      }
    }

    // Byte-size instructions
    if (Sz == 0) {
      switch (Opc) {
      case 0x00:
        Instr.setOpcode(MyCPU::ADDB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Size = 4; return Success;
      case 0x02:
        Instr.setOpcode(MyCPU::SUBB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Size = 4; return Success;
      case 0x04:
        Instr.setOpcode(MyCPU::ANDB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Size = 4; return Success;
      case 0x06:
        Instr.setOpcode(MyCPU::ORB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Size = 4; return Success;
      case 0x08:
        Instr.setOpcode(MyCPU::XORB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Size = 4; return Success;
      case 0x12: // MOVAB.b (寄存器传送)
        Instr.setOpcode(MyCPU::MOVABB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Size = 4; return Success;
      case 0x13: // MOVI.b (Byte 立即数加载)
        Instr.setOpcode(MyCPU::MOVIB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(getImm15(Insn, Sz)));
        Size = 4; return Success;
      case 0x14: { // COP LD.b (协处理器读)
        unsigned Addr = (Fb << 3) | Fc; // 6-bit addr
        Instr.setOpcode(MyCPU::COPLDB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));    // cop_id
        Instr.addOperand(MCOperand::createImm(Addr));
        Size = 4; return Success;
      }
      case 0x15: { // COP ST.b (协处理器写)
        unsigned Addr = (Fb << 3) | Fc; // 6-bit addr
        Instr.setOpcode(MyCPU::COPSTB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd])); // src
        Instr.addOperand(MCOperand::createImm(Fa));    // cop_id
        Instr.addOperand(MCOperand::createImm(Addr));
        Size = 4; return Success;
      }
      case 0x28: { // MOV 访存.b — c_bit 区分 LD/ST
        unsigned CBit = getCBit(Insn, Sz);
        if (CBit == 0) {
          Instr.setOpcode(MyCPU::LDB);
          Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
          Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
          Instr.addOperand(MCOperand::createImm((Fb << 3) | Fc));
        } else {
          Instr.setOpcode(MyCPU::STB);
          Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
          Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
          Instr.addOperand(MCOperand::createImm((Fb << 3) | Fc));
        }
        Size = 4; return Success;
      }
      case 0x16:
        Instr.setOpcode(MyCPU::CMPB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Instr.addOperand(MCOperand::createImm(FIdx));
        Size = 4; return Success;
      case 0x0C:
        Instr.setOpcode(MyCPU::SHLB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Size = 4; return Success;
      case 0x0E:
        Instr.setOpcode(MyCPU::SHRB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Size = 4; return Success;
      case 0x10:
        Instr.setOpcode(MyCPU::SARB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Size = 4; return Success;
      case 0x0D:
        Instr.setOpcode(MyCPU::SHLIB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Size = 4; return Success;
      case 0x0F:
        Instr.setOpcode(MyCPU::SHRIB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Size = 4; return Success;
      case 0x11:
        Instr.setOpcode(MyCPU::SARIB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Size = 4; return Success;
      case 0x22:
        Instr.setOpcode(MyCPU::BSETB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Size = 4; return Success;
      case 0x23:
        Instr.setOpcode(MyCPU::BCLRB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Size = 4; return Success;
      case 0x24:
        Instr.setOpcode(MyCPU::BNOTB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Size = 4; return Success;
      case 0x25:
        Instr.setOpcode(MyCPU::BTSTB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createImm(Fa));
        Instr.addOperand(MCOperand::createImm(FIdx));
        Size = 4; return Success;
      case 0x0A:
        Instr.setOpcode(MyCPU::NOTB);
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Rd]));
        Instr.addOperand(MCOperand::createReg(GPRDecodeTable[Fa]));
        Instr.addOperand(MCOperand::createImm(Fb));
        Instr.addOperand(MCOperand::createImm(Fc));
        Size = 4; return Success;
      }
    }

    return Fail;
  }
};

} // namespace

static MCDisassembler *
createMyCPUDissassembler(const Target &T, const MCSubtargetInfo &STI,
                          MCContext &Ctx) {
  return new MyCPUDissassembler(STI, Ctx);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeMyCPUDisassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheMyCPUTarget(),
                                          createMyCPUDissassembler);
}
