//===-- MyCPU.td - MyCPU Target Description -----------------*- tablegen -*-===//
//
// This file provides the root target description for the MyCPU backend.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Target-independent interfaces
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// MyCPU subtarget features.
//===----------------------------------------------------------------------===//

include "MyCPUFeatures.td"

//===----------------------------------------------------------------------===//
// MyCPU processor definitions.
//===----------------------------------------------------------------------===//

include "MyCPUProcessors.td"

//===----------------------------------------------------------------------===//
// MyCPU scheduling model.
//===----------------------------------------------------------------------===//

include "MyCPUSchedule.td"

//===----------------------------------------------------------------------===//
// Register file, instruction descriptions, and calling convention.
//===----------------------------------------------------------------------===//

include "MyCPURegisterInfo.td"
include "MyCPUInstrFormats.td"
include "MyCPUInstrInfo.td"
include "MyCPUCallingConv.td"

//===----------------------------------------------------------------------===//
// Declare the MyCPU target.
//===----------------------------------------------------------------------===//

def MyCPUInstrInfo : InstrInfo;

def MyCPUAsmWriter : AsmWriter {
  string AsmWriterClassName = "InstPrinter";
  int Variant = 0;
}

def MyCPU : Target {
  let InstructionSet = MyCPUInstrInfo;
  let AssemblyWriters = [MyCPUAsmWriter];
  let AllowRegisterRenaming = 1;
}
