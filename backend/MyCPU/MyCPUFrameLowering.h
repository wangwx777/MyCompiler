//===-- MyCPUFrameLowering.h - MyCPU Frame Lowering -------------*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_MYCPU_MYCPUFRAMELOWERING_H
#define LLVM_LIB_TARGET_MYCPU_MYCPUFRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {

class MyCPUSubtarget;

class MyCPUFrameLowering : public TargetFrameLowering {
public:
  explicit MyCPUFrameLowering(const MyCPUSubtarget &STI);

  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  StackOffset getFrameIndexReference(const MachineFunction &MF, int FI,
                                     Register &FrameReg) const override;

  void determineCalleeSaves(MachineFunction &MF, BitVector &SavedRegs,
                            RegScavenger *RS) const override;

  bool hasFPImpl(const MachineFunction &MF) const override;
  bool hasReservedCallFrame(const MachineFunction &MF) const override;
  void processFunctionBeforeFrameFinalized(MachineFunction &MF,
                                           RegScavenger *RS) const override;

  bool canSimplifyCallFramePseudos(
      const MachineFunction &MF) const override {
    return false;
  }

  bool spillCalleeSavedRegisters(
      MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
      ArrayRef<CalleeSavedInfo> CSI,
      const TargetRegisterInfo *TRI) const override;

  bool restoreCalleeSavedRegisters(
      MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
      MutableArrayRef<CalleeSavedInfo> CSI,
      const TargetRegisterInfo *TRI) const override;
};

} // namespace llvm

#endif
