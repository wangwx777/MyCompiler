//===-- MyCPUMachineFunctionInfo.cpp - MyCPU MachineFunctionInfo ----------===//

#include "MyCPUMachineFunctionInfo.h"

using namespace llvm;

MyCPUMachineFunctionInfo::MyCPUMachineFunctionInfo(
    const Function &F, const TargetSubtargetInfo *STI)
    : MachineFunctionInfo() {}

MachineFunctionInfo *MyCPUMachineFunctionInfo::clone(
    BumpPtrAllocator &Allocator, MachineFunction &DestMF,
    const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
    const {
  return DestMF.cloneInfo<MyCPUMachineFunctionInfo>(*this);
}
