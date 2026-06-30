//===-- MyCPUTargetInfo.cpp - MyCPU Target Implementation -----------------===//

#include "TargetInfo/MyCPUTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

Target &llvm::getTheMyCPUTarget() {
  static Target TheMyCPUTarget;
  return TheMyCPUTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeMyCPUTargetInfo() {
  RegisterTarget<Triple::mycpu> X(getTheMyCPUTarget(), "mycpu",
                                   "MyCPU 32-bit", "MyCPU");
}
