//===-- MyCPUInstPrinter.cpp - MyCPU MCInst Printer -----------------------===//

#include "MyCPUInstPrinter.h"
#include "MyCPUBaseInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "mycpu-asm-printer"

MyCPUInstPrinter::MyCPUInstPrinter(const MCAsmInfo &MAI,
                                     const MCInstrInfo &MII,
                                     const MCRegisterInfo &MRI)
    : MCInstPrinter(MAI, MII, MRI) {}

void MyCPUInstPrinter::printRegName(raw_ostream &O, MCRegister Reg) {
  O << getRegisterName(Reg);
}

void MyCPUInstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                    raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg()) {
    printRegName(O, Op.getReg());
    return;
  }
  if (Op.isImm()) {
    O << Op.getImm();
    return;
  }
  if (Op.isExpr()) {
    MAI.printExpr(O, *Op.getExpr());
    return;
  }
}

void MyCPUInstPrinter::printU5Imm(const MCInst *MI, unsigned OpNo,
                                  raw_ostream &O) {
  O << MI->getOperand(OpNo).getImm();
}

void MyCPUInstPrinter::printU4Imm(const MCInst *MI, unsigned OpNo,
                                  raw_ostream &O) {
  O << MI->getOperand(OpNo).getImm();
}

void MyCPUInstPrinter::printU3Imm(const MCInst *MI, unsigned OpNo,
                                  raw_ostream &O) {
  O << MI->getOperand(OpNo).getImm();
}

void MyCPUInstPrinter::printSImm15(const MCInst *MI, unsigned OpNo,
                                   raw_ostream &O) {
  O << MI->getOperand(OpNo).getImm();
}

void MyCPUInstPrinter::printSImm10(const MCInst *MI, unsigned OpNo,
                                   raw_ostream &O) {
  O << MI->getOperand(OpNo).getImm();
}

void MyCPUInstPrinter::printBranchTarget(const MCInst *MI, unsigned OpNo,
                                          raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm())
    O << formatHex(Op.getImm());
  else if (Op.isExpr())
    MAI.printExpr(O, *Op.getExpr());
}

void MyCPUInstPrinter::printCallTarget(const MCInst *MI, unsigned OpNo,
                                        raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm())
    O << formatHex(Op.getImm());
  else if (Op.isExpr())
    MAI.printExpr(O, *Op.getExpr());
}

void MyCPUInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                  StringRef Annot, const MCSubtargetInfo &STI,
                                  raw_ostream &O) {
  printAnnotation(O, Annot);
  printInstruction(MI, Address, O);
}

#define GET_INSTRINFO_ENUM
#include "MyCPUGenInstrInfo.inc"

#include "MyCPUGenAsmWriter.inc"
