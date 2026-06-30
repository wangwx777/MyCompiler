//===-- MyCPUSubtarget.h - MyCPU Subtarget Information -----------*- C++ -*-===//
//
// ★ Subtarget — 后端各组件的容器和特性查询接口 ★
//
// 包含：
//   - FrameLowering  — prologue/epilogue 生成
//   - InstrInfo      — 指令操作(含 RegisterInfo)
//   - TLInfo         — ISel 降级策略
//   - SelectionDAGTargetInfo — DAG 选择器辅助
//
// ★ 添加 Subtarget 特性(例如 hasMul、hasDiv)：
//   1. 在 MyCPUFeatures.td 中定义 SubtargetFeature
//   2. 在此添加对应的 bool 字段和 hasXxx() 查询
//   3. 在 .cpp 的 ParseSubtargetFeatures() 调用后自动设置
//   4. 在 MyCPUISelLowering 中根据 hasXxx() 改变 Legal/Expand
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MYCPU_MYCPUSUBTARGET_H
#define LLVM_LIB_TARGET_MYCPU_MYCPUSUBTARGET_H

#include "MyCPUFrameLowering.h"
#include "MyCPUISelLowering.h"
#include "MyCPUInstrInfo.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DataLayout.h"
#include <string>

// ★ 包含 TableGen 生成的 SubtargetInfo 声明
#define GET_SUBTARGETINFO_HEADER
#include "MyCPUGenSubtargetInfo.inc"

namespace llvm {

class MyCPUSubtarget : public MyCPUGenSubtargetInfo {
public:
  MyCPUSubtarget(const Triple &TT, StringRef CPU, StringRef FS,
                 const TargetMachine &TM);

  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);
  MyCPUSubtarget &initializeSubtargetDependencies(const Triple &TT,
                                                   StringRef CPU,
                                                   StringRef FS);

  //===--- 子组件获取接口 ---===//
  const MyCPUInstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const MyCPURegisterInfo *getRegisterInfo() const override {
    return &InstrInfo.getRegisterInfo();
  }
  const MyCPUTargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }
  const MyCPUFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  const SelectionDAGTargetInfo *getSelectionDAGInfo() const override {
    return TSInfo.get();
  }

  //===--- 特性查询 ---===//
  // ★ 当前只有一个特性: HasBase (基本 ISA 支持)
  // 后续添加新特性时在此添加 hasXxx() 方法
  bool hasBase() const { return HasBase; }

private:
  // ★ 子组件 — 作为直接成员(非指针)，避免间接寻址开销
  MyCPUFrameLowering FrameLowering;
  MyCPUInstrInfo InstrInfo;
  MyCPUTargetLowering TLInfo;
  std::unique_ptr<SelectionDAGTargetInfo> TSInfo;

  // ★ SubtargetFeature 对应的 bool 字段
  // ParseSubtargetFeatures() 根据命令行和函数属性自动设置
  bool HasBase;
  bool Is64Bit;
  bool HasMulDiv;
};

} // namespace llvm

#endif
