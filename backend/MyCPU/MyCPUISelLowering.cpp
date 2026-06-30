//===-- MyCPUISelLowering.cpp - MyCPU DAG Lowering Implementation ---------===//
//
// ★ 这是实现指令选择降级(TargetLowering)的核心文件 ★
//
// 职责：
//   1. 声明哪些操作是 Legal/Expand/Custom/Promote
//   2. 实现调用约定(参数传递、返回值)
//   3. 实现 Custom 操作的降级逻辑(如 BR_CC、GlobalAddress)
//   4. 实现内联汇编约束
//
// ★ 添加新操作降级的步骤：
//   1. 在构造函数中设置 setOperationAction(ISD::XXX, MVT::xxx, Legal/Custom/...)
//   2. 如果是 Custom: 在 LowerOperation() 中添加 case，实现 LowerXXX() 方法
//   3. 在 MyCPUISelDAGToDAG.cpp 的 Select() 中添加对应的 MachineNode 选择
//   4. 必要时在 MyCPUInstrInfo.td 中添加 Pat<> 模式
//
//===----------------------------------------------------------------------===//

#include "MyCPUISelLowering.h"
#include "MyCPU.h"
#include "MyCPUSubtarget.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "mycpu-lower"

#define GET_REGINFO_ENUM
#include "MyCPUGenRegisterInfo.inc"
#define GET_INSTRINFO_ENUM
#include "MyCPUGenInstrInfo.inc"

// ★ 参数寄存器列表：A0-A6 (R1-R7) 用于传递函数参数
// 如需增加参数寄存器，在此数组中追加即可
static const MCPhysReg ArgRegs[] = {
    MyCPU::A0, MyCPU::A1, MyCPU::A2, MyCPU::A3,
    MyCPU::A4, MyCPU::A5, MyCPU::A6
};
static const unsigned NumArgRegs = std::size(ArgRegs);

// ★ 调用约定分配函数 — CCState 回调
static bool CC_MyCPU_Assign(unsigned ValNo, MVT ValVT, MVT LocVT,
                            CCValAssign::LocInfo LocInfo,
                            ISD::ArgFlagsTy ArgFlags, Type * /*OrigTy*/,
                            CCState &State) {
  if (LocVT == MVT::i32) {
    MCRegister Reg = State.AllocateReg(ArgRegs);
    if (Reg) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }
  unsigned Offset = State.AllocateStack(4, Align(4));
  State.addLoc(CCValAssign::getMem(ValNo, ValVT, Offset, LocVT, LocInfo));
  return false;
}

//===----------------------------------------------------------------------===//
// MyCPUTargetLowering Constructor
//
// ★ 最重要的配置点：在此声明每个 ISD 操作节点的处理策略
//
// 四种策略：
//   Legal    — 指令集直接支持，由 TableGen Pat<> 模式匹配
//   Expand   — LLVM 自动展开为更小的操作(如 MUL→移位加法序列)
//   Custom   — 需要手写 LowerXXX() 降级逻辑
//   Promote  — 提升到更宽的位宽(i8/i16 → i32)
//===----------------------------------------------------------------------===//

MyCPUTargetLowering::MyCPUTargetLowering(const TargetMachine &TM,
                                         const MyCPUSubtarget &STI)
    : TargetLowering(TM, STI), Subtarget(STI) {

  // ★ 注册寄存器类 — i32/i16/i8 映射到 GPR
  // FREG 不在此注册：FREG 是标志寄存器(8bit, Z/C/N/V)，不应被寄存分配器用于数据值
  addRegisterClass(MVT::i32, &MyCPU::GPRRegClass);
  addRegisterClass(MVT::i16, &MyCPU::GPRRegClass);
  addRegisterClass(MVT::i8,  &MyCPU::GPRRegClass);

  // 根据寄存器类计算派生属性(如最大寄存器大小)
  computeRegisterProperties(STI.getRegisterInfo());

  //===--- 地址计算 ---===//
  // GlobalAddress 不能直接编码为立即数，需要 MOVIW + ADDIW 序列
  setOperationAction(ISD::GlobalAddress, MVT::i32, Custom);

  //===--- 整数运算：Legal ---===//
  // ★ 这些操作直接对应 MyCPU 指令，由 TableGen Pat<> 匹配
  // ADDW/SUBW/ANDW/ORW/XORW 指令
  setOperationAction(ISD::ADD, MVT::i32, Legal);
  setOperationAction(ISD::SUB, MVT::i32, Legal);
  setOperationAction(ISD::AND, MVT::i32, Legal);
  setOperationAction(ISD::OR,  MVT::i32, Legal);
  setOperationAction(ISD::XOR, MVT::i32, Legal);

  //===--- 移位：Legal ---===//
  // SHLW/SHRW/SARW 指令
  setOperationAction(ISD::SHL, MVT::i32, Legal);
  setOperationAction(ISD::SRL, MVT::i32, Legal);
  setOperationAction(ISD::SRA, MVT::i32, Legal);

  //===--- 内存访问：Legal ---===//
  // LDW/STW 指令
  setOperationAction(ISD::LOAD,  MVT::i32, Legal);
  setOperationAction(ISD::STORE, MVT::i32, Legal);

  //===--- 不支持的操作：Expand ---===//
  // ★ 如果没有硬件乘法器/除法器，Expand 会展开为库调用或移位加法序列
  // 如果后续添加了 MULW/DIVW 指令，将下面改为 Legal 即可
  setOperationAction(ISD::MUL,    MVT::i32, Expand);
  setOperationAction(ISD::SDIV,   MVT::i32, Expand);
  setOperationAction(ISD::UDIV,   MVT::i32, Expand);
  setOperationAction(ISD::SREM,   MVT::i32, Expand);
  setOperationAction(ISD::UREM,   MVT::i32, Expand);
  setOperationAction(ISD::MULHU,  MVT::i32, Expand);
  setOperationAction(ISD::MULHS,  MVT::i32, Expand);

  //===--- 小类型：i8/i16 Legal ---===//
  // Half/Byte 格式指令直接支持 8-bit 和 16-bit 操作
  // 截断(truncate)模式在 MyCPUInstrInfo.td 中通过 MOVB/MOVH 匹配
  setOperationAction(ISD::ADD, MVT::i8, Legal);
  setOperationAction(ISD::ADD, MVT::i16, Legal);
  setOperationAction(ISD::SUB, MVT::i8, Legal);
  setOperationAction(ISD::SUB, MVT::i16, Legal);
  setOperationAction(ISD::AND, MVT::i8, Legal);
  setOperationAction(ISD::AND, MVT::i16, Legal);
  setOperationAction(ISD::OR,  MVT::i8, Legal);
  setOperationAction(ISD::OR,  MVT::i16, Legal);
  setOperationAction(ISD::XOR, MVT::i8, Legal);
  setOperationAction(ISD::XOR, MVT::i16, Legal);
  setOperationAction(ISD::LOAD, MVT::i8, Legal);
  setOperationAction(ISD::LOAD, MVT::i16, Legal);
  setOperationAction(ISD::STORE, MVT::i8, Legal);
  setOperationAction(ISD::STORE, MVT::i16, Legal);

  //===--- 64 位类型：Expand ---===//
  // ★ 32 位 CPU 不支持 64 位操作，展开为 32 位对
  setOperationAction(ISD::ADD, MVT::i64, Expand);
  setOperationAction(ISD::SUB, MVT::i64, Expand);
  setOperationAction(ISD::LOAD, MVT::i64, Expand);
  setOperationAction(ISD::STORE, MVT::i64, Expand);

  //===--- 分支和选择 ---===//
  // ★ BR_CC: Legal → 直接由 ISel Select() 展开为 CMPW + BZW/BNZ 序列
  // BRCOND/BR_JT/BRIND: Expand → LLVM 展开
  // SELECT_CC 由 LLVM 展开为控制流
  setOperationAction(ISD::BR_JT,     MVT::Other, Expand);
  setOperationAction(ISD::BRIND,     MVT::Other, Expand);
  setOperationAction(ISD::BRCOND,    MVT::Other, Expand);
  setOperationAction(ISD::BR_CC,     MVT::i32, Legal);
  setOperationAction(ISD::BR_CC,     MVT::i16, Legal);
  setOperationAction(ISD::BR_CC,     MVT::i8, Legal);
  setOperationAction(ISD::SELECT_CC, MVT::i32, Expand);

  // SETCC 展开为 cmp + 条件码的组合
  setOperationAction(ISD::SETCC, MVT::i32, Expand);

  //===--- 地址相关：Custom ---===//
  setOperationAction(ISD::BlockAddress, MVT::i32, Custom);
  setOperationAction(ISD::FRAMEADDR,    MVT::i32, Custom);
  setOperationAction(ISD::RETURNADDR,   MVT::i32, Custom);

  // 可变参数暂不支持
  setOperationAction(ISD::VASTART, MVT::Other, Custom);

  // ★ 栈指针寄存器 — 当需要跨调用保存/恢复 SP 时使用
  setStackPointerRegisterToSaveRestore(MyCPU::SP);

  // 栈对齐和原子操作限制
  setMinStackArgumentAlignment(Align(4));
  setMaxAtomicSizeInBitsSupported(0); // 不支持原子指令
}

//===----------------------------------------------------------------------===//
// Target Node Names
// ★ 调试用：返回自定义 SDNode 的名称
// 添加新的 Custom SDNode (在 MyCPUISelLowering.h 中声明) 后，
// 在此添加对应的名称
//===----------------------------------------------------------------------===//

const char *MyCPUTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch ((MyCPUISD::NodeType)Opcode) {
  case MyCPUISD::RetFlag:
    return "MyCPUISD::RetFlag";
  case MyCPUISD::Call:
    return "MyCPUISD::Call";
  case MyCPUISD::Wrapper:
    return "MyCPUISD::Wrapper";
  case MyCPUISD::Cmp:
    return "MyCPUISD::Cmp";
  case MyCPUISD::BrZ:
    return "MyCPUISD::BrZ";
  case MyCPUISD::BrNZ:
    return "MyCPUISD::BrNZ";
  default:
    return nullptr;
  }
}

//===----------------------------------------------------------------------===//
// Formal Arguments Lowering
//
// ★ 调用约定 — 函数入口参数处理
//
// 参数传递规则：
//   前 7 个 i32 参数 → 寄存器 A0-A6 (R1-R7)
//   超出部分        → 栈传递 (caller 在调用前 push)
//
// 修改调用约定时注意：
//   - 增加参数寄存器：修改 ArgRegs[] 数组
//   - 改变寄存器使用：同步修改 MyCPUCallingConv.td 中的 CSR 列表
//   - 栈传递参数从低地址到高地址排列
//===----------------------------------------------------------------------===//

SDValue MyCPUTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {

  MachineFunction &MF = DAG.getMachineFunction();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();

  // ★ 分配每个参数的位置(寄存器或栈)
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());

  // ★ 调用约定分配 — 使用 CCState 内置的寄存器/栈分配
  CCInfo.AnalyzeFormalArguments(Ins, CC_MyCPU_Assign);

  // 为每个参数创建 CopyFromReg 或 Load
  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    if (VA.isRegLoc()) {
      // ★ 寄存器参数：创建虚拟寄存器并标记为 LiveIn
      Register VReg = RegInfo.createVirtualRegister(&MyCPU::GPRRegClass);
      RegInfo.addLiveIn(VA.getLocReg(), VReg);
      SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, VReg, MVT::i32);
      InVals.push_back(ArgValue);
    } else {
      // ★ 栈参数：通过 FrameIndex 加载
      int FI = MF.getFrameInfo().CreateFixedObject(4, VA.getLocMemOffset(),
                                                    true);
      SDValue FIN = DAG.getFrameIndex(FI, MVT::i32);
      SDValue ArgValue = DAG.getLoad(MVT::i32, DL, Chain, FIN,
                                     MachinePointerInfo());
      InVals.push_back(ArgValue);
    }
  }

  return Chain;
}

//===----------------------------------------------------------------------===//
// Return Lowering
//
// ★ 函数返回值处理
// 当前只支持单个 i32 返回值(放在 A0 中)
// 如需支持多返回值，需要分配多个返回值寄存器
//===----------------------------------------------------------------------===//

SDValue MyCPUTargetLowering::LowerReturn(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
    SelectionDAG &DAG) const {

  // ★ 返回值放入 A0 (R1)
  if (!Outs.empty()) {
    SDValue RetVal = OutVals[0];
    Chain = DAG.getCopyToReg(Chain, DL, MyCPU::A0, RetVal, SDValue());
  }

  // ★ 生成 RetFlag — 在 ISel 阶段替换为 RET 指令
  return DAG.getNode(MyCPUISD::RetFlag, DL, MVT::Other, Chain);
}

//===----------------------------------------------------------------------===//
// Call Lowering
//
// ★ 函数调用处理
//
// 调用序列：
//   1. 将参数复制到 A0-A6 寄存器(超出部分写入栈)
//   2. 发射 CALL 指令
//   3. 从 A0 复制返回值
//
// 修改调用约定时注意与 LowerFormalArguments 保持一致
//===----------------------------------------------------------------------===//

SDValue MyCPUTargetLowering::LowerCall(
    TargetLowering::CallLoweringInfo &CLI,
    SmallVectorImpl<SDValue> &InVals) const {

  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  CallingConv::ID CallConv = CLI.CallConv;
  bool IsVarArg = CLI.IsVarArg;

  // ★ 分配参数位置
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());

  CCInfo.AnalyzeCallOperands(Outs, CC_MyCPU_Assign);

  // ★ 将实参复制到指定的寄存器/栈位置
  SmallVector<SDValue, 8> MemOpChains;
  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    SDValue Arg = OutVals[i];

    if (VA.isRegLoc()) {
      Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), Arg, SDValue());
    } else {
      // 栈传递参数
      int FI = DAG.getMachineFunction().getFrameInfo().CreateFixedObject(
          4, VA.getLocMemOffset(), true);
      SDValue Ptr = DAG.getFrameIndex(FI, MVT::i32);
      SDValue Store = DAG.getStore(Chain, DL, Arg, Ptr, MachinePointerInfo());
      MemOpChains.push_back(Store);
    }
  }

  // 合并所有内存操作链
  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

  // ★ 发射 CALL 节点 — 在 ISel 阶段替换为 CALL 指令
  SDValue Ops[] = {Chain, Callee};
  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  Chain = DAG.getNode(MyCPUISD::Call, DL, NodeTys, Ops);

  // ★ 从 A0 读取返回值
  if (!Ins.empty()) {
    SDValue Copy = DAG.getCopyFromReg(Chain, DL, MyCPU::A0, MVT::i32,
                                      Chain.getValue(1));
    InVals.push_back(Copy);
  }

  return Chain;
}

//===----------------------------------------------------------------------===//
// Operation Lowering — 分发器
//
// ★ 新增 Custom 操作降级时：
//   1. 在此 switch 中添加 case
//   2. 实现对应的 LowerXXX() 方法
//   3. 在 .h 文件中声明该方法
//===----------------------------------------------------------------------===//

SDValue MyCPUTargetLowering::LowerOperation(SDValue Op,
                                             SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::GlobalAddress:
    return LowerGlobalAddress(Op, DAG);
  case ISD::BlockAddress:
    return LowerBlockAddress(Op, DAG);
  case ISD::FRAMEADDR:
    return LowerFRAMEADDR(Op, DAG);
  case ISD::RETURNADDR:
    return LowerRETURNADDR(Op, DAG);
  case ISD::VASTART:
    return LowerVASTART(Op, DAG);
  default:
    llvm_unreachable("Unexpected node to lower");
  }
}

//===----------------------------------------------------------------------===//
// Global Address Lowering
//
// ★ 全局变量/函数地址加载
// Wrapper 节点封装了目标特定的地址表示
// 在 ISel 阶段会被展开为 MOVIW + ADDIW 序列(如果需要重定位)
//===----------------------------------------------------------------------===//

SDValue MyCPUTargetLowering::LowerGlobalAddress(SDValue Op,
                                                 SelectionDAG &DAG) const {
  const GlobalAddressSDNode *GA = cast<GlobalAddressSDNode>(Op);
  SDLoc DL(Op);
  return DAG.getNode(MyCPUISD::Wrapper, DL, MVT::i32,
                     DAG.getTargetGlobalAddress(GA->getGlobal(), DL, MVT::i32,
                                                GA->getOffset()));
}

SDValue MyCPUTargetLowering::LowerBlockAddress(SDValue Op,
                                                SelectionDAG &DAG) const {
  const BlockAddressSDNode *BA = cast<BlockAddressSDNode>(Op);
  SDLoc DL(Op);
  return DAG.getNode(MyCPUISD::Wrapper, DL, MVT::i32,
                     DAG.getTargetBlockAddress(BA->getBlockAddress(), MVT::i32,
                                               0));
}

//===----------------------------------------------------------------------===//
// Branch Condition Lowering
//
// ★ 核心降级：条件分支 BR_CC
//
// C 语言中的 if/while/for 最终都转化为 BR_CC 节点。
// 降级策略：
//   1. 发射 CMPW 比较指令 → 设置 F0 的 Z/C/N/V 标志位
//   2. 发射 BZW/BNZ 条件跳转 → 根据 F0.Z 决定是否跳转
//
// ★ 当前只实现了 EQ/NE 比较。其他条件(SLT/SGT/...)需要检查
//   CMP 结果的 N 和 V 标志位，暂用 fallback 处理。
//
// 如需添加新条件码支持：
//   1. 在 getBranchOpcodeForCond() 中添加 case
//   2. 可能需要发射多条指令序列(如 AND + BNZ)
//===----------------------------------------------------------------------===//

SDValue MyCPUTargetLowering::LowerBR_CC(SDValue Op, SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);
  SDValue Dest = Op.getOperand(4);
  SDLoc DL(Op);

  // ★ 根据条件码选择对应的分支节点
  // SETEQ → BrZ (F0.Z == 1)
  // SETNE → BrNZ (F0.Z == 0)
  unsigned BccNode = getBranchOpcodeForCond(CC);

  // ★ 发射 Cmp + Bcc 序列
  // Cmp: 隐式设置 F0 的标志位 (msb=31, lsb=0 比较完整 32 位值)
  // BrZ/BrNZ: 根据 F0.Z 决定跳转
  SDValue Ops[] = {Chain, LHS, RHS,
                   DAG.getTargetConstant(31, DL, MVT::i32),  // msb
                   DAG.getTargetConstant(0, DL, MVT::i32),   // lsb
                   DAG.getTargetConstant(0, DL, MVT::i32)};  // F0
  Chain = DAG.getNode(MyCPUISD::Cmp, DL, MVT::Other, Ops);

  Chain = DAG.getNode(BccNode, DL, MVT::Other, Chain,
                      DAG.getTargetConstant(0, DL, MVT::i32), // F0
                      Dest);

  return Chain;
}

// ★ 条件码 → 分支指令映射
// 当前只支持 EQ/NE。添加更多条件时扩充此函数。
unsigned MyCPUTargetLowering::getBranchOpcodeForCond(ISD::CondCode CC) const {
  switch (CC) {
  case ISD::SETEQ:  return MyCPUISD::BrZ;   // 等于 → 零标志跳转
  case ISD::SETNE:  return MyCPUISD::BrNZ;  // 不等 → 非零跳转
  // ★ 以下条件码需要多指令序列实现，暂用 fallback
  case ISD::SETULT:  // 无符号小于
  case ISD::SETLT:   // 有符号小于 (需检查 N xor V)
  case ISD::SETULE:
  case ISD::SETLE:
  case ISD::SETUGT:
  case ISD::SETGT:
  case ISD::SETUGE:
  case ISD::SETGE:
  default:
    return MyCPUISD::BrNZ;
  }
}

//===----------------------------------------------------------------------===//
// SELECT_CC Lowering
//
// ★ select(cond, trueVal, falseVal) → 条件 MOV
// 当前未实现，由 LLVM 的 Expand 处理(通过分支+标签实现)
// 后续可以优化为：
//   CMP + BSET/BCLR (位操作控制 Freg) + 条件 MOV 序列
//===----------------------------------------------------------------------===//

SDValue MyCPUTargetLowering::LowerSELECT_CC(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  SDValue TrueV = Op.getOperand(2);
  SDValue FalseV = Op.getOperand(3);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();
  SDLoc DL(Op);

  // ★ 当前: 由 LLVM 展开为基本块+分支
  // TODO: 实现条件 MOV 指令后，在此处发射单条指令
  llvm_unreachable("SELECT_CC should be expanded");
}

//===----------------------------------------------------------------------===//
// Frame / Return Address Lowering
//===----------------------------------------------------------------------===//

SDValue MyCPUTargetLowering::LowerFRAMEADDR(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDLoc DL(Op);
  // ★ 帧地址直接读取 FP 寄存器的值
  return DAG.getCopyFromReg(DAG.getEntryNode(), DL, MyCPU::FP, MVT::i32);
}

SDValue MyCPUTargetLowering::LowerRETURNADDR(SDValue Op,
                                              SelectionDAG &DAG) const {
  SDLoc DL(Op);
  // ★ 返回地址直接读取 LR 寄存器的值
  return DAG.getCopyFromReg(DAG.getEntryNode(), DL, MyCPU::LR, MVT::i32);
}

SDValue MyCPUTargetLowering::LowerVASTART(SDValue Op,
                                           SelectionDAG &DAG) const {
  llvm_unreachable("Varargs not supported");
}

//===----------------------------------------------------------------------===//
// Inline Asm Support
//
// ★ 内联汇编约束支持
// 当前只支持 'r' 约束(通用寄存器)
//
// 添加约束类型示例：
//   'I' → 小立即数约束    → 返回 C_Other
//   'm' → 内存操作数约束  → 返回 C_Memory
//===----------------------------------------------------------------------===//

TargetLowering::ConstraintType
MyCPUTargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'r':
      return C_RegisterClass;
    default:
      break;
    }
  }
  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass *>
MyCPUTargetLowering::getRegForInlineAsmConstraint(
    const TargetRegisterInfo *TRI, StringRef Constraint, MVT VT) const {
  if (Constraint.size() == 1 && Constraint[0] == 'r')
    return std::make_pair(0U, &MyCPU::GPRRegClass);
  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}
