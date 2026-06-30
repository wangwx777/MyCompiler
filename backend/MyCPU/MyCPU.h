//===-- MyCPU.h - Top-level interface for MyCPU representation --*- C++ -*-===//
//
// This file contains the entry points for global functions defined in the
// MyCPU target library.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MYCPU_MYCPU_H
#define LLVM_LIB_TARGET_MYCPU_MYCPU_H

#include "MCTargetDesc/MyCPUMCTargetDesc.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {

class FunctionPass;
class MyCPUTargetMachine;

FunctionPass *createMyCPUISelDag(MyCPUTargetMachine &TM);

} // namespace llvm

#endif
