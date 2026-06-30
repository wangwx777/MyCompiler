//===-- MyCPUISelDAGToDAG.cpp - MyCPU DAG-to-DAG Instruction Selector -----===//
//
// ★ 指令选择器(Instruction Selector) — SDNode → MachineSDNode 转换 ★
//
// 工作流程：
//   1. TableGen 自动生成 SelectCode() 函数(MyCPUGenDAGISel.inc)
//   2. Select() 先处理自定义 SDNode(Wrapper/RetFlag/Call/FrameIndex/Constant)
//   3. 其余 SDNode 交由 SelectCode() 自动匹配 Pat<> 模式
//
// ★ 添加新指令选择的步骤：
//   1. 如果新指令可以用 Pat<> 模式匹配 → 在 MyCPUInstrInfo.td 中添加
//   2. 如果需要手动选择 → 在 Select() 函数中添加 case
//   3. 使用 CurDAG->getMachineNode(Opc, DL, ...) 创建 MachineSDNode
//   4. 使用 ReplaceNode() 替换原 SDNode
//
// 常用 API：
//   CurDAG->getMachineNode(Opc, DL, [VT], [Operands...])  — 创建机器指令节点
//   CurDAG->getTargetFrameIndex(FI, VT)                     — 创建帧索引
//   CurDAG->getTargetConstant(Val, DL, VT)                  — 创建常量操作数
//   ReplaceNode(N, Result.getNode())                        — 替换节点
//
//===----------------------------------------------------------------------===//

#include "MyCPU.h"
#include "MyCPUTargetMachine.h"
#include "llvm/CodeGen/FunctionLoweringInfo.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "mycpu-isel"

#define GET_INSTRINFO_ENUM
#include "MyCPUGenInstrInfo.inc"
#define GET_REGINFO_ENUM
#include "MyCPUGenRegisterInfo.inc"

namespace {

class MyCPUDAGToDAGISel : public SelectionDAGISel {
public:
  MyCPUDAGToDAGISel(MyCPUTargetMachine &TM)
      : SelectionDAGISel(TM) {}

  void Select(SDNode *N) override;

  bool SelectAddrFI(SDValue Addr, SDValue &Base, SDValue &Offset);

#include "MyCPUGenDAGISel.inc"
};

} // namespace

static char ID = 0;

FunctionPass *llvm::createMyCPUISelDag(MyCPUTargetMachine &TM) {
  return new SelectionDAGISelLegacy(ID, std::make_unique<MyCPUDAGToDAGISel>(TM));
}

//===----------------------------------------------------------------------===//
// SelectAddrFI — 帧索引地址选择
//
// 用于 LDW/STW 指令的地址操作数：
//   LDW rd, base, offset   →  base=SP/FP, offset=帧偏移
//
// 如果地址是帧索引，返回 true 表示匹配成功。
// 实际的帧偏移由 eliminateFrameIndex 在寄存器分配后解决。
//===----------------------------------------------------------------------===//

bool MyCPUDAGToDAGISel::SelectAddrFI(SDValue Addr, SDValue &Base,
                                      SDValue &Offset) {
  if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i32);
    Offset = CurDAG->getTargetConstant(0, SDLoc(Addr), MVT::i32);
    return true;
  }
  return false;
}

//===----------------------------------------------------------------------===//
// Select() — 核心选择函数
//
// ★ 对每个 SDNode 调用，选择合适的 MachineInstruction
//
// 处理顺序：
//   1. FrameIndex      → 替换为 TargetFrameIndex
//   2. Wrapper         → 全局地址/块地址展开为 MOVIW 序列
//   3. RetFlag         → RET 伪指令
//   4. Call            → CALL 伪指令
//   5. Constant        → 小常量 → MOVIW，大常量 → 展开
//   6. default         → 交由 TableGen 生成的 SelectCode() 自动匹配
//
// ★ 添加新的自定义选择逻辑时，在 default 之前添加 case
//===----------------------------------------------------------------------===//

void MyCPUDAGToDAGISel::Select(SDNode *N) {
  SDLoc DL(N);

  // BRCOND: if (cond) goto dest → CMP cond, ZERO + BNZ dest
  if (N->getOpcode() == ISD::BRCOND) {
    SDValue Chain = N->getOperand(0);
    SDValue Cond  = N->getOperand(1);
    SDValue Dest  = N->getOperand(2);
    SDValue CmpOps[] = {Cond,
                        CurDAG->getRegister(MyCPU::ZERO, MVT::i32),
                        CurDAG->getTargetConstant(31, DL, MVT::i32),
                        CurDAG->getTargetConstant(0, DL, MVT::i32),
                        CurDAG->getTargetConstant(0, DL, MVT::i32),
                        Chain};
    SDNode *Cmp = CurDAG->getMachineNode(MyCPU::CMPW, DL, MVT::Other, CmpOps);
    SDValue BccOps[] = {CurDAG->getTargetConstant(0, DL, MVT::i32),
                        Dest, SDValue(Cmp, 0)};
    CurDAG->SelectNodeTo(N, MyCPU::BNZ, MVT::Other, BccOps);
    return;
  }

  switch (N->getOpcode()) {

  //===--- FrameIndex ---===//
  // ★ 帧索引 → TargetFrameIndex
  // 实际的基址+偏移计算推迟到 eliminateFrameIndex
  case ISD::FrameIndex: {
    int FI = cast<FrameIndexSDNode>(N)->getIndex();
    SDValue TFI = CurDAG->getTargetFrameIndex(FI, MVT::i32);
    ReplaceNode(N, TFI.getNode());
    return;
  }

  //===--- Wrapper ---===//
  // ★ 全局地址/块地址 → MOVIW 指令
  // 注意：32 位地址需要 MOVIW + ADDIW 两条指令(如果 offset != 0)
  // 当前简化为单条 MOVIW，完整实现需要：
  //   %reg = MOVIW  lo16  (或 MOVI 15位立即数)
  //   %reg = ADDIW  %reg, hi16_offset (后续可添加)
  case MyCPUISD::Wrapper: {
    SDValue Target = N->getOperand(0);
    SDValue Result;
    if (auto *GA = dyn_cast<GlobalAddressSDNode>(Target)) {
      Result = CurDAG->getTargetGlobalAddress(GA->getGlobal(), DL, MVT::i32,
                                              GA->getOffset());
    } else if (auto *BA = dyn_cast<BlockAddressSDNode>(Target)) {
      Result = CurDAG->getTargetBlockAddress(BA->getBlockAddress(), MVT::i32,
                                             0);
    }
    if (Result) {
      ReplaceNode(N, CurDAG->getMachineNode(MyCPU::MOVIW, DL, MVT::i32,
                                             Result));
    }
    return;
  }

  //===--- RetFlag ---===//
  // ★ 函数返回标记 → RET 指令
  case MyCPUISD::RetFlag: {
    ReplaceNode(N, CurDAG->getMachineNode(MyCPU::RET, DL, MVT::Other,
                                           N->getOperand(0)));
    return;
  }

  //===--- Call ---===//
  // ★ 函数调用 → CALL 指令
  //
  // 处理流程：Callee 可能是几种形式：
  //   1. Wrapper 已被选中 → MOVIW MachineSDNode，从中提取 TargetGlobalAddress
  //   2. Wrapper 未被选中 → 直接从 Wrapper 提取 inner target
  //   3. 直接是 TargetGlobalAddress 或 TargetExternalSymbol
  //   4. 寄存器值 → 使用 JALR 间接调用
  case MyCPUISD::Call: {
    SDValue Chain = N->getOperand(0);
    SDValue Callee = N->getOperand(1);
    SDNode *CNode = Callee.getNode();

    SDValue TargetOp;
    // 已被选中的 MOVIW → 提取内部目标地址
    if (CNode->isMachineOpcode()) {
      TargetOp = CNode->getOperand(0);
    }
    // 未被选中的 Wrapper → 提取内部目标地址
    else if (CNode->getOpcode() == MyCPUISD::Wrapper) {
      TargetOp = CNode->getOperand(0);
    }
    // 直接的全局地址或外部符号
    else if (CNode->getOpcode() == ISD::TargetGlobalAddress ||
             CNode->getOpcode() == ISD::TargetExternalSymbol) {
      TargetOp = Callee;
    }

    if (TargetOp.getNode()) {
      ReplaceNode(N, CurDAG->getMachineNode(MyCPU::CALL, DL, MVT::Other,
                                            MVT::Glue, TargetOp, Chain));
      return;
    }

    // Indirect or unsupported callee — fall through to SelectCode
    break;
  }

  //===--- Constant ---===//
  // ★ 常量 → MOVIW (如果 fit 15-bit signed)
  // i16/i8 常量：MOVIW 加载后用 MOVABH/MOVABB 截断
  // 大常量由 LLVM 展开为多指令序列
  case ISD::Constant: {
    ConstantSDNode *C = cast<ConstantSDNode>(N);
    int64_t Val = C->getSExtValue();
    EVT VT = N->getValueType(0);

    // i16 constant: materialize as i32 then truncate
    if (VT == MVT::i16) {
      if (isInt<15>(Val)) {
        SDNode *Mov = CurDAG->getMachineNode(MyCPU::MOVIW, DL, MVT::i32,
                        CurDAG->getTargetConstant(Val, DL, MVT::i32));
        CurDAG->SelectNodeTo(N, MyCPU::MOVABH, VT, SDValue(Mov, 0));
        return;
      }
      // For i16 constants > 15 bits: MOVIW high_byte, SHLIW 8, ORIW low_byte
      uint64_t UV = C->getZExtValue();
      int64_t Hi = (UV >> 8) & 0xFF;
      int64_t Lo = UV & 0xFF;
      if (isInt<15>(Hi)) {
        SDNode *HiMov = CurDAG->getMachineNode(MyCPU::MOVIW, DL, MVT::i32,
                          CurDAG->getTargetConstant(Hi, DL, MVT::i32));
        SDValue ShOps[] = {SDValue(HiMov, 0),
                           CurDAG->getTargetConstant(8, DL, MVT::i32)};
        SDNode *Sh = CurDAG->getMachineNode(MyCPU::SHLIW, DL, MVT::i32, ShOps);
        if (Lo != 0 && isInt<15>(Lo)) {
          SDValue OrOps[] = {SDValue(Sh, 0),
                             CurDAG->getTargetConstant(Lo, DL, MVT::i32)};
          SDNode *Or = CurDAG->getMachineNode(MyCPU::ORIW, DL, MVT::i32, OrOps);
          CurDAG->SelectNodeTo(N, MyCPU::MOVABH, MVT::i16, SDValue(Or, 0));
        } else {
          CurDAG->SelectNodeTo(N, MyCPU::MOVABH, MVT::i16, SDValue(Sh, 0));
        }
        return;
      }
      break;
    }

    // i8 constant: materialize as i32 then truncate
    if (VT == MVT::i8) {
      if (isInt<15>(Val)) {
        SDNode *Mov = CurDAG->getMachineNode(MyCPU::MOVIW, DL, MVT::i32,
                        CurDAG->getTargetConstant(Val, DL, MVT::i32));
        CurDAG->SelectNodeTo(N, MyCPU::MOVABB, VT, SDValue(Mov, 0));
        return;
      }
      break;
    }

    if (isInt<15>(Val)) {
      ReplaceNode(N, CurDAG->getMachineNode(
                         MyCPU::MOVIW, DL, MVT::i32,
                         CurDAG->getTargetConstant(Val, DL, MVT::i32)));
      return;
    }
    // ★ 大常量：不在此处处理，留给 Expand 展开为多指令序列
    break;
  }

  //===--- ADD/SUB — 根据值类型选择 Word/Half/Byte 指令 ---===//
  case ISD::ADD: {
    SDValue LHS = N->getOperand(0);
    SDValue RHS = N->getOperand(1);
    EVT VT = N->getValueType(0);
    unsigned Opc;
    int64_t MSB, LSB;
    if (VT == MVT::i8)       { Opc = MyCPU::ADDB; MSB = 7;  LSB = 0; }
    else if (VT == MVT::i16) { Opc = MyCPU::ADDH; MSB = 15; LSB = 0; }
    else                     { Opc = MyCPU::ADDW; MSB = 31; LSB = 0; }
    SDValue Ops[] = {LHS, RHS,
                     CurDAG->getTargetConstant(MSB, DL, MVT::i32),
                     CurDAG->getTargetConstant(LSB, DL, MVT::i32)};
    CurDAG->SelectNodeTo(N, Opc, VT, Ops);
    return;
  }
  case ISD::SUB: {
    SDValue LHS = N->getOperand(0);
    SDValue RHS = N->getOperand(1);
    EVT VT = N->getValueType(0);
    unsigned Opc;
    int64_t MSB, LSB;
    if (VT == MVT::i8)       { Opc = MyCPU::SUBB; MSB = 7;  LSB = 0; }
    else if (VT == MVT::i16) { Opc = MyCPU::SUBH; MSB = 15; LSB = 0; }
    else                     { Opc = MyCPU::SUBW; MSB = 31; LSB = 0; }
    SDValue Ops[] = {LHS, RHS,
                     CurDAG->getTargetConstant(MSB, DL, MVT::i32),
                     CurDAG->getTargetConstant(LSB, DL, MVT::i32)};
    CurDAG->SelectNodeTo(N, Opc, VT, Ops);
    return;
  }

  //===--- BR (unconditional branch) → JMP ---===//
  case ISD::BR: {
    SDValue Chain  = N->getOperand(0);
    SDValue Target = N->getOperand(1);
    SDValue Ops[] = {Target, Chain};
    CurDAG->SelectNodeTo(N, MyCPU::JMP, MVT::Other, Ops);
    return;
  }

  //===--- LOAD with zext/anyext from i8/i16 → LDB/LDH ---===//
  case ISD::LOAD: {
    if (auto *LD = dyn_cast<LoadSDNode>(N)) {
      ISD::LoadExtType ExtType = LD->getExtensionType();
      if (ExtType != ISD::NON_EXTLOAD && ExtType != ISD::SEXTLOAD) {
        EVT MemVT = LD->getMemoryVT();
        if (MemVT == MVT::i8 || MemVT == MVT::i16) {
          unsigned Opc = (MemVT == MVT::i8) ? MyCPU::LDB : MyCPU::LDH;
          SDValue Chain = N->getOperand(0);
          SDValue Ptr   = N->getOperand(1);
          SDValue Base, Offset;
          // Extract base+offset from address: (add base, offset) or direct
          if (Ptr.getOpcode() == ISD::ADD &&
              isa<ConstantSDNode>(Ptr.getOperand(1))) {
            Base = Ptr.getOperand(0);
            Offset = CurDAG->getTargetConstant(
                cast<ConstantSDNode>(Ptr.getOperand(1))->getSExtValue(),
                DL, MVT::i32);
          } else {
            Base = Ptr;
            Offset = CurDAG->getTargetConstant(0, DL, MVT::i32);
          }
          SDValue Ops[] = {Base, Offset, Chain};
          CurDAG->SelectNodeTo(N, Opc, N->getValueType(0), Ops);
          return;
        }
      }
    }
    break;
  }

  //===--- STORE with trunc from i32 → i8/i16 ---===//
  case ISD::STORE: {
    if (auto *ST = dyn_cast<StoreSDNode>(N)) {
      if (ST->isTruncatingStore()) {
        EVT MemVT = ST->getMemoryVT();
        if (MemVT == MVT::i8 || MemVT == MVT::i16) {
          unsigned Opc = (MemVT == MVT::i8) ? MyCPU::STB : MyCPU::STH;
          SDValue Chain = N->getOperand(0);
          SDValue Val   = N->getOperand(1);  // value to store (pre-trunc)
          SDValue Ptr   = N->getOperand(2);  // address
          SDValue Base, Offset;
          if (Ptr.getOpcode() == ISD::ADD &&
              isa<ConstantSDNode>(Ptr.getOperand(1))) {
            Base = Ptr.getOperand(0);
            Offset = CurDAG->getTargetConstant(
                cast<ConstantSDNode>(Ptr.getOperand(1))->getSExtValue(),
                DL, MVT::i32);
          } else {
            Base = Ptr;
            Offset = CurDAG->getTargetConstant(0, DL, MVT::i32);
          }
          SDValue Ops[] = {Val, Base, Offset, Chain};
          CurDAG->SelectNodeTo(N, Opc, MVT::Other, Ops);
          return;
        }
      }
    }
    break;
  }

  //===--- SHL/SRL/SRA — dispatch Word/Half/Byte + reg/imm shift ---===//
  case ISD::SHL:
  case ISD::SRL:
  case ISD::SRA: {
    SDValue LHS = N->getOperand(0);
    SDValue RHS = N->getOperand(1);
    EVT VT = N->getValueType(0);
    unsigned Opc;
    bool isImm = isa<ConstantSDNode>(RHS);

    if (VT == MVT::i8) {
      if (N->getOpcode() == ISD::SHL) Opc = isImm ? MyCPU::SHLIB : MyCPU::SHLB;
      else if (N->getOpcode() == ISD::SRL) Opc = isImm ? MyCPU::SHRIB : MyCPU::SHRB;
      else Opc = isImm ? MyCPU::SARIB : MyCPU::SARB;
    } else if (VT == MVT::i16) {
      if (N->getOpcode() == ISD::SHL) Opc = isImm ? MyCPU::SHLIH : MyCPU::SHLH;
      else if (N->getOpcode() == ISD::SRL) Opc = isImm ? MyCPU::SHRIH : MyCPU::SHRH;
      else Opc = isImm ? MyCPU::SARIH : MyCPU::SARH;
    } else {
      if (N->getOpcode() == ISD::SHL) Opc = isImm ? MyCPU::SHLIW : MyCPU::SHLW;
      else if (N->getOpcode() == ISD::SRL) Opc = isImm ? MyCPU::SHRIW : MyCPU::SHRW;
      else Opc = isImm ? MyCPU::SARIW : MyCPU::SARW;
    }

    if (isImm) {
      int64_t ShAmt = cast<ConstantSDNode>(RHS)->getSExtValue();
      SDValue Ops[] = {LHS, CurDAG->getTargetConstant(ShAmt, DL, MVT::i32)};
      CurDAG->SelectNodeTo(N, Opc, VT, Ops);
    } else {
      SDValue Ops[] = {LHS, RHS};
      CurDAG->SelectNodeTo(N, Opc, VT, Ops);
    }
    return;
  }

  //===--- BR_CC (conditional branch) → CMP + BZW/BNZ ---===//
  // ★ 根据操作数类型选择 Word/Half/Byte 比较指令
  case ISD::BR_CC: {
    SDValue Chain = N->getOperand(0);
    ISD::CondCode CC = cast<CondCodeSDNode>(N->getOperand(1))->get();
    SDValue LHS   = N->getOperand(2);
    SDValue RHS   = N->getOperand(3);
    SDValue Dest  = N->getOperand(4);

    unsigned BccOpc;
    switch (CC) {
    case ISD::SETEQ:  BccOpc = MyCPU::BZW;  break;
    case ISD::SETNE:  BccOpc = MyCPU::BNZ;  break;
    default:          BccOpc = MyCPU::BNZ;  break;
    }

    // ★ 如果 RHS 是常量 0，用 ZERO 寄存器代替（CMPW 需要寄存器操作数）
    if (auto *C = dyn_cast<ConstantSDNode>(RHS)) {
      if (C->isZero())
        RHS = CurDAG->getRegister(MyCPU::ZERO, MVT::i32);
    }

    // ★ 根据被比较值的类型选择 CMP 指令
    EVT CmpVT = LHS.getValueType();
    unsigned CmpOpc;
    int64_t MSB, LSB;
    if (CmpVT == MVT::i8)       { CmpOpc = MyCPU::CMPB; MSB = 7;  LSB = 0; }
    else if (CmpVT == MVT::i16) { CmpOpc = MyCPU::CMPH; MSB = 15; LSB = 0; }
    else                        { CmpOpc = MyCPU::CMPW; MSB = 31; LSB = 0; }

    SDValue F0  = CurDAG->getTargetConstant(0, DL, MVT::i32);
    SDValue CmpOps[] = {LHS, RHS,
                        CurDAG->getTargetConstant(MSB, DL, MVT::i32),
                        CurDAG->getTargetConstant(LSB, DL, MVT::i32),
                        F0, Chain};
    SDNode *Cmp = CurDAG->getMachineNode(CmpOpc, DL, MVT::Other, CmpOps);

    SDValue BccOps[] = {F0, Dest, SDValue(Cmp, 0)};
    CurDAG->SelectNodeTo(N, BccOpc, MVT::Other, BccOps);
    return;
  }

  default:
    break;
  }

  // ★ 兜底：交由 TableGen 生成的 SelectCode() 匹配 Pat<> 模式
  SelectCode(N);
}
