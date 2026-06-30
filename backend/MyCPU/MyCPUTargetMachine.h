//===-- MyCPUTargetMachine.h - TargetMachine for MyCPU -----------*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_MYCPU_MYCPUTARGETMACHINE_H
#define LLVM_LIB_TARGET_MYCPU_MYCPUTARGETMACHINE_H

#include "MyCPUSubtarget.h"
#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "llvm/CodeGen/Passes.h"
#include <optional>

namespace llvm {

class MyCPUTargetMachine : public CodeGenTargetMachineImpl {
public:
  MyCPUTargetMachine(const Target &T, const Triple &TT,
                     StringRef CPU, StringRef FS,
                     const TargetOptions &Options,
                     std::optional<Reloc::Model> RM,
                     std::optional<CodeModel::Model> CM,
                     CodeGenOptLevel OL, bool JIT);

  ~MyCPUTargetMachine() override;

  const MyCPUSubtarget *getSubtargetImpl(const Function &F) const override;
  const MyCPUSubtarget *getSubtargetImpl() const = delete;

  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }

  MachineFunctionInfo *
  createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
                            const TargetSubtargetInfo *STI) const override;

  TargetTransformInfo getTargetTransformInfo(const Function &F) const override;

  bool isNoopAddrSpaceCast(unsigned SrcAS, unsigned DestAS) const override {
    return true;
  }

private:
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  mutable StringMap<std::unique_ptr<MyCPUSubtarget>> SubtargetMap;
};

} // namespace llvm

#endif
