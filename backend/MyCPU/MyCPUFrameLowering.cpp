//===-- MyCPUFrameLowering.cpp - MyCPU Frame Lowering ---------------------===//
//
// ★ 栈帧管理 — Prologue / Epilogue / 栈槽分配 ★
//
// 职责：
//   1. emitPrologue() — 函数入口，设置栈帧
//   2. emitEpilogue() — 函数出口，恢复栈帧
//   3. getFrameIndexReference() — 计算帧索引相对 FP/SP 的偏移
//   4. spillCalleeSavedRegisters() / restoreCalleeSavedRegisters()
//
// ★ 标准栈帧布局 (栈向下增长，低地址在顶部)：
//
//   高地址 (调用前 SP)
//   +----------------+
//   | 调用者栈帧     |
//   +----------------+
//   | 返回地址 (LR)  |  ← 仅当函数有调用时保存
//   +----------------+
//   | 旧 FP 值       |  ← 仅当 hasFP() 为 true 时保存
//   +----------------+  ← FP 指向此处 (或 SP 如果没有 FP)
//   | 局部变量       |
//   | ...            |
//   +----------------+  ← 当前 SP
//   低地址
//
// ★ 修改 Prologue/Epilogue 序列时的注意事项：
//   - 栈操作通过 SUBIW/ADDIW + STW/LDW 组合实现 (无专用 PUSH/POP 指令)
//   - FP 的保存和恢复顺序必须对称
//   - 如果 adding 新的 callee-saved 寄存器，同步修改
//     MyCPUCallingConv.td 中的 CSR_MyCPU_SaveList
//
//===----------------------------------------------------------------------===//

#include "MyCPUFrameLowering.h"
#include "MyCPUInstrInfo.h"
#include "MyCPUMachineFunctionInfo.h"
#include "MyCPUSubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

using namespace llvm;

#define DEBUG_TYPE "mycpu-frame"

#define GET_REGINFO_ENUM
#include "MyCPUGenRegisterInfo.inc"
#define GET_INSTRINFO_ENUM
#include "MyCPUGenInstrInfo.inc"

// ★ 构造函数参数：
//   StackGrowsDown — 栈向低地址增长
//   Align(4)       — 栈帧 4 字节对齐
//   0              — 局部变量区偏移(对 SP 无额外偏移)
//   Align(4)       — 透明堆栈(用于异常处理)对齐
MyCPUFrameLowering::MyCPUFrameLowering(const MyCPUSubtarget &STI)
    : TargetFrameLowering(TargetFrameLowering::StackGrowsDown, Align(4), 0,
                          Align(4)) {}

//===----------------------------------------------------------------------===//
// emitPrologue — 函数入口代码生成
//
// ★ 插入以下序列(按需)：
//   1. SUBIW SP, SP, 4; STW LR, [SP + 0] — 如果函数内有调用则保存返回地址
//   2. SUBIW SP, SP, 4; STW FP, [SP + 0] — 如果需要帧指针则保存旧 FP
//   3. MOVW FP, SP    — 建立新的帧指针
//   4. SUBIW SP, size — 分配局部变量空间
//
// ★ 栈帧大小限制：
//   - SUBIW 的立即数范围为 15 位有符号(-16384..16383)
//   - 如果超过了 32767，需要使用多指令序列(通过 MOVIW + SUBW)
//===----------------------------------------------------------------------===//

void MyCPUFrameLowering::emitPrologue(MachineFunction &MF,
                                       MachineBasicBlock &MBB) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const MyCPURegisterInfo *RegInfo = static_cast<const MyCPURegisterInfo *>(
      MF.getSubtarget().getRegisterInfo());
  const MyCPUInstrInfo *TII = static_cast<const MyCPUInstrInfo *>(
      MF.getSubtarget().getInstrInfo());
  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();

  bool HasFP = hasFP(MF);

  // ★ 步骤 1: 如果该函数有子调用，保存返回地址 LR
  // SUBIW SP, SP, 4; STW LR, [SP + 0]
  if (MFI.hasCalls()) {
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::SUBIW), MyCPU::SP)
        .addReg(MyCPU::SP)
        .addImm(4);
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::STW))
        .addReg(MyCPU::LR)
        .addReg(MyCPU::SP)
        .addImm(0);
  }

  // ★ 步骤 2: 如果需要帧指针，保存旧 FP 并建立新 FP
  // SUBIW SP, SP, 4; STW FP, [SP + 0]
  // MOVW FP, SP → FP = SP (此时 SP 已减去了保存的量)
  //
  // 这样 FP 指向栈上的 saved-FP 位置，局部变量通过 FP 负偏移访问
  if (HasFP) {
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::SUBIW), MyCPU::SP)
        .addReg(MyCPU::SP)
        .addImm(4);
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::STW))
        .addReg(MyCPU::FP)
        .addReg(MyCPU::SP)
        .addImm(0);
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::MOVABW), MyCPU::FP)
        .addReg(MyCPU::SP);
  }

  // ★ 步骤 3: 分配局部变量空间
  // SUBIW SP, SP, StackSize → SP = SP - StackSize
  uint64_t StackSize = MFI.getStackSize();
  if (StackSize > 0) {
    if (StackSize <= 32767) {
      // ★ 小栈帧：单条 SUBIW 指令即可
      BuildMI(MBB, MBBI, DL, TII->get(MyCPU::SUBIW), MyCPU::SP)
          .addReg(MyCPU::SP)
          .addImm(StackSize);
    } else {
      // ★ 大栈帧：需要多指令序列
      //   MOVIW T0, StackSize_lo
      //   SUBW SP, SP, T0
      // TODO: 实现大栈帧支持
      llvm_unreachable("Large stack frames not yet supported");
    }
  }
}

//===----------------------------------------------------------------------===//
// emitEpilogue — 函数出口代码生成
//
// ★ 与 Prologue 对称逆序操作：
//   1. ADDIW SP, SP, size — 回收局部变量空间
//   2. LDW FP, [SP + 0]; ADDIW SP, SP, 4 — 恢复旧帧指针
//   3. LDW LR, [SP + 0]; ADDIW SP, SP, 4 — 恢复返回地址
//   后面紧跟 RET 指令(由 LowerReturn 生成)
//===----------------------------------------------------------------------===//

void MyCPUFrameLowering::emitEpilogue(MachineFunction &MF,
                                       MachineBasicBlock &MBB) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const MyCPUInstrInfo *TII = static_cast<const MyCPUInstrInfo *>(
      MF.getSubtarget().getInstrInfo());

  // ★ 在分支指令前插入 epilogue (通常是 RET 之前)
  MachineBasicBlock::iterator MBBI = MBB.getFirstTerminator();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();

  // ★ 步骤 1: 回收栈空间
  // ADDIW SP, SP, StackSize → SP = SP + StackSize
  uint64_t StackSize = MFI.getStackSize();
  if (StackSize > 0 && StackSize <= 32767) {
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::ADDIW), MyCPU::SP)
        .addReg(MyCPU::SP)
        .addImm(StackSize);
  }

  bool HasFP = hasFP(MF);

  // ★ 步骤 2: 恢复 FP
  // LDW FP, [SP + 0]; ADDIW SP, SP, 4
  if (HasFP) {
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::LDW), MyCPU::FP)
        .addReg(MyCPU::SP)
        .addImm(0);
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::ADDIW), MyCPU::SP)
        .addReg(MyCPU::SP)
        .addImm(4);
  }

  // ★ 步骤 3: 恢复 LR
  // LDW LR, [SP + 0]; ADDIW SP, SP, 4
  if (MFI.hasCalls()) {
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::LDW), MyCPU::LR)
        .addReg(MyCPU::SP)
        .addImm(0);
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::ADDIW), MyCPU::SP)
        .addReg(MyCPU::SP)
        .addImm(4);
  }
}

//===----------------------------------------------------------------------===//
// getFrameIndexReference — 计算帧对象相对于帧基址的偏移
//
// ★ 返回值用于在 eliminateFrameIndex 中将 FrameIndex 替换为
//   FrameReg + Offset
//
// 如果有 FP: FrameReg = FP, Offset = 对象偏移(负值，FP 之上)
// 如果无 FP: FrameReg = SP, Offset = 对象偏移(负值或正值)
//===----------------------------------------------------------------------===//

StackOffset
MyCPUFrameLowering::getFrameIndexReference(const MachineFunction &MF, int FI,
                                            Register &FrameReg) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  int Offset = MFI.getObjectOffset(FI);

  if (hasFP(MF)) {
    // ★ FP 相对寻址
    // FP 指向保存的旧 FP 位置
    // 局部变量在 FP 下方(更负的偏移)
    FrameReg = MyCPU::FP;
    return StackOffset::getFixed(Offset);
  } else {
    // ★ SP 相对寻址
    FrameReg = MyCPU::SP;
    return StackOffset::getFixed(Offset);
  }
}

//===----------------------------------------------------------------------===//
// determineCalleeSaves — 确定需要被调用者保存的寄存器
//
// ★ 在寄存器分配前调用，决定哪些寄存器需要 spill/restore
// 除调用约定中的 callee-saved 寄存器外，还强制保存 FP 和 LR
//===----------------------------------------------------------------------===//

void MyCPUFrameLowering::determineCalleeSaves(MachineFunction &MF,
                                               BitVector &SavedRegs,
                                               RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);

  // ★ 如果有帧指针则保存 FP
  if (hasFP(MF))
    SavedRegs.set(MyCPU::FP);

  // ★ 如果有子调用则保存 LR (返回地址会被 CALL 覆盖)
  if (MF.getFrameInfo().hasCalls())
    SavedRegs.set(MyCPU::LR);
}

//===----------------------------------------------------------------------===//
// hasFP — 判断函数是否需要帧指针
//
// ★ 需要帧指针的情况：
//   - 函数地址被取(&func)
//   - 函数有可变大小对象(alloca/VLA)
//   - 函数有 stack map 或 patch point
//
// 修改此函数可以控制 FP 的使用策略：
//   返回 true → 始终使用 FP (调试友好，但多一条指令)
//   返回 false → 尽可能省略 FP (更快，但调试困难)
//===----------------------------------------------------------------------===//

bool MyCPUFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return MFI.isFrameAddressTaken() || MFI.hasVarSizedObjects() ||
         MFI.hasStackMap() || MFI.hasPatchPoint();
}

//===----------------------------------------------------------------------===//
// hasReservedCallFrame
//
// ★ 返回 false 表示调用帧不是"预分配"的—每次 call 后需要
//   显式回收参数栈空间，而不是在 epilogue 一次性回收
//===----------------------------------------------------------------------===//

bool MyCPUFrameLowering::hasReservedCallFrame(
    const MachineFunction &MF) const {
  return false;
}

void MyCPUFrameLowering::processFunctionBeforeFrameFinalized(
    MachineFunction &MF, RegScavenger *RS) const {
  // Nothing specific needed
}

//===----------------------------------------------------------------------===//
// spillCalleeSavedRegisters — 保存被调用者寄存器到栈
//
// ★ 在 prologue 中调用，将 CSR 列表中的寄存器保存到分配的栈槽
// 保存顺序：按 CSI 数组顺序逐个调用 storeRegToStackSlot
//===----------------------------------------------------------------------===//

bool MyCPUFrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  const MyCPUInstrInfo *TII = static_cast<const MyCPUInstrInfo *>(
      MBB.getParent()->getSubtarget().getInstrInfo());

  for (const auto &CS : CSI) {
    unsigned Reg = CS.getReg();
    int FI = CS.getFrameIdx();
    TII->storeRegToStackSlot(MBB, MI, Reg, true, FI,
                             TRI->getMinimalPhysRegClass(Reg), Register());
  }
  return true;
}

//===----------------------------------------------------------------------===//
// restoreCalleeSavedRegisters — 从栈恢复被调用者寄存器
//
// ★ 在 epilogue 前调用，与 spillCalleeSavedRegisters 对称
//===----------------------------------------------------------------------===//

bool MyCPUFrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  const MyCPUInstrInfo *TII = static_cast<const MyCPUInstrInfo *>(
      MBB.getParent()->getSubtarget().getInstrInfo());

  for (const auto &CS : CSI) {
    unsigned Reg = CS.getReg();
    int FI = CS.getFrameIdx();
    TII->loadRegFromStackSlot(MBB, MI, Reg, FI,
                              TRI->getMinimalPhysRegClass(Reg), Register());
  }
  return true;
}
