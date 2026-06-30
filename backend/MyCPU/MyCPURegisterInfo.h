//===-- MyCPURegisterInfo.h - MyCPU Register Information ---------*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_MYCPU_MYCPUREGISTERINFO_H
#define LLVM_LIB_TARGET_MYCPU_MYCPUREGISTERINFO_H

#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_REGINFO_HEADER
#include "MyCPUGenRegisterInfo.inc"

namespace llvm {

struct MyCPURegisterInfo : public MyCPUGenRegisterInfo {
  explicit MyCPURegisterInfo();

  const MCPhysReg *getCalleeSavedRegs(const MachineFunction *MF) const override;
  const uint32_t *getCallPreservedMask(const MachineFunction &MF,
                                       CallingConv::ID CC) const override;

  BitVector getReservedRegs(const MachineFunction &MF) const override;

  bool eliminateFrameIndex(MachineBasicBlock::iterator MI, int SPAdj,
                           unsigned FIOperandNum,
                           RegScavenger *RS = nullptr) const override;

  Register getFrameRegister(const MachineFunction &MF) const override;

  bool requiresRegisterScavenging(const MachineFunction &MF) const override {
    return true;
  }

  bool requiresFrameIndexScavenging(const MachineFunction &MF) const override {
    return true;
  }

  const TargetRegisterClass *
  getPointerRegClass(unsigned Kind = 0) const override;

  Register getStackRegister() const;
  Register getFramePointer() const;
};

} // namespace llvm

#endif
