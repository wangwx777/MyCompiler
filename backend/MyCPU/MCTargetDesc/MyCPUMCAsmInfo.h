//===-- MyCPUMCAsmInfo.h - MyCPU MC AsmInfo ----------------------*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_MYCPU_MCTARGETDESC_MYCPUMCASMINFO_H
#define LLVM_LIB_TARGET_MYCPU_MCTARGETDESC_MYCPUMCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {

class Triple;

class MyCPUMCAsmInfo : public MCAsmInfoELF {
public:
  explicit MyCPUMCAsmInfo(const MCTargetOptions &Options, const Triple &TT);
};

} // namespace llvm

#endif
