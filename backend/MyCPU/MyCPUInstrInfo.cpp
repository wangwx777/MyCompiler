//===-- MyCPUInstrInfo.cpp - MyCPU Instruction Information ----------------===//
//
// ★ 指令操作辅助类 — 提供目标特定的指令操作 ★
//
// 职责：
//   1. copyPhysReg()    — 物理寄存器间复制(MOV)
//   2. stack slot 操作  — 寄存器溢出到栈/从栈恢复
//   3. analyzeBranch()  — 分析基本块末尾的分支模式
//   4. insertBranch()   — 在基本块末尾插入分支
//   5. removeBranch()   — 移除基本块末尾的分支
//   6. reverseBranchCondition() — 反转分支条件
//
// ★ 如果需要修改分支指令(如添加新的条件分支类型)：
//   1. 在 analyzeBranch() 中识别新指令
//   2. 在 insertBranch() 中添加新指令的插入逻辑
//   3. 在 removeBranch() 中添加新指令的移除逻辑
//   4. 在 reverseBranchCondition() 中添加条件反转映射
//
//===----------------------------------------------------------------------===//

#include "MyCPUInstrInfo.h"
#include "MyCPU.h"
#include "MyCPUSubtarget.h"
#include "MCTargetDesc/MyCPUMCTargetDesc.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

using namespace llvm;

#define DEBUG_TYPE "mycpu-instrinfo"

#define GET_REGINFO_ENUM
#include "MyCPUGenRegisterInfo.inc"

// ★ 包含 TableGen 生成的枚举和构造函数
#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_CTOR_DTOR
#include "MyCPUGenInstrInfo.inc"

MyCPUInstrInfo::MyCPUInstrInfo(MyCPUSubtarget &STI)
    : MyCPUGenInstrInfo(static_cast<const TargetSubtargetInfo &>(STI), RegInfo,
                        MyCPU::ADJCALLSTACKUP, MyCPU::ADJCALLSTACKDOWN, 0,
                        MyCPU::RET),
      RegInfo() {}

//===----------------------------------------------------------------------===//
// copyPhysReg — 物理寄存器复制
//
// ★ 寄存器分配器在需要移动寄存器值时调用此函数
// 实现为 MOVW rd, rs 指令
//
// 如果后续需要支持特殊寄存器(如 Freg)间的复制：
//   添加 contains 检查和专用传送指令
//===----------------------------------------------------------------------===//

void MyCPUInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator MI,
                                  const DebugLoc &DL, Register DestReg,
                                  Register SrcReg, bool KillSrc,
                                  bool RenamableDest, bool RenamableSrc) const {
  // ★ GPR → GPR: 使用 MOVW 指令
  if (MyCPU::GPRRegClass.contains(DestReg, SrcReg)) {
    BuildMI(MBB, MI, DL, get(MyCPU::MOVABW), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }

  // ★ 如需 Freg → GPR 或 GPR → Freg 传送，在此添加
  // 例如: if (MyCPU::FREGRegClass.contains(SrcReg)) { ... }

  llvm_unreachable("Impossible reg-to-reg copy");
}

//===----------------------------------------------------------------------===//
// storeRegToStackSlot — 将寄存器保存到栈槽
//
// ★ 寄存器溢出(spill)时调用
// 实现为 STW src, SP, offset 指令
//
// 编码细节：
//   addFrameOperand() 注册了一个帧索引引用，
//   实际的偏移量在 eliminateFrameIndex() 阶段解析
//===----------------------------------------------------------------------===//

void MyCPUInstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register SrcReg,
    bool IsKill, int FrameIndex, const TargetRegisterClass *RC,
    Register VReg, MachineInstr::MIFlag Flags) const {
  BuildMI(MBB, MI, DebugLoc(), get(MyCPU::STW))
      .addReg(SrcReg, getKillRegState(IsKill))
      .addFrameIndex(FrameIndex)
      .addImm(0);
}

//===----------------------------------------------------------------------===//
// loadRegFromStackSlot — 从栈槽恢复寄存器
//
// ★ 寄存器重载(reload)时调用
// 实现为 LDW dst, SP, offset 指令
//===----------------------------------------------------------------------===//

void MyCPUInstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register DestReg,
    int FrameIndex, const TargetRegisterClass *RC,
    Register VReg, unsigned SubReg, MachineInstr::MIFlag Flags) const {
  BuildMI(MBB, MI, DebugLoc(), get(MyCPU::LDW), DestReg)
      .addFrameIndex(FrameIndex)
      .addImm(0);
}

//===----------------------------------------------------------------------===//
// analyzeBranch — 分析基本块末尾的分支模式
//
// ★ 用于识别基本块末尾的控制流结构
//
// 返回值 false = 分析成功，true = 无法分析
//
// MyCPU 的分支模式：
//   无条件跳转:  JMP target
//   条件跳转:    BZW Fx, true_target  ; fall-through → false_target
//                BNZ Fx, true_target  ; fall-through → false_target
//   条件+无条件: BZW Fx, true_target
//                JMP false_target
//
// 参数说明：
//   TBB — true 目标(MBB 为条件真时跳转的目标)
//   FBB — false 目标(MBB 为条件假时到达的目标)
//   Cond — 条件操作数列表 [opcode, Freg_index]
//
// ★ 如果添加了新的条件分支指令(如 BCS/BCC)：
//   在此函数中添加对应的识别逻辑
//===----------------------------------------------------------------------===//

bool MyCPUInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                    MachineBasicBlock *&TBB,
                                    MachineBasicBlock *&FBB,
                                    SmallVectorImpl<MachineOperand> &Cond,
                                    bool AllowModify) const {
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I == MBB.end())
    return false;

  // ★ 纯 JMP — 简单无条件跳转，BranchFolder 可以安全优化
  if (I->getOpcode() == MyCPU::JMP) {
    // 检查前面是否有 Bcc (条件+无条件复合分支)
    // 如果有，返回 true 避免 BranchFolder 处理这种复杂模式
    MachineBasicBlock::iterator Prev = I;
    while (Prev != MBB.begin()) {
      --Prev;
      if (Prev->isDebugInstr())
        continue;
      if (Prev->getOpcode() == MyCPU::BZW ||
          Prev->getOpcode() == MyCPU::BNZ)
        return true; // 复合分支 → 不可分析，避免 BranchFolder 崩溃
      break;
    }
    TBB = I->getOperand(0).getMBB();
    return false;
  }

  // ★ BZW/BNZ — 条件分支一律返回 true，暂不让 BranchFolder 优化
  if (I->getOpcode() == MyCPU::BZW || I->getOpcode() == MyCPU::BNZ)
    return true;

  // RET 等其他终止符
  return true;
}

//===----------------------------------------------------------------------===//
// insertBranch — 在基本块末尾插入分支指令
//
// ★ 用于创建/修改基本块的控制流
//
// Cond 为空 → 无条件跳转(JMP)
// Cond 非空 → 条件跳转(BZW/BNZ) + 可选 JMP
//===----------------------------------------------------------------------===//

unsigned MyCPUInstrInfo::insertBranch(
    MachineBasicBlock &MBB, MachineBasicBlock *TBB, MachineBasicBlock *FBB,
    ArrayRef<MachineOperand> Cond, const DebugLoc &DL,
    int *BytesAdded) const {
  // ★ 无条件跳转
  if (Cond.empty()) {
    BuildMI(&MBB, DL, get(MyCPU::JMP)).addMBB(TBB);
    return 1;
  }

  // ★ 条件跳转：需重建 CMPW + Bcc
  // Cond 格式: [BccOpc, F0, LHS, RHS, MSB, LSB]
  // 如果 Cond 包含 CMPW 操作数(size >= 6)，先发射 CMPW
  unsigned Opc = Cond[0].getImm();
  if (Cond.size() >= 6) {
    BuildMI(&MBB, DL, get(MyCPU::CMPW))
        .add(Cond[2])  // LHS
        .add(Cond[3])  // RHS
        .add(Cond[4])  // MSB
        .add(Cond[5])  // LSB
        .add(Cond[1]); // F0
  }
  BuildMI(&MBB, DL, get(Opc)).add(Cond[1]).addMBB(TBB);

  // ★ 如果有 false 目标，追加 JMP
  if (FBB) {
    BuildMI(&MBB, DL, get(MyCPU::JMP)).addMBB(FBB);
    return Cond.size() >= 6 ? 3 : 2;
  }
  return Cond.size() >= 6 ? 2 : 1;
}

//===----------------------------------------------------------------------===//
// removeBranch — 移除基本块末尾的分支指令
//
// ★ 从后向前遍历指令，移除所有分支指令(JMP/BZW/BNZ)
// 在修改基本块控制流前调用
//===----------------------------------------------------------------------===//

unsigned MyCPUInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                       int *BytesRemoved) const {
  MachineBasicBlock::iterator I = MBB.end();
  unsigned Count = 0;
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    unsigned Opc = I->getOpcode();
    // ★ CMPW 作为条件分支的前置指令也需要一起移除
    // 否则 BranchFolder 移除 Bcc 后留下孤立的 CMPW(非 Terminator) → CFG 崩溃
    if (Opc != MyCPU::JMP && Opc != MyCPU::BZW && Opc != MyCPU::BNZ &&
        Opc != MyCPU::CMPW)
      break;
    I->eraseFromParent();
    Count++;
  }
  return Count;
}

//===----------------------------------------------------------------------===//
// reverseBranchCondition — 反转分支条件
//
// ★ BZW ↔ BNZ 互换
//
// 用于优化器反转条件分支，当添加新的条件分支指令时，
// 需添加对应的反转映射
//===----------------------------------------------------------------------===//

bool MyCPUInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  unsigned Opc = Cond[0].getImm();
  if (Opc == MyCPU::BZW) {
    Cond[0].setImm(MyCPU::BNZ);
    return false;
  }
  if (Opc == MyCPU::BNZ) {
    Cond[0].setImm(MyCPU::BZW);
    return false;
  }
  return true; // 无法反转
}

//===----------------------------------------------------------------------===//
// isSchedulingBoundary — 调度边界检查
//
// ★ CALL 和 RET 不能跨指令调度，必须作为基本块边界
//===----------------------------------------------------------------------===//

bool MyCPUInstrInfo::isSchedulingBoundary(const MachineInstr &MI,
                                           const MachineBasicBlock *MBB,
                                           const MachineFunction &MF) const {
  if (MI.isCall() || MI.isReturn())
    return true;
  return false;
}

//===----------------------------------------------------------------------===//
// getGlobalBaseReg — 获取全局基址寄存器
//
// ★ 返回 GP (R31)，用于 PIC 代码的全局数据访问
//===----------------------------------------------------------------------===//

Register MyCPUInstrInfo::getGlobalBaseReg(MachineFunction *MF) const {
  return MyCPU::GP;
}
