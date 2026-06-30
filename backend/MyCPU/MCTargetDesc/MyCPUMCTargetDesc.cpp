//===-- MyCPUMCTargetDesc.cpp - MyCPU Target Descriptions -----------------===//

#include "MyCPUMCTargetDesc.h"
#include "MyCPUBaseInfo.h"
#include "MyCPUInstPrinter.h"
#include "MyCPUMCAsmInfo.h"
#include "TargetInfo/MyCPUTargetInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "mycpu-mc"

#define GET_REGINFO_ENUM
#include "MyCPUGenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_DESC
#define GET_INSTRINFO_MC_HELPERS
#include "MyCPUGenInstrInfo.inc"

#define GET_REGINFO_MC_DESC
#include "MyCPUGenRegisterInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#define GET_SUBTARGETINFO_MC_DESC
#include "MyCPUGenSubtargetInfo.inc"

static MCInstrInfo *createMyCPUMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitMyCPUMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createMyCPUMCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitMyCPUMCRegisterInfo(X, MyCPU::LR);
  return X;
}

static MCSubtargetInfo *
createMyCPUMCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
  return createMyCPUMCSubtargetInfoImpl(TT, CPU, /*TuneCPU=*/CPU, FS);
}

static MCAsmInfo *createMyCPUMCAsmInfo(const MCRegisterInfo &MRI,
                                        const Triple &TT,
                                        const MCTargetOptions &Options) {
  MCAsmInfo *MAI = new MyCPUMCAsmInfo(Options, TT);
  return MAI;
}

static MCInstPrinter *createMyCPUMCInstPrinter(const Triple &TT,
                                                unsigned SyntaxVariant,
                                                const MCAsmInfo &MAI,
                                                const MCInstrInfo &MII,
                                                const MCRegisterInfo &MRI) {
  return new MyCPUInstPrinter(MAI, MII, MRI);
}

//===----------------------------------------------------------------------===//
// Target registration
//===----------------------------------------------------------------------===//

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeMyCPUTargetMC() {
  Target &T = getTheMyCPUTarget();

  TargetRegistry::RegisterMCAsmInfo(T, createMyCPUMCAsmInfo);
  TargetRegistry::RegisterMCInstrInfo(T, createMyCPUMCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(T, createMyCPUMCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createMyCPUMCSubtargetInfo);
  TargetRegistry::RegisterMCInstPrinter(T, createMyCPUMCInstPrinter);
  TargetRegistry::RegisterMCCodeEmitter(T, createMyCPUMCCodeEmitter);
  TargetRegistry::RegisterMCAsmBackend(T, createMyCPUAsmBackend);
}
