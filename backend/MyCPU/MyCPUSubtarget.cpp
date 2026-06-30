//===-- MyCPUSubtarget.cpp - MyCPU Subtarget Information ------------------===//
//
// ★ Subtarget — 目标特定特性和子组件容器 ★
//
// Subtarget 是后端组件的容器，持有：
//   - FrameLowering (栈帧管理)
//   - InstrInfo (指令操作 + 寄存器信息)
//   - TargetLowering (ISel 降级策略)
//   - SelectionDAGTargetInfo (DAG 选择器信息)
//
// ★ 添加 Subtarget 特性(如 "has-mul" 表示有乘法器)：
//   1. 在 MyCPUFeatures.td 中定义 SubtargetFeature
//   2. 在 MyCPUSubtarget.h 中添加 XXX field 和 hasXXX() 查询
//   3. 在此文件的 initializeSubtargetDependencies() 中解析
//   4. 在 MyCPUISelLowering.cpp 中根据 hasXXX() 设置 Legal/Expand
//
//===----------------------------------------------------------------------===//

#include "MyCPUSubtarget.h"
#include "MyCPU.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"

using namespace llvm;

#define DEBUG_TYPE "mycpu-subtarget"

// ★ 包含 TableGen 生成的 Subtarget 代码
#define GET_SUBTARGETINFO_ENUM
#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "MyCPUGenSubtargetInfo.inc"

//===----------------------------------------------------------------------===//
// 构造函数 — 初始化所有子组件
//
// ★ 初始化顺序：
//   1. 调用 TableGen 生成的父类构造函数 MyCPUGenSubtargetInfo
//   2. 初始化直接成员: FrameLowering → InstrInfo → TLInfo
//   3. 调用 initializeSubtargetDependencies() 解析 CPU 和 Feature 字串
//   4. 创建 SelectionDAGTargetInfo
//===----------------------------------------------------------------------===//

MyCPUSubtarget::MyCPUSubtarget(const Triple &TT, StringRef CPU, StringRef FS,
                               const TargetMachine &TM)
    : MyCPUGenSubtargetInfo(TT, CPU, /*TuneCPU=*/CPU, FS),
      FrameLowering(*this),
      InstrInfo(*this),
      TLInfo(TM, *this),
      HasBase(false) {
  initializeSubtargetDependencies(TT, CPU, FS);
  TSInfo = std::make_unique<SelectionDAGTargetInfo>();
}

//===----------------------------------------------------------------------===//
// initializeSubtargetDependencies — 解析 CPU 和 Feature 字串
//
// ★ Feature 字串格式: "+feature1,-feature2,+feature3"
// ParseSubtargetFeatures 解析后设置对应的 bool 字段
//
// ★ 添加新特性的解析逻辑：
//   此处调用 ParseSubtargetFeatures() 会自动处理
//   无需手动添加代码
//===----------------------------------------------------------------------===//

MyCPUSubtarget &
MyCPUSubtarget::initializeSubtargetDependencies(const Triple &TT,
                                                 StringRef CPU,
                                                 StringRef FS) {
  if (CPU.empty())
    CPU = "generic-mycpu";

  ParseSubtargetFeatures(CPU, /*TuneCPU=*/CPU, FS);
  HasBase = true;
  return *this;
}
