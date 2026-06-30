//===-- MyCPUInstrInfo.h - MyCPU Instruction Information ---------*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_MYCPU_MYCPUINSTRINFO_H
#define LLVM_LIB_TARGET_MYCPU_MYCPUINSTRINFO_H

#include "MyCPURegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "MyCPUGenInstrInfo.inc"

namespace llvm {

class MyCPUSubtarget;

class MyCPUInstrInfo : public MyCPUGenInstrInfo {
public:
  explicit MyCPUInstrInfo(MyCPUSubtarget &STI);

  const MyCPURegisterInfo &getRegisterInfo() const { return RegInfo; }

  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
                   const DebugLoc &DL, Register DestReg, Register SrcReg,
                   bool KillSrc,
                   bool RenamableDest = false,
                   bool RenamableSrc = false) const override;

  void storeRegToStackSlot(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MI,
                           Register SrcReg, bool isKill, int FrameIndex,
                           const TargetRegisterClass *RC, Register VReg,
                           MachineInstr::MIFlag Flags = MachineInstr::NoFlags) const override;

  void loadRegFromStackSlot(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MI,
                            Register DestReg, int FrameIndex,
                            const TargetRegisterClass *RC, Register VReg,
                            unsigned SubReg = 0,
                            MachineInstr::MIFlag Flags = MachineInstr::NoFlags) const override;

  bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                     MachineBasicBlock *&FBB,
                     SmallVectorImpl<MachineOperand> &Cond,
                     bool AllowModify) const override;

  unsigned insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                        MachineBasicBlock *FBB, ArrayRef<MachineOperand> Cond,
                        const DebugLoc &DL,
                        int *BytesAdded = nullptr) const override;

  unsigned removeBranch(MachineBasicBlock &MBB,
                        int *BytesRemoved = nullptr) const override;

  bool reverseBranchCondition(
      SmallVectorImpl<MachineOperand> &Cond) const override;

  bool isSchedulingBoundary(const MachineInstr &MI,
                            const MachineBasicBlock *MBB,
                            const MachineFunction &MF) const override;

  Register getGlobalBaseReg(MachineFunction *MF) const;

private:
  const MyCPURegisterInfo RegInfo;
};

} // namespace llvm

#endif
