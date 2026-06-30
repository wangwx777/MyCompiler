//===-- MyCPUAsmParser.cpp - MyCPU Assembly Parser ------------------------===//

#include "MCTargetDesc/MyCPUMCTargetDesc.h"
#include "TargetInfo/MyCPUTargetInfo.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/SMLoc.h"

using namespace llvm;

#define DEBUG_TYPE "mycpu-asm-parser"

#define GET_INSTRINFO_ENUM
#include "MyCPUGenInstrInfo.inc"

#define GET_REGINFO_ENUM
#include "MyCPUGenRegisterInfo.inc"

// Register decode table: encoding 0..31 → MCRegister
static const MCRegister RegisterDecodeTable[] = {
    MyCPU::ZERO, MyCPU::A0, MyCPU::A1, MyCPU::A2,
    MyCPU::A3,   MyCPU::A4, MyCPU::A5, MyCPU::A6,
    MyCPU::T0,   MyCPU::T1, MyCPU::T2, MyCPU::T3,
    MyCPU::T4,   MyCPU::T5, MyCPU::T6, MyCPU::T7,
    MyCPU::S0,   MyCPU::S1, MyCPU::S2, MyCPU::S3,
    MyCPU::S4,   MyCPU::S5, MyCPU::S6, MyCPU::S7,
    MyCPU::T8,   MyCPU::T9, MyCPU::T10, MyCPU::T11,
    MyCPU::LR,   MyCPU::SP, MyCPU::FP, MyCPU::GP,
};

namespace {

enum OperandKind {
  OK_Register = 0,
  OK_Immediate = 1,
  OK_Token = 2,
};

class MyCPUOperand : public MCParsedAsmOperand {
public:
  OperandKind Kind;
  SMLoc StartLoc, EndLoc;

  // Register value
  MCRegister RegNum;

  // Immediate value
  const MCExpr *ExprVal = nullptr;
  int64_t ImmVal = 0;

  // Token value
  StringRef TokVal;

  MyCPUOperand(OperandKind K, SMLoc S, SMLoc E)
      : Kind(K), StartLoc(S), EndLoc(E) {}

  bool isReg() const override { return Kind == OK_Register; }
  bool isImm() const override { return Kind == OK_Immediate; }
  bool isToken() const override { return Kind == OK_Token; }
  bool isMem() const override { return false; }

  SMLoc getStartLoc() const override { return StartLoc; }
  SMLoc getEndLoc() const override { return EndLoc; }

  MCRegister getReg() const override { return RegNum; }

  StringRef getToken() const {
    assert(Kind == OK_Token);
    return TokVal;
  }

  const MCExpr *getImmExpr() const {
    assert(Kind == OK_Immediate);
    return ExprVal;
  }

  int64_t getImm() const {
    assert(Kind == OK_Immediate && ExprVal == nullptr);
    return ImmVal;
  }

  void print(raw_ostream &OS, const MCAsmInfo &) const override {
    switch (Kind) {
    case OK_Register:  OS << "r" << RegNum; break;
    case OK_Immediate: OS << (ExprVal ? "expr" : std::to_string(ImmVal)); break;
    case OK_Token:     OS << TokVal; break;
    }
  }

  static std::unique_ptr<MyCPUOperand> createReg(MCRegister Reg, SMLoc S, SMLoc E) {
    auto Op = std::make_unique<MyCPUOperand>(OK_Register, S, E);
    Op->RegNum = Reg;
    return Op;
  }

  static std::unique_ptr<MyCPUOperand> createImm(int64_t Val, SMLoc S, SMLoc E) {
    auto Op = std::make_unique<MyCPUOperand>(OK_Immediate, S, E);
    Op->ImmVal = Val;
    return Op;
  }

  static std::unique_ptr<MyCPUOperand> createExpr(const MCExpr *Expr, SMLoc S, SMLoc E) {
    auto Op = std::make_unique<MyCPUOperand>(OK_Immediate, S, E);
    Op->ExprVal = Expr;
    return Op;
  }

  static std::unique_ptr<MyCPUOperand> createTok(StringRef Str, SMLoc S) {
    auto Op = std::make_unique<MyCPUOperand>(OK_Token, S, S);
    Op->TokVal = Str;
    return Op;
  }
};

class MyCPUAsmParser : public MCTargetAsmParser {
  MCAsmParser &Parser;

  // ★ Mnemonic → {Opcode, FormatClass}
  // FormatClass: 0=RRR, 1=RRI, 2=MOV, 3=LD, 4=ST, 5=BR, 6=BRF, 7=NOP,
  //              8=(reserved), 9=BSET, 10=BTST, 11=RET/HALT, 12=CMP
  //              13=NOT, 14=JALR, 15=SHIFT, 16=SHIFTI,
  //              17=COPLD, 18=COPST
  struct InstInfo {
    unsigned Opcode;
    unsigned FormatClass;
  };

  StringMap<InstInfo> MnemonicTable;

  void initMnemonicTable() {
    if (!MnemonicTable.empty()) return;
    // Word instructions
    MnemonicTable["add.w"]    = {MyCPU::ADDW,    0};
    MnemonicTable["sub.w"]    = {MyCPU::SUBW,    0};
    MnemonicTable["and.w"]    = {MyCPU::ANDW,    0};
    MnemonicTable["or.w"]     = {MyCPU::ORW,     0};
    MnemonicTable["xor.w"]    = {MyCPU::XORW,    0};
    MnemonicTable["addi.w"]   = {MyCPU::ADDIW,   1};
    MnemonicTable["subi.w"]   = {MyCPU::SUBIW,   1};
    MnemonicTable["andi.w"]   = {MyCPU::ANDIW,   1};
    MnemonicTable["ori.w"]    = {MyCPU::ORIW,    1};
    MnemonicTable["xori.w"]   = {MyCPU::XORIW,   1};
    MnemonicTable["movab.w"]    = {MyCPU::MOVABW,    2};
    MnemonicTable["ld.w"]     = {MyCPU::COPLDW, 17};
    MnemonicTable["st.w"]     = {MyCPU::COPSTW, 18};
    MnemonicTable["movi.w"]   = {MyCPU::MOVIW,   1};
    MnemonicTable["movi.h"]   = {MyCPU::MOVIH,   1};
    MnemonicTable["movi.b"]   = {MyCPU::MOVIB,   1};
    // "mov.w" handled in parseInstruction (peek for load vs store)
    MnemonicTable["mov.w"]    = {MyCPU::LDW,     3};
    MnemonicTable["jmp"]      = {MyCPU::JMP,     5};
    MnemonicTable["bz"]       = {MyCPU::BZW,     6};
    MnemonicTable["bnz"]      = {MyCPU::BNZ,     6};
    MnemonicTable["call"]     = {MyCPU::CALL,    5};
    MnemonicTable["ret"]      = {MyCPU::RET,    11};
    MnemonicTable["nop"]      = {MyCPU::NOP,    11};
    MnemonicTable["halt"]     = {MyCPU::HALT,   11};
    MnemonicTable["cmp.w"]    = {MyCPU::CMPW,   12};
    MnemonicTable["cmpi.w"]   = {MyCPU::CMPIW,   1};
    MnemonicTable["not.w"]    = {MyCPU::NOTW,   13};
    MnemonicTable["jalr"]     = {MyCPU::JALR,   14};
    MnemonicTable["shl.w"]    = {MyCPU::SHLW,   15};
    MnemonicTable["shr.w"]    = {MyCPU::SHRW,   15};
    MnemonicTable["sar.w"]    = {MyCPU::SARW,   15};
    MnemonicTable["shli.w"]   = {MyCPU::SHLIW,  16};
    MnemonicTable["shri.w"]   = {MyCPU::SHRIW,  16};
    MnemonicTable["sari.w"]   = {MyCPU::SARIW,  16};
    MnemonicTable["bset"]     = {MyCPU::BSET,    9};
    MnemonicTable["bclr"]     = {MyCPU::BCLR,    9};
    MnemonicTable["bnot"]     = {MyCPU::BNOT,    9};
    MnemonicTable["btst"]     = {MyCPU::BTST,   10};
    // Half shift/bitop
    MnemonicTable["shl.h"]    = {MyCPU::SHLH,   15};
    MnemonicTable["shr.h"]    = {MyCPU::SHRH,   15};
    MnemonicTable["sar.h"]    = {MyCPU::SARH,   15};
    MnemonicTable["shli.h"]   = {MyCPU::SHLIH,  16};
    MnemonicTable["shri.h"]   = {MyCPU::SHRIH,  16};
    MnemonicTable["sari.h"]   = {MyCPU::SARIH,  16};
    MnemonicTable["bset.h"]   = {MyCPU::BSETH,   9};
    MnemonicTable["bclr.h"]   = {MyCPU::BCLRH,   9};
    MnemonicTable["bnot.h"]   = {MyCPU::BNOTH,   9};
    MnemonicTable["btst.h"]   = {MyCPU::BTSTH,  10};
    // Byte shift/bitop
    MnemonicTable["shl.b"]    = {MyCPU::SHLB,   15};
    MnemonicTable["shr.b"]    = {MyCPU::SHRB,   15};
    MnemonicTable["sar.b"]    = {MyCPU::SARB,   15};
    MnemonicTable["shli.b"]   = {MyCPU::SHLIB,  16};
    MnemonicTable["shri.b"]   = {MyCPU::SHRIB,  16};
    MnemonicTable["sari.b"]   = {MyCPU::SARIB,  16};
    MnemonicTable["bset.b"]   = {MyCPU::BSETB,   9};
    MnemonicTable["bclr.b"]   = {MyCPU::BCLRB,   9};
    MnemonicTable["bnot.b"]   = {MyCPU::BNOTB,   9};
    MnemonicTable["btst.b"]   = {MyCPU::BTSTB,  10};
    // Half instructions
    MnemonicTable["add.h"]    = {MyCPU::ADDH,    0};
    MnemonicTable["sub.h"]    = {MyCPU::SUBH,    0};
    MnemonicTable["and.h"]    = {MyCPU::ANDH,    0};
    MnemonicTable["or.h"]     = {MyCPU::ORH,     0};
    MnemonicTable["xor.h"]    = {MyCPU::XORH,    0};
    MnemonicTable["movab.h"]    = {MyCPU::MOVABH,    2};
    MnemonicTable["ld.h"]     = {MyCPU::COPLDH, 17};
    MnemonicTable["st.h"]     = {MyCPU::COPSTH, 18};
    // "mov.h" handled in parseInstruction
    MnemonicTable["mov.h"]    = {MyCPU::LDH,     3};
    MnemonicTable["cmp.h"]    = {MyCPU::CMPH,   12};
    MnemonicTable["not.h"]    = {MyCPU::NOTH,   13};
    // Byte instructions
    MnemonicTable["add.b"]    = {MyCPU::ADDB,    0};
    MnemonicTable["sub.b"]    = {MyCPU::SUBB,    0};
    MnemonicTable["and.b"]    = {MyCPU::ANDB,    0};
    MnemonicTable["or.b"]     = {MyCPU::ORB,     0};
    MnemonicTable["xor.b"]    = {MyCPU::XORB,    0};
    MnemonicTable["movab.b"]    = {MyCPU::MOVABB,    2};
    MnemonicTable["ld.b"]     = {MyCPU::COPLDB, 17};
    MnemonicTable["st.b"]     = {MyCPU::COPSTB, 18};
    // "mov.b" handled in parseInstruction
    MnemonicTable["mov.b"]    = {MyCPU::LDB,     3};
    MnemonicTable["cmp.b"]    = {MyCPU::CMPB,   12};
    MnemonicTable["not.b"]    = {MyCPU::NOTB,   13};
  }

public:
  MyCPUAsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                 const MCInstrInfo &MII)
      : MCTargetAsmParser(STI, MII), Parser(Parser) {
    initMnemonicTable();
  }

  bool parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                     SMLoc &EndLoc) override {
    const AsmToken &Tok = getLexer().getTok();
    if (Tok.isNot(AsmToken::Identifier))
      return true;

    StringRef Name = Tok.getString();
    if (!Name.starts_with("r") && !Name.starts_with("R"))
      return true;

    unsigned RegNo;
    if (Name.drop_front().getAsInteger(10, RegNo) || RegNo > 31)
      return true;

    StartLoc = Tok.getLoc();
    EndLoc = Tok.getEndLoc();
    Reg = RegisterDecodeTable[RegNo];
    getLexer().Lex();
    return false;
  }

  ParseStatus tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                               SMLoc &EndLoc) override {
    AsmToken Tok = getLexer().getTok();
    if (Tok.isNot(AsmToken::Identifier))
      return ParseStatus();

    StringRef Name = Tok.getString();
    if (!Name.starts_with("r") && !Name.starts_with("R"))
      return ParseStatus();

    unsigned RegNo;
    if (Name.drop_front().getAsInteger(10, RegNo) || RegNo > 31)
      return ParseStatus();

    Reg = RegisterDecodeTable[RegNo];
    return ParseStatus(false);
  }

  // ★ Parse a register optionally followed by [msb:lsb] bitfield
  // Returns true on error
  bool parseRegOrBitfield(OperandVector &Operands) {
    MCRegister Reg;
    SMLoc Start, End;
    if (parseRegister(Reg, Start, End))
      return true;
    Operands.push_back(MyCPUOperand::createReg(Reg, Start, End));

    // Check for [msb:lsb] bitfield
    if (getLexer().is(AsmToken::LBrac)) {
      SMLoc BracLoc = getLexer().getLoc();
      getLexer().Lex(); // eat [

      // Parse msb
      const MCExpr *MsbExpr;
      SMLoc MsbLoc = getLexer().getLoc();
      if (Parser.parseExpression(MsbExpr))
        return true;
      Operands.push_back(MyCPUOperand::createExpr(MsbExpr, MsbLoc,
                          getLexer().getLoc()));

      // Expect ':'
      if (getLexer().isNot(AsmToken::Colon))
        return Error(getLexer().getLoc(), "expected ':' in bitfield");
      getLexer().Lex(); // eat :

      // Parse lsb
      const MCExpr *LsbExpr;
      SMLoc LsbLoc = getLexer().getLoc();
      if (Parser.parseExpression(LsbExpr))
        return true;
      Operands.push_back(MyCPUOperand::createExpr(LsbExpr, LsbLoc,
                          getLexer().getLoc()));

      // Expect ']'
      if (getLexer().isNot(AsmToken::RBrac))
        return Error(getLexer().getLoc(), "expected ']'");
      getLexer().Lex(); // eat ]
    }
    return false;
  }

  // ★ Parse memory operand: [base + offset]
  bool parseMemOperand(OperandVector &Operands) {
    SMLoc Start = getLexer().getLoc();
    if (getLexer().isNot(AsmToken::LBrac))
      return Error(Start, "expected '['");
    getLexer().Lex(); // eat '['

    // Parse base register
    MCRegister Base;
    SMLoc RegStart, RegEnd;
    if (parseRegister(Base, RegStart, RegEnd))
      return Error(getLexer().getLoc(), "expected register in memory operand");
    Operands.push_back(MyCPUOperand::createReg(Base, RegStart, RegEnd));

    // Parse + offset
    if (getLexer().is(AsmToken::Plus)) {
      getLexer().Lex(); // eat '+'
    }

    const MCExpr *OffsetExpr;
    if (Parser.parseExpression(OffsetExpr))
      return true;
    Operands.push_back(MyCPUOperand::createExpr(OffsetExpr, Start,
                        getLexer().getLoc()));

    if (getLexer().isNot(AsmToken::RBrac))
      return Error(getLexer().getLoc(), "expected ']'");
    getLexer().Lex(); // eat ']'
    return false;
  }

  bool parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override {
    // ★ MOV memory: same syntax for load and store
    //   mov.w $rd, [$base + $off] → load (LDW)
    //   mov.w $src, [$base + $off] → store (STW)
    unsigned Opc;
    unsigned Fmt;
    if (Name == "mov.w" || Name == "mov.h" || Name == "mov.b") {
      Fmt = 3; // MOV mem: same parsing as load
      if (Name == "mov.w")      Opc = MyCPU::LDW;
      else if (Name == "mov.h") Opc = MyCPU::LDH;
      else                      Opc = MyCPU::LDB;
    } else {
      auto It = MnemonicTable.find(Name);
      if (It == MnemonicTable.end())
        return Error(NameLoc, "unknown instruction: " + Name);
      Opc = It->second.Opcode;
      Fmt = It->second.FormatClass;
    }

    // Create mnemonic token
    Operands.push_back(MyCPUOperand::createTok(Name, NameLoc));

    // Parse operands based on format class
    switch (Fmt) {
    case 19: // MOV store: [$base + $off], $src
      if (parseMemOperand(Operands)) return true;             // [base + offset]
      if (!parseOptionalComma()) break;
      if (parseRegOrBitfield(Operands)) return true;          // src
      break;

    case 0: // RRR: rd, rs2[msb:lsb]  (rd_in is tied, not in asm)
      if (parseRegOrBitfield(Operands)) return true;          // rd
      if (!parseOptionalComma()) break;
      if (parseRegOrBitfield(Operands)) return true;          // rs2 [msb lsb]
      break;

    case 1: // RRI: rd, rd, imm  or  rd, imm
      if (parseRegOrBitfield(Operands)) return true;          // rd
      if (!parseOptionalComma()) break;
      // For three-address RRI (addi.w rd, rd, imm), second reg is just a reg
      if (getLexer().is(AsmToken::Identifier)) {
        MCRegister R;
        SMLoc S, E;
        if (!parseRegister(R, S, E)) {
          Operands.push_back(MyCPUOperand::createReg(R, S, E));
          if (!parseOptionalComma()) break;
        }
      }
      {
        const MCExpr *ImmExpr;
        SMLoc ImmLoc = getLexer().getLoc();
        if (Parser.parseExpression(ImmExpr)) return true;
        Operands.push_back(MyCPUOperand::createExpr(ImmExpr, ImmLoc,
                            getLexer().getLoc()));
      }
      break;

    case 2: // MOV: rd, rs2
      if (parseRegOrBitfield(Operands)) return true;          // rd
      if (!parseOptionalComma()) break;
      if (parseRegOrBitfield(Operands)) return true;          // rs2
      break;

    case 3: // LD: rd, [base + offset]
      if (parseRegOrBitfield(Operands)) return true;          // rd
      if (!parseOptionalComma()) break;
      if (parseMemOperand(Operands)) return true;             // [base + offset]
      break;

    case 4: // ST: src, [base + offset]
      if (parseRegOrBitfield(Operands)) return true;          // src
      if (!parseOptionalComma()) break;
      if (parseMemOperand(Operands)) return true;             // [base + offset]
      break;

    case 5: // BR: target  (JMP, CALL)
      {
        const MCExpr *TargetExpr;
        SMLoc TargetLoc = getLexer().getLoc();
        if (Parser.parseExpression(TargetExpr)) return true;
        Operands.push_back(MyCPUOperand::createExpr(TargetExpr, TargetLoc,
                            getLexer().getLoc()));
      }
      break;

    case 6: // BRF: f, target  (BZ, BNZ)
      {
        const MCExpr *FExpr;
        if (Parser.parseExpression(FExpr)) return true;
        Operands.push_back(MyCPUOperand::createExpr(FExpr, getLexer().getLoc(),
                            getLexer().getLoc()));
        if (!parseOptionalComma()) break;
        const MCExpr *TargetExpr;
        SMLoc TargetLoc = getLexer().getLoc();
        if (Parser.parseExpression(TargetExpr)) return true;
        Operands.push_back(MyCPUOperand::createExpr(TargetExpr, TargetLoc,
                            getLexer().getLoc()));
      }
      break;

    case 8: // (reserved — was PUSH/POP)
      break;

    case 9: // BSET/BCLR/BNOT: rd, bitpos
      if (parseRegOrBitfield(Operands)) return true;          // rd
      if (!parseOptionalComma()) break;
      {
        const MCExpr *BitExpr;
        SMLoc BitLoc = getLexer().getLoc();
        if (Parser.parseExpression(BitExpr)) return true;
        Operands.push_back(MyCPUOperand::createExpr(BitExpr, BitLoc,
                            getLexer().getLoc()));
      }
      break;

    case 10: // BTST: rd, bitpos, f
      if (parseRegOrBitfield(Operands)) return true;          // rd
      if (!parseOptionalComma()) break;
      {
        const MCExpr *BitExpr;
        SMLoc BitLoc = getLexer().getLoc();
        if (Parser.parseExpression(BitExpr)) return true;
        Operands.push_back(MyCPUOperand::createExpr(BitExpr, BitLoc,
                            getLexer().getLoc()));
      }
      if (!parseOptionalComma()) break;
      {
        const MCExpr *FExpr;
        if (Parser.parseExpression(FExpr)) return true;
        Operands.push_back(MyCPUOperand::createExpr(FExpr, getLexer().getLoc(),
                            getLexer().getLoc()));
      }
      break;

    case 11: // RET/NOP/HALT: no operands
      break;

    case 12: // CMP: rd, rs2[msb:lsb], f
      if (parseRegOrBitfield(Operands)) return true;          // rd
      if (!parseOptionalComma()) break;
      if (parseRegOrBitfield(Operands)) return true;          // rs2 [msb lsb]
      if (!parseOptionalComma()) break;
      {
        const MCExpr *FExpr;
        if (Parser.parseExpression(FExpr)) return true;
        Operands.push_back(MyCPUOperand::createExpr(FExpr, getLexer().getLoc(),
                            getLexer().getLoc()));
      }
      break;

    case 13: // NOT: rd, rs2[msb:lsb]
      if (parseRegOrBitfield(Operands)) return true;          // rd
      if (!parseOptionalComma()) break;
      if (parseRegOrBitfield(Operands)) return true;          // rs2 [msb lsb]
      break;

    case 14: // JALR: rd, rs2
      if (parseRegOrBitfield(Operands)) return true;          // rd
      if (!parseOptionalComma()) break;
      if (parseRegOrBitfield(Operands)) return true;          // rs2
      break;

    case 15: // SHIFT: rd, rd, rs2  (shl.w rd, rd, rs2)
      if (parseRegOrBitfield(Operands)) return true;          // rd
      if (!parseOptionalComma()) break;
      if (parseRegOrBitfield(Operands)) return true;          // rd_in (same as rd in AsmPrinter)
      if (!parseOptionalComma()) break;
      if (parseRegOrBitfield(Operands)) return true;          // rs2
      break;

    case 16: // SHIFTI: rd, rd, shamt
      if (parseRegOrBitfield(Operands)) return true;          // rd
      if (!parseOptionalComma()) break;
      if (parseRegOrBitfield(Operands)) return true;          // rd_in
      if (!parseOptionalComma()) break;
      {
        const MCExpr *ShamtExpr;
        if (Parser.parseExpression(ShamtExpr)) return true;
        Operands.push_back(MyCPUOperand::createExpr(ShamtExpr, getLexer().getLoc(),
                            getLexer().getLoc()));
      }
      break;

    case 17: // COPLD: rd, cop_id, addr
      if (parseRegOrBitfield(Operands)) return true;          // rd
      if (!parseOptionalComma()) break;
      {
        const MCExpr *CopID;
        if (Parser.parseExpression(CopID)) return true;
        Operands.push_back(MyCPUOperand::createExpr(CopID, getLexer().getLoc(),
                            getLexer().getLoc()));
      }
      if (!parseOptionalComma()) break;
      {
        const MCExpr *Addr;
        if (Parser.parseExpression(Addr)) return true;
        Operands.push_back(MyCPUOperand::createExpr(Addr, getLexer().getLoc(),
                            getLexer().getLoc()));
      }
      break;

    case 18: // COPST: src, cop_id, addr
      if (parseRegOrBitfield(Operands)) return true;          // src
      if (!parseOptionalComma()) break;
      {
        const MCExpr *CopID;
        if (Parser.parseExpression(CopID)) return true;
        Operands.push_back(MyCPUOperand::createExpr(CopID, getLexer().getLoc(),
                            getLexer().getLoc()));
      }
      if (!parseOptionalComma()) break;
      {
        const MCExpr *Addr;
        if (Parser.parseExpression(Addr)) return true;
        Operands.push_back(MyCPUOperand::createExpr(Addr, getLexer().getLoc(),
                            getLexer().getLoc()));
      }
      break;
    }

    // Check for unexpected trailing tokens
    if (getLexer().isNot(AsmToken::EndOfStatement))
      return Error(getLexer().getLoc(), "unexpected token");

    getLexer().Lex(); // eat end-of-statement
    return false;
  }

  bool matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override {
    MCInst Inst;

    // First operand is always the mnemonic token
    StringRef Mnemonic = ((MyCPUOperand *)Operands[0].get())->getToken();
    auto It = MnemonicTable.find(Mnemonic);
    if (It == MnemonicTable.end())
      return Error(IDLoc, "unknown mnemonic");

    Inst.setOpcode(It->second.Opcode);
    unsigned Fmt = It->second.FormatClass;

    // Build MCInst based on format class
    // Operand convention (matching what MCCodeEmitter expects):
    //   idx 0 = mnemonic token (skip)
    //   idx 1+ = actual operands
    switch (Fmt) {
    case 0: // RRR: rd, rs2[msb:lsb]
      // Parsed: token, rd_reg, rs2_reg, msb_imm, lsb_imm
      addReg(Inst, Operands, 1);  // rd
      addReg(Inst, Operands, 2);  // rs2
      addImmWithDefault(Inst, Operands, 3, 31); // msb (default 31 for Word)
      addImmWithDefault(Inst, Operands, 4, 0);  // lsb
      break;

    case 1: // RRI: rd, [rd_in,] imm  (rd_in is tied, not in MCInst)
      addReg(Inst, Operands, 1);  // rd
      // rd_in at operand[2] is skipped (tied constraint)
      addImmFromLast(Inst, Operands); // imm
      break;

    case 2: // MOV: rd, rs2
      addReg(Inst, Operands, 1);  // rd
      addReg(Inst, Operands, 2);  // rs2
      break;

    case 3: // LD: rd, base, offset
      addReg(Inst, Operands, 1);  // rd
      addReg(Inst, Operands, 2);  // base
      addImmFromLast(Inst, Operands); // offset
      break;

    case 4: // COP ST: src, cop_id, addr  (or MOV store: [base+off], src)
      // Check if first operand is memory (format 19 redirected here)
      addReg(Inst, Operands, 1);  // src
      addReg(Inst, Operands, 2);  // base
      addImmFromLast(Inst, Operands); // offset
      break;

    case 19: // MOV store: [base + off], src
      // Parsed as: token, base_reg, offset_imm/expt, src_reg
      addReg(Inst, Operands, 3);  // src (last operand)
      addReg(Inst, Operands, 1);  // base (from mem operand)
      addImm(Inst, Operands, 2);  // offset
      break;

    case 5: // BR: target (JMP, CALL)
      addExpr(Inst, Operands, 1);
      break;

    case 6: // BRF: f, target (BZ, BNZ)
      addImm(Inst, Operands, 1);
      addExpr(Inst, Operands, 2);
      break;

    case 8: // (reserved — was PUSH/POP)
      break;

    case 9: // BSET/BCLR/BNOT: rd, bitpos
      addReg(Inst, Operands, 1);
      addImm(Inst, Operands, 2);
      break;

    case 10: // BTST: rd, bitpos, f
      addReg(Inst, Operands, 1);
      addImm(Inst, Operands, 2);
      addImm(Inst, Operands, 3);
      break;

    case 11: // RET/NOP/HALT: no operands
      break;

    case 12: // CMP: rd, rs2[msb:lsb], f
      addReg(Inst, Operands, 1);   // rd
      addReg(Inst, Operands, 2);   // rs2
      addImmWithDefault(Inst, Operands, 3, 31); // msb
      addImmWithDefault(Inst, Operands, 4, 0);  // lsb
      addImmFromLast(Inst, Operands); // f
      break;

    case 13: // NOT: rd, rs2[msb:lsb]
      addReg(Inst, Operands, 1);   // rd
      addReg(Inst, Operands, 2);   // rs2
      addImmWithDefault(Inst, Operands, 3, 31); // msb
      addImmWithDefault(Inst, Operands, 4, 0);  // lsb
      break;

    case 14: // JALR: rd, rs2
      addReg(Inst, Operands, 1);
      addReg(Inst, Operands, 2);
      break;

    case 15: // SHIFT: rd, rd_in, rs2  (rd_in not in MCInst due to tied constraint)
      addReg(Inst, Operands, 1);  // rd
      addReg(Inst, Operands, 3);  // rs2 (skip rd_in at idx 2)
      break;

    case 16: // SHIFTI: rd, rd_in, shamt
      addReg(Inst, Operands, 1);  // rd
      addImm(Inst, Operands, 3);  // shamt (skip rd_in at idx 2)
      break;

    case 17: // COPLD: rd, cop_id, addr → MCInst[0]=rd, [1]=cop_id, [2]=addr
      addReg(Inst, Operands, 1);  // rd
      addImm(Inst, Operands, 2);  // cop_id
      addImm(Inst, Operands, 3);  // addr
      break;

    case 18: // COPST: src, cop_id, addr → MCInst[0]=src, [1]=cop_id, [2]=addr
      addReg(Inst, Operands, 1);  // src
      addImm(Inst, Operands, 2);  // cop_id
      addImm(Inst, Operands, 3);  // addr
      break;
    }

    Out.emitInstruction(Inst, getSTI());
    return false;
  }

  void convertToMapAndConstraints(unsigned Kind,
                                  const OperandVector &Operands) override {}

  unsigned validateTargetOperandClass(MCParsedAsmOperand &Op,
                                      unsigned Kind) override {
    return MCTargetAsmParser::validateTargetOperandClass(Op, Kind);
  }

private:
  bool parseOptionalComma() {
    if (getLexer().is(AsmToken::Comma)) {
      getLexer().Lex();
      return true;
    }
    return false;
  }

  static bool isReg(const MCParsedAsmOperand *Op) {
    return ((MyCPUOperand *)Op)->isReg();
  }

  void addReg(MCInst &Inst, OperandVector &Ops, unsigned Idx) {
    if (Ops.size() <= Idx) return;
    auto *Op = (MyCPUOperand *)Ops[Idx].get();
    if (Op->isReg())
      Inst.addOperand(MCOperand::createReg(Op->getReg()));
  }

  void addImm(MCInst &Inst, OperandVector &Ops, unsigned Idx) {
    if (Ops.size() <= Idx) return;
    auto *Op = (MyCPUOperand *)Ops[Idx].get();
    if (!Op->isImm()) return;
    if (Op->ExprVal)
      Inst.addOperand(MCOperand::createExpr(Op->ExprVal));
    else
      Inst.addOperand(MCOperand::createImm(Op->ImmVal));
  }

  void addExpr(MCInst &Inst, OperandVector &Ops, unsigned Idx) {
    if (Ops.size() <= Idx) return;
    auto *Op = (MyCPUOperand *)Ops[Idx].get();
    if (!Op->isImm()) return;
    if (Op->ExprVal)
      Inst.addOperand(MCOperand::createExpr(Op->ExprVal));
    else
      Inst.addOperand(MCOperand::createImm(Op->ImmVal));
  }

  void addImmFromLast(MCInst &Inst, OperandVector &Ops) {
    addImm(Inst, Ops, Ops.size() - 1);
  }

  void addImmWithDefault(MCInst &Inst, OperandVector &Ops, unsigned Idx,
                         int64_t Default) {
    if (Ops.size() <= Idx) {
      Inst.addOperand(MCOperand::createImm(Default));
      return;
    }
    addImm(Inst, Ops, Idx);
  }
};

} // namespace

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeMyCPUAsmParser() {
  RegisterMCAsmParser<MyCPUAsmParser> X(getTheMyCPUTarget());
}
