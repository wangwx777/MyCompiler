//===-- MyCPUISelLowering.h - MyCPU DAG Lowering Interface ------*- C++ -*-===//
//
// ★ TargetLowering 子类 — 定义 MyCPU 的指令降级策略 ★
//
// 自定义 SDNode 类型 (MyCPUISD namespace):
//   RetFlag — 函数返回标记，在 ISel 阶段替换为 RET 指令
//   Call    — 函数调用节点，在 ISel 阶段替换为 CALL 指令
//   Wrapper — 全局地址/块地址包装，在 ISel 阶段展开为 MOVIW+ADDIW
//
// ★ 添加新的自定义 SDNode:
//   1. 在 MyCPUISD::NodeType 枚举中添加(在 FIRST_NUMBER 之后)
//   2. 在 MyCPUISelLowering.cpp 中实现对应的 LowerXXX() 方法
//   3. 在 MyCPUISelDAGToDAG.cpp 的 Select() 中添加 MachineNode 选择
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MYCPU_MYCPUISELLOWERING_H
#define LLVM_LIB_TARGET_MYCPU_MYCPUISELLOWERING_H

#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

class MyCPUSubtarget;

//===----------------------------------------------------------------------===//
// MyCPUISD — 自定义 SDNode 操作码
//
// ★ ISD::BUILTIN_OP_END 之后的编号为后端自定义节点
// 这些节点在 IR Lowering 阶段创建，在 ISel 阶段被替换为实际指令
//===----------------------------------------------------------------------===//

namespace MyCPUISD {
enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,

  // ★ RetFlag: 返回标记节点 [chain] → [chain]
  // 在 LowerReturn() 中创建，由 ISel 替换为 RET 指令
  RetFlag,

  // ★ Call: 函数调用节点 [chain, callee] → [chain, glue]
  // 在 LowerCall() 中创建，由 ISel 替换为 CALL 指令
  Call,

  // ★ Wrapper: 地址包装节点 [target_global/target_block] → [i32]
  // 在 LowerGlobalAddress/LowerBlockAddress 中创建
  // 由 ISel 展开为 MOVIW + ADDIW 序列
  Wrapper,

  // ★ Cmp: 比较节点 [chain, lhs, rhs, msb, lsb, f] → [chain]
  // 在 LowerBR_CC 中创建，由 ISel 替换为 CMPW 指令
  Cmp,

  // ★ BrZ: 零标志跳转 [chain, f, dest] → [chain]
  // ★ BrNZ: 零标志不跳转 [chain, f, dest] → [chain]
  // 在 LowerBR_CC 中创建，由 ISel 替换为 BZW/BNZ 指令
  BrZ,
  BrNZ,
};
} // namespace MyCPUISD

//===----------------------------------------------------------------------===//
// MyCPUTargetLowering — 指令降级策略
//
// ★ 负责 C 语言操作的降级：将 LLVM IR 操作映射到目标指令
//
// 核心方法:
//   - LowerFormalArguments() — 函数参数传入
//   - LowerReturn()          — 函数值返回
//   - LowerCall()            — 函数调用
//   - LowerOperation()       — Custom 操作分发
//   - LowerBR_CC()           — 条件分支降级
//   - LowerGlobalAddress()   — 全局地址降级
//
// ★ 添加新操作降级:
//   1. 在 .cpp 构造函数中 setOperationAction()
//   2. 声明 private LowerXXX() 方法
//   3. 在 LowerOperation() switch 中添加 case
//===----------------------------------------------------------------------===//

class MyCPUTargetLowering : public TargetLowering {
public:
  explicit MyCPUTargetLowering(const TargetMachine &TM,
                               const MyCPUSubtarget &STI);

  //===--- 调用约定 ---===//
  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals,
                      const SDLoc &DL, SelectionDAG &DAG) const override;

  SDValue LowerCall(TargetLowering::CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

  //===--- Custom 操作降级分发 ---===//
  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;

  //===--- 内联汇编支持 ---===//
  ConstraintType getConstraintType(StringRef Constraint) const override;
  std::pair<unsigned, const TargetRegisterClass *>
  getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                               StringRef Constraint, MVT VT) const override;

  //===--- 调试支持 ---===//
  const char *getTargetNodeName(unsigned Opcode) const override;

private:
  const MyCPUSubtarget &Subtarget;

  // ★ 各操作的降级辅助方法
  SDValue LowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerBlockAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerBR_CC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSELECT_CC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerFRAMEADDR(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerRETURNADDR(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVASTART(SDValue Op, SelectionDAG &DAG) const;

  // ★ 条件码 → 分支指令映射
  unsigned getBranchOpcodeForCond(ISD::CondCode CC) const;
};

} // namespace llvm

#endif
