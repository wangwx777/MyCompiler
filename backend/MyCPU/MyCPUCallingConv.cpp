//===-- MyCPUCallingConv.cpp - MyCPU Calling Convention -------------------===//

#include "MyCPU.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

// The actual calling convention logic is implemented in
// MyCPUISelLowering.cpp using lambdas in CCState::AnalyzeFormalArguments
// and CCState::AnalyzeCallOperands.
//
// This file exists for compatibility with the build system and
// future expansion of calling convention support.

// Calling convention analysis is implemented directly in
// MyCPUISelLowering.cpp using lambdas for CCState.
// No TableGen-generated CC file is needed.
