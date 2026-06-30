//===-- MyCPUAsmPrinter.cpp - MyCPU Assembly Printer ----------------------===//
//
// ★ 汇编输出器 — 将 MachineInstr 输出为汇编文本(.s 文件) ★
//
// 工作流程：
//   1. MachineInstr → MCInstLowering → MCInst (中间表示)
//   2. MCInst → emitInstruction → MCStreamer → 汇编文本
//
// ★ 伪指令展开 (lowerPseudoInstExpansion):
//   LOAD_IMM  → MOVIW (常量在 15 位范围内时)
//   LOAD_ADDR → MOVIW (地址加载)
//   超出范围的常量需要多指令序列，由 MyCPUExpandPseudo pass 处理
//
// ★ 添加新的伪指令展开：
//   1. 在 lowerPseudoInstExpansion() 的 switch 中添加 case
//   2. 构造 MCInst 操作数(注意操作数顺序与 .td 定义一致)
//   3. 返回 true 表示已展开，false 表示交给标准 lowering
//
//===----------------------------------------------------------------------===//

#include "MyCPU.h"
#include "MyCPUTargetMachine.h"
#include "MCTargetDesc/MyCPUInstPrinter.h"
#include "MCTargetDesc/MyCPUMCTargetDesc.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "mycpu-asm-printer"

#define GET_INSTRINFO_ENUM
#include "MyCPUGenInstrInfo.inc"

namespace {

class MyCPUAsmPrinter : public AsmPrinter {
public:
  explicit MyCPUAsmPrinter(TargetMachine &TM,
                           std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "MyCPU Assembly Printer"; }

  // ★ 输出单条指令(ASM 文本或 .o 对象文件)
  void emitInstruction(const MachineInstr *MI) override;

  void emitFunctionBodyStart() override;
  void emitFunctionBodyEnd() override;

  bool runOnMachineFunction(MachineFunction &MF) override;

  // ★ 伪指令展开：将伪 MI 转换为真实 MCInst
  bool lowerPseudoInstExpansion(const MachineInstr *MI, MCInst &Inst);

private:
  MCOperand lowerOperand(const MachineOperand &MO) const;
  void lowerToMCInst(const MachineInstr *MI, MCInst &OutMI);
};

} // namespace

//===----------------------------------------------------------------------===//
// runOnMachineFunction — 函数级入口
//===----------------------------------------------------------------------===//

bool MyCPUAsmPrinter::runOnMachineFunction(MachineFunction &MF) {
  SetupMachineFunction(MF);
  emitFunctionBody();
  return false;
}

void MyCPUAsmPrinter::emitFunctionBodyStart() {
  // emitFunctionHeader() already emitted CurrentFnSym via emitFunctionEntryLabel()
}

void MyCPUAsmPrinter::emitFunctionBodyEnd() {}

//===----------------------------------------------------------------------===//
// lowerPseudoInstExpansion — 伪指令展开
//
// ★ MachineInstr → MCInst 的预展开步骤
//   在标准 MCInstLowering 之前尝试展开伪指令
//
// 操作数顺序注意事项：
//   LOAD_IMM rd, imm  →  MOVIW rd, imm
//   操作数[0] = rd (寄存器 — 输出)
//   操作数[1] = imm (立即数)
//
//   LOAD_ADDR rd, addr → MOVIW rd, addr
//   操作数[0] = rd (寄存器 — 输出)
//   操作数[1] = addr (立即数)
//
// ★ 如果需要支持 32 位完整立即数(超出 15 位范围)：
//   在此 emit 多条指令序列
//===----------------------------------------------------------------------===//

bool MyCPUAsmPrinter::lowerPseudoInstExpansion(const MachineInstr *MI,
                                                MCInst &Inst) {
  unsigned Opcode = MI->getOpcode();

  switch (Opcode) {

  //===--- LOAD_IMM → MOVIW (常量在 15 位范围内) ---===//
  // ★ LOAD_IMM rd, imm → 如果 imm ∈ [-16384, 16383] → MOVIW rd, imm
  // 否则需要多指令序列: MOVIW + SHLIW + ORIW
  case MyCPU::LOAD_IMM: {
    int64_t Imm = MI->getOperand(1).getImm();
    if (isInt<15>(Imm)) {
      Inst.setOpcode(MyCPU::MOVIW);
      Inst.addOperand(MCOperand::createReg(MI->getOperand(0).getReg()));
      Inst.addOperand(MCOperand::createImm(Imm));
      return true;
    }
    // ★ 大立即数：留待 MyCPUExpandPseudo pass 展开为多条指令
    return false;
  }

  //===--- LOAD_ADDR → MOVIW (地址放立即数) ---===//
  // ★ 地址标签在链接时由 fixup 修正为实际地址
  case MyCPU::LOAD_ADDR: {
    int64_t Addr = MI->getOperand(1).getImm();
    Inst.setOpcode(MyCPU::MOVIW);
    Inst.addOperand(MCOperand::createReg(MI->getOperand(0).getReg()));
    Inst.addOperand(MCOperand::createImm(Addr));
    return true;
  }

  default:
    return false;
  }
}

//===----------------------------------------------------------------------===//
// emitInstruction — 输出单条指令
//
// ★ 先尝试展开伪指令，如果失败则使用标准 MCInstLowering
//===----------------------------------------------------------------------===//

MCOperand MyCPUAsmPrinter::lowerOperand(const MachineOperand &MO) const {
  switch (MO.getType()) {
  default:
    break;
  case MachineOperand::MO_Register:
    if (MO.isImplicit())
      break;
    return MCOperand::createReg(MO.getReg());
  case MachineOperand::MO_Immediate:
    return MCOperand::createImm(MO.getImm());
  case MachineOperand::MO_MachineBasicBlock:
    return MCOperand::createExpr(
        MCSymbolRefExpr::create(MO.getMBB()->getSymbol(), OutContext));
  case MachineOperand::MO_GlobalAddress:
    return MCOperand::createExpr(
        MCSymbolRefExpr::create(getSymbol(MO.getGlobal()), OutContext));
  case MachineOperand::MO_BlockAddress:
    return MCOperand::createExpr(
        MCSymbolRefExpr::create(GetBlockAddressSymbol(MO.getBlockAddress()),
                                OutContext));
  case MachineOperand::MO_ExternalSymbol:
    return MCOperand::createExpr(
        MCSymbolRefExpr::create(GetExternalSymbolSymbol(MO.getSymbolName()),
                                OutContext));
  case MachineOperand::MO_ConstantPoolIndex:
    return MCOperand::createExpr(
        MCSymbolRefExpr::create(GetCPISymbol(MO.getIndex()), OutContext));
  case MachineOperand::MO_RegisterMask:
    break;
  }
  return MCOperand();
}

void MyCPUAsmPrinter::lowerToMCInst(const MachineInstr *MI, MCInst &OutMI) {
  OutMI.setOpcode(MI->getOpcode());
  for (const MachineOperand &MO : MI->operands()) {
    // Skip tied USE operands — they duplicate the tied DEF operand
    if (MO.isReg() && MO.isTied() && !MO.isDef())
      continue;
    MCOperand MCOp = lowerOperand(MO);
    if (MCOp.isValid())
      OutMI.addOperand(MCOp);
  }
}

void MyCPUAsmPrinter::emitInstruction(const MachineInstr *MI) {
  MCInst TmpInst;

  if (lowerPseudoInstExpansion(MI, TmpInst)) {
    EmitToStreamer(*OutStreamer, TmpInst);
    return;
  }

  lowerToMCInst(MI, TmpInst);
  EmitToStreamer(*OutStreamer, TmpInst);
}

//===----------------------------------------------------------------------===//
// 汇编器注册入口
//===----------------------------------------------------------------------===//

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeMyCPUAsmPrinter() {
  RegisterAsmPrinter<MyCPUAsmPrinter> X(getTheMyCPUTarget());
}
