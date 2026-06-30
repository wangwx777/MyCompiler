//===-- MyCPUMachineFunctionInfo.h - MyCPU MachineFunctionInfo ---*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_MYCPU_MYCPUMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_MYCPU_MYCPUMACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

class MyCPUMachineFunctionInfo : public MachineFunctionInfo {
public:
  MyCPUMachineFunctionInfo(const Function &F, const TargetSubtargetInfo *STI);

  MachineFunctionInfo *
  clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
        const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
      const override;

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  void setVarArgsFrameIndex(int FI) { VarArgsFrameIndex = FI; }

  unsigned getCalleeSavedFrameSize() const { return CalleeSavedFrameSize; }
  void setCalleeSavedFrameSize(unsigned Size) { CalleeSavedFrameSize = Size; }

  bool getHasFP() const { return HasFP; }
  void setHasFP(bool V) { HasFP = V; }

private:
  int VarArgsFrameIndex = 0;
  unsigned CalleeSavedFrameSize = 0;
  bool HasFP = false;
};

} // namespace llvm

#endif
