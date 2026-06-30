//===-- MyCPURegisterInfo.cpp - MyCPU Register Information ----------------===//
//
// ★ 寄存器信息实现 — 寄存器分配和帧索引消除 ★
//
// 职责：
//   1. getReservedRegs()       — 声明哪些物理寄存器不能被分配器使用
//   2. eliminateFrameIndex()   — 将帧索引替换为寄存器+立即数偏移
//   3. getCalleeSavedRegs()    — 返回被调用者保存的寄存器列表
//   4. getCallPreservedMask()  — 返回跨调用保留的寄存器掩码
//
// ★ 添加新寄存器时的修改步骤：
//   1. 在 MyCPURegisterInfo.td 中定义新寄存器
//   2. 将其加入 GPR 或新建 RegisterClass
//   3. 如果需要预留(getReservedRegs)，在此处添加
//   4. 如果是 callee-saved，在 MyCPUCallingConv.td 的 CSR 中添加
//   5. 更新 MyCPUMCCodeEmitter.cpp 中的寄存器编码逻辑
//
//===----------------------------------------------------------------------===//

#include "MyCPURegisterInfo.h"
#include "MyCPU.h"
#include "MyCPUFrameLowering.h"
#include "MyCPUInstrInfo.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "mycpu-reginfo"

// ★ 包含 TableGen 生成的寄存器描述信息
#define GET_REGINFO_ENUM
#define GET_REGINFO_TARGET_DESC
#include "MyCPUGenRegisterInfo.inc"

// ★ 构造函数：LR 作为返回地址寄存器
// 这个参数用于 exception handling 和 debug info
MyCPURegisterInfo::MyCPURegisterInfo()
    : MyCPUGenRegisterInfo(MyCPU::LR) {}

//===----------------------------------------------------------------------===//
// getCalleeSavedRegs — 被调用者保存寄存器列表
//
// ★ 返回 CSR_MyCPU_SaveList (在 MyCPUCallingConv.td 中定义)
// 当前: S0-S7 (R16-R23), FP (R30)
// 线程:  函数入口时需将原值保存到栈，出口时恢复
//===----------------------------------------------------------------------===//

static const MCPhysReg CSR_MyCPU_SaveList[] = {
    MyCPU::S0, MyCPU::S1, MyCPU::S2, MyCPU::S3,
    MyCPU::S4, MyCPU::S5, MyCPU::S6, MyCPU::S7,
    MyCPU::FP, 0};

static const uint32_t CSR_MyCPU_RegMask[] = {0x1FE0002, 0x00000000};

const MCPhysReg *
MyCPURegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  return CSR_MyCPU_SaveList;
}

//===----------------------------------------------------------------------===//
// getCallPreservedMask — 跨调用保留的寄存器掩码
//
// ★ 告诉寄存器分配器哪些寄存器在函数调用后仍然有效
// 这影响调用者(caller)在调用前后如何保存寄存器
//===----------------------------------------------------------------------===//

const uint32_t *
MyCPURegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                         CallingConv::ID CC) const {
  return CSR_MyCPU_RegMask;
}

//===----------------------------------------------------------------------===//
// getReservedRegs — 预留寄存器
//
// ★ 这些寄存器永远不会被寄存器分配器使用
//
// 预留寄存器列表：
//   R0  (ZERO) — 硬连线零寄存器，写入被忽略
//   R29 (SP)   — 栈指针，由 SUBIW/ADDIW + STW/LDW 栈操作自动管理
//   R30 (FP)   — 帧指针，指向当前栈帧基准位置
//   R31 (GP)   — 全局指针，用于全局数据访问
//   R28 (LR)   — 链接寄存器，存储函数返回地址
//
// ★ 修改预留寄存器时注意：
//   - 减少预留 = 更多可用寄存器，但 SP/FP/ZERO 绝不能解除预留
//   - 增加预留 = 寄存器分配器可使用更少寄存器，可能增加 spill
//   - 使用 markSuperRegs() 确保任何超寄存器也被标记
//===----------------------------------------------------------------------===//

BitVector
MyCPURegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  markSuperRegs(Reserved, MyCPU::ZERO); // R0  — 硬连线零
  markSuperRegs(Reserved, MyCPU::SP);   // R29 — 栈指针
  markSuperRegs(Reserved, MyCPU::FP);   // R30 — 帧指针
  markSuperRegs(Reserved, MyCPU::GP);   // R31 — 全局指针
  markSuperRegs(Reserved, MyCPU::LR);   // R28 — 链接寄存器
  return Reserved;
}

//===----------------------------------------------------------------------===//
// eliminateFrameIndex — 帧索引消除
//
// ★ 这是寄存器分配后的关键 pass
//
// 在 ISel 阶段，栈访问被表示为 FrameIndex + 偏移。
// 此函数将 FrameIndex 替换为实际的物理寄存器(SP 或 FP) + 具体偏移量。
//
// 变换示例：
//   分配前: STW src, %stack.0, 0     (FrameIndex=0, Offset=0)
//   分配后: STW src, SP, -16         (FrameReg=SP, Offset=-16)
//
// ★ 如果修改了 LD/ST 指令的帧索引操作数位置(operand index)，
//   需要同步修改此函数中的 FIOperandNum 参数使用方式
//===----------------------------------------------------------------------===//

bool MyCPURegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator MI,
                                             int SPAdj, unsigned FIOperandNum,
                                             RegScavenger *RS) const {
  MachineInstr &MInst = *MI;
  MachineFunction &MF = *MI->getParent()->getParent();
  const MachineFrameInfo &MFI = MF.getFrameInfo();

  // ★ 获取 FrameIndex 和初始偏移
  int FrameIndex = MInst.getOperand(FIOperandNum).getIndex();
  int Offset = MFI.getObjectOffset(FrameIndex);

  // ★ 确定帧基址寄存器(SP 或 FP)和附加偏移
  Register FrameReg;
  StackOffset StackOff = getFrameLowering(MF)->getFrameIndexReference(
      MF, FrameIndex, FrameReg);

  // ★ 累计最终偏移 = 初始偏移 + 帧基址调整
  Offset += StackOff.getFixed();

  // ★ 替换操作数：FrameIndex → 寄存器, 占位立即数 → 实际偏移
  // LDW 的操作数结构: [rd=0, base=1(FIOperandNum), offset=2(FIOperandNum+1)]
  // STW 的操作数结构: [src=0, base=1(FIOperandNum), offset=2(FIOperandNum+1)]
  MInst.getOperand(FIOperandNum).ChangeToRegister(FrameReg, false);
  MInst.getOperand(FIOperandNum + 1).ChangeToImmediate(Offset);

  return false;
}

//===----------------------------------------------------------------------===//
// getFrameRegister — 返回帧指针寄存器
//
// ★ 用于调试信息和异常处理
//===----------------------------------------------------------------------===//

Register
MyCPURegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return MyCPU::FP;
}

const TargetRegisterClass *
MyCPURegisterInfo::getPointerRegClass(unsigned Kind) const {
  return &MyCPU::GPRRegClass;
}

Register MyCPURegisterInfo::getStackRegister() const {
  return MyCPU::SP;
}

Register MyCPURegisterInfo::getFramePointer() const {
  return MyCPU::FP;
}
