//===-- MyCPUMCTargetDesc.h - MyCPU Target Descriptions ---------*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_MYCPU_MCTARGETDESC_MYCPUMCTARGETDESC_H
#define LLVM_LIB_TARGET_MYCPU_MCTARGETDESC_MYCPUMCTARGETDESC_H

#include "llvm/Support/DataTypes.h"
#include <memory>

namespace llvm {

class MCAsmBackend;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCRegisterInfo;
class MCSubtargetInfo;
class MCTargetOptions;
class StringRef;
class Target;
class Triple;

Target &getTheMyCPUTarget();

MCCodeEmitter *createMyCPUMCCodeEmitter(const MCInstrInfo &MCII,
                                         MCContext &Ctx);

MCAsmBackend *createMyCPUAsmBackend(const Target &T,
                                     const MCSubtargetInfo &STI,
                                     const MCRegisterInfo &MRI,
                                     const MCTargetOptions &Options);

std::unique_ptr<MCObjectTargetWriter>
createMyCPUELFObjectWriter(uint8_t OSABI);

} // namespace llvm

#endif
