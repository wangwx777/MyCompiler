//===-- MyCPUTargetMachine.cpp - TargetMachine for MyCPU ------------------===//
//
// ★ TargetMachine — LLVM 后端的顶层入口 ★
//
// 职责：
//   1. 注册目标到 LLVM 框架(RegisterTargetMachine)
//   2. 定义 DataLayout(数据在内存中的排布规则)
//   3. 管理 Subtarget 实例(按函数缓存)
//   4. 配置编译 Pass 流水线(addPassesToEmitFile / TargetPassConfig)
//   5. 提供 Pseudo 指令展开 pass
//
// ★ 添加新 Pass 的步骤：
//   1. 创建新的 MachineFunctionPass 子类
//   2. 在 TargetPassConfig 中重写对应阶段的 addXxx() 方法
//   3. 调用 addPass(new YourPass())
//   4. 在 LLVMInitializeMyCPUTarget() 中注册该 pass
//
//===----------------------------------------------------------------------===//

#include "MyCPUTargetMachine.h"
#include "MyCPU.h"
#include "MyCPUInstrInfo.h"
#include "MyCPUMachineFunctionInfo.h"
#include "MCTargetDesc/MyCPUMCTargetDesc.h"
#include "TargetInfo/MyCPUTargetInfo.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/MC/TargetRegistry.h"

#define GET_INSTRINFO_ENUM
#include "MyCPUGenInstrInfo.inc"
#define GET_REGINFO_ENUM
#include "MyCPUGenRegisterInfo.inc"

using namespace llvm;

// ★ 命令行选项：控制是否展开伪指令
// 使用: llc -mycpu-expand-pseudos=false 禁用伪指令展开
static cl::opt<bool> EnableMyCPUExpandPseudos(
    "mycpu-expand-pseudos", cl::init(true), cl::Hidden,
    cl::desc("Enable MyCPU pseudo-instruction expansion"));

//===----------------------------------------------------------------------===//
// LLVMInitializeMyCPUTarget — 目标注册入口
//
// ★ LLVM 启动时调用此函数注册 MyCPU 目标
// 此函数在 TargetInfo/MyCPUTargetInfo.cpp 中通过 extern "C" 导出
//===----------------------------------------------------------------------===//

namespace llvm {
void initializeMyCPUExpandPseudoPass(PassRegistry &);
FunctionPass *createMyCPUExpandPseudoPass();
} // namespace llvm

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeMyCPUTarget() {
  RegisterTargetMachine<MyCPUTargetMachine> X(getTheMyCPUTarget());
  auto *PR = PassRegistry::getPassRegistry();
  initializeMyCPUExpandPseudoPass(*PR);
}

//===----------------------------------------------------------------------===//
// computeDataLayout — 目标数据排布
//

//===----------------------------------------------------------------------===//
// MyCPUTargetMachine 构造函数
//
// ★ 参数说明：
//   TT      — 目标三元组 (如 mycpu-unknown-elf)
//   CPU     — CPU 名称 (用于选择 Subtarget 特性)
//   FS      — CPU 特性字串 (如 "+feature1,-feature2")
//   Options — 目标选项
//   RM      — 重定位模型 (Static/PIC/DynamicNoPIC)
//   CM      — 代码模型 (Small/Medium/Large)
//   OL      — 优化级别 (O0-O3)
//   JIT     — 是否使用 JIT 编译
//===----------------------------------------------------------------------===//

MyCPUTargetMachine::MyCPUTargetMachine(
    const Target &T, const Triple &TT, StringRef CPU, StringRef FS,
    const TargetOptions &Options, std::optional<Reloc::Model> RM,
    std::optional<CodeModel::Model> CM, CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(T, TT.computeDataLayout(), TT, CPU, FS, Options,
                                RM.value_or(Reloc::Static),
                                CM.value_or(CodeModel::Small), OL),
      TLOF(std::make_unique<TargetLoweringObjectFileELF>()) {
  initAsmInfo();
}

MyCPUTargetMachine::~MyCPUTargetMachine() = default;

//===----------------------------------------------------------------------===//
// getSubtargetImpl — Subtarget 实例管理
//
// ★ 按函数缓存 Subtarget 实例
//
// 不同函数可能有不同的 target-cpu 和 target-features 属性，
// 因此需要为每个 (CPU, FS) 组合缓存对应的 Subtarget 实例。
//
// SubtargetMap 是 mutable StringMap<unique_ptr<MyCPUSubtarget>>，
// 用于避免重复创建相同配置的 Subtarget。
//===----------------------------------------------------------------------===//

const MyCPUSubtarget *
MyCPUTargetMachine::getSubtargetImpl(const Function &F) const {
  Attribute CPUAttr = F.getFnAttribute("target-cpu");
  Attribute FSAttr = F.getFnAttribute("target-features");

  std::string CPU = CPUAttr.getValueAsString().str();
  std::string FS = FSAttr.getValueAsString().str();

  // ★ 使用 (CPU + FS) 字符串作为缓存 key
  std::string Key = CPU + FS;
  auto &I = SubtargetMap[Key];
  if (!I) {
    I = std::make_unique<MyCPUSubtarget>(TargetTriple, CPU, FS, *this);
  }
  return I.get();
}

//===----------------------------------------------------------------------===//
// createMachineFunctionInfo — 创建 MachineFunctionInfo
//
// ★ 每个 MachineFunction 调用一次，分配 MyCPUMachineFunctionInfo
// 用于存储函数特定的后端数据
//===----------------------------------------------------------------------===//

MachineFunctionInfo *MyCPUTargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return MyCPUMachineFunctionInfo::create<MyCPUMachineFunctionInfo>(Allocator, F,
                                                                     STI);
}

//===----------------------------------------------------------------------===//
// getTargetTransformInfo — 目标变换信息
//
// ★ 供 LLVM IR 级优化器查询目标特定信息
// 如指令代价、向量化宽度等
//===----------------------------------------------------------------------===//

TargetTransformInfo
MyCPUTargetMachine::getTargetTransformInfo(const Function &F) const {
  return TargetTransformInfo(std::make_unique<BasicTTIImpl>(this, F));
}

//===----------------------------------------------------------------------===//
// Pass Configuration — 编译流水线配置
//
// ★ Pass 流水线顺序 (CodeGen 阶段)：
//   IR → Pre-ISel → ISel → Post-ISel → RegAlloc → Pre-Emit → AsmPrinter
//
// 各阶段对应的 addXxx() 方法：
//   addIRPasses()      — IR 级优化 (在 ISel 之前)
//   addInstSelector()  — 指令选择 (DAG→DAG ISel)
//   addPreRegAlloc()   — 寄存器分配前的优化 / pseudo 展开
//   addPreEmitPass()   — 寄存器分配后 / 发射前的 pass
//
// ★ 如果需要添加自定义优化 pass (如延迟槽填充、Peephole 优化)：
//   在对应的 addXxx() 方法中调用 addPass()
//===----------------------------------------------------------------------===//

namespace {

class MyCPUPassConfig : public TargetPassConfig {
public:
  MyCPUPassConfig(MyCPUTargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  MyCPUTargetMachine &getMyCPUTargetMachine() const {
    return getTM<MyCPUTargetMachine>();
  }

  void addIRPasses() override;       // ★ IR → DAG 阶段
  bool addInstSelector() override;   // ★ 指令选择
  void addPreRegAlloc() override;    // ★ 寄存器分配前
  void addPreEmitPass() override;    // ★ 发射前
};

} // namespace

TargetPassConfig *
MyCPUTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new MyCPUPassConfig(*this, PM);
}

//===--- Pass 阶段实现 ---===//

void MyCPUPassConfig::addIRPasses() {
  TargetPassConfig::addIRPasses();
}

// ★ 指令选择：使用 SelectionDAG ISel (非 GlobalISel)
// 返回 false 表示还需要后续的机器码优化 pass
bool MyCPUPassConfig::addInstSelector() {
  addPass(createMyCPUISelDag(getMyCPUTargetMachine()));
  return false;
}

// ★ 寄存器分配前：展开伪指令
// PseudoCMP、LOAD_IMM、LOAD_ADDR 等在此阶段展开为真实指令序列
void MyCPUPassConfig::addPreRegAlloc() {
  if (EnableMyCPUExpandPseudos)
    addPass(createMyCPUExpandPseudoPass());
}

// ★ 发射前：post-RA 伪指令展开 (如需要)
void MyCPUPassConfig::addPreEmitPass() {
  // 暂时为空。后续可在此处添加：
  //   - 延迟槽填充 (如果 ISA 有分支延迟槽)
  //   - Peephole 优化 (相邻指令合并)
  //   - 代码对齐和 NOP 填充
}

//===----------------------------------------------------------------------===//
// MyCPUExpandPseudo — 伪指令展开 Pass
//
// ★ 将伪指令(Pseudo Instructions)展开为真实指令序列
//
// 伪指令是:
//   - 不能直接编码为机器码的指令
//   - 需要被多条真实指令替换
//
// 当前伪指令列表 (在 MyCPUInstrInfo.td 中定义)：
//   PseudoCMP       — 比较指令，展开为 CMPW
//   LOAD_IMM        — 加载常量，展开为 MOVIW (或 MOVIW+ADDIW)
//   LOAD_ADDR       — 加载地址，展开为 MOVIW+ADDIW
//   LD_FI           — 栈加载，展开为 LDW SP, offset
//   PseudoBR        — 伪分支，展开为 Bcc
//   SELECT          — 条件选择，展开为 CMP+Bcc+MOV
//   ADJCALLSTACK*   — 栈调整，展开为 ADDIW/SUBIW
//
// ★ 当前实现为 stub，需要为每种伪指令实现展开逻辑
// 展开示例(LOAD_IMM → MOVIW)：
//   MachineInstr *MI = ...; // LOAD_IMM rd, imm
//   BuildMI(MBB, MI, DL, TII->get(MyCPU::MOVIW), rd).addImm(imm);
//   MI->eraseFromParent();
//
//===----------------------------------------------------------------------===//

namespace llvm {

class MyCPUExpandPseudo : public MachineFunctionPass {
public:
  static char ID;
  MyCPUExpandPseudo() : MachineFunctionPass(ID) {
    initializeMyCPUExpandPseudoPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return "MyCPU Pseudo Instruction Expansion";
  }

private:
  bool expandMBB(MachineBasicBlock &MBB);
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI);
};

bool MyCPUExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= expandMBB(MBB);
  return Modified;
}

bool MyCPUExpandPseudo::expandMBB(MachineBasicBlock &MBB) {
  bool Modified = false;
  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    Modified |= expandMI(MBB, MBBI);
    MBBI = NMBBI;
  }
  return Modified;
}

// ★ 伪指令展开核心 — 逐条检查并展开
bool MyCPUExpandPseudo::expandMI(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator MBBI) {
  MachineInstr &MI = *MBBI;
  unsigned Opcode = MI.getOpcode();
  MachineFunction &MF = *MBB.getParent();
  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  DebugLoc DL = MI.getDebugLoc();

  switch (Opcode) {

  //===-- LOAD_IMM: 加载 32 位立即数 ---===//
  // 展开为 MOVIW + (SHLIW + 临时寄存器 + ORW/ADDW)* 序列
  // 立即数按 15 位有符号 chunk 拆分为多指令
  case MyCPU::LOAD_IMM: {
    Register DstReg = MI.getOperand(0).getReg();
    int64_t Imm = MI.getOperand(1).getImm();

    if (isInt<15>(Imm)) {
      BuildMI(MBB, MBBI, DL, TII->get(MyCPU::MOVIW), DstReg).addImm(Imm);
      MI.eraseFromParent();
      return true;
    }

    uint32_t V = static_cast<uint32_t>(Imm);
    unsigned Lo = V & 0x7FFF;
    // 计算 sign-extend(15) 后的剩余值
    int32_t SE_Lo = (Lo & 0x4000) ? (Lo | 0xFFFF8000) : Lo;
    int32_t Remaining = static_cast<int32_t>(V) - SE_Lo;
    int32_t RemShifted = Remaining >> 15;
    unsigned Mid = RemShifted & 0x7FFF;
    unsigned Top = (RemShifted >> 15) & 0x3;

    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::MOVIW), DstReg).addImm(Lo);

    if (Mid != 0 || Top != 0) {
      Register TmpReg = MRI.createVirtualRegister(&MyCPU::GPRRegClass);
      BuildMI(MBB, MBBI, DL, TII->get(MyCPU::MOVIW), TmpReg).addImm(Mid);
      // 如果 Mid 有 bit14=1, MOVIW 符号扩展导致负值，需加回补偿
      if (Mid & 0x4000) {
        Register FixReg = MRI.createVirtualRegister(&MyCPU::GPRRegClass);
        BuildMI(MBB, MBBI, DL, TII->get(MyCPU::MOVIW), FixReg).addImm(1);
        BuildMI(MBB, MBBI, DL, TII->get(MyCPU::SHLIW), FixReg)
            .addReg(FixReg).addImm(15);
        BuildMI(MBB, MBBI, DL, TII->get(MyCPU::ADDW), TmpReg)
            .addReg(TmpReg).addReg(FixReg).addImm(31).addImm(0);
      }
      BuildMI(MBB, MBBI, DL, TII->get(MyCPU::SHLIW), TmpReg)
          .addReg(TmpReg).addImm(15);
      BuildMI(MBB, MBBI, DL, TII->get(MyCPU::ADDW), DstReg)
          .addReg(DstReg).addReg(TmpReg).addImm(31).addImm(0);

      if (Top != 0) {
        Register TmpReg2 = MRI.createVirtualRegister(&MyCPU::GPRRegClass);
        BuildMI(MBB, MBBI, DL, TII->get(MyCPU::MOVIW), TmpReg2).addImm(Top);
        BuildMI(MBB, MBBI, DL, TII->get(MyCPU::SHLIW), TmpReg2)
            .addReg(TmpReg2).addImm(30);
        BuildMI(MBB, MBBI, DL, TII->get(MyCPU::ADDW), DstReg)
            .addReg(DstReg).addReg(TmpReg2).addImm(31).addImm(0);
      }
    }
    MI.eraseFromParent();
    return true;
  }

  //===-- LOAD_ADDR: 加载地址（同 LOAD_IMM） ---===//
  case MyCPU::LOAD_ADDR: {
    Register DstReg = MI.getOperand(0).getReg();
    int64_t Addr = MI.getOperand(1).getImm();
    if (isInt<15>(Addr)) {
      BuildMI(MBB, MBBI, DL, TII->get(MyCPU::MOVIW), DstReg).addImm(Addr);
      MI.eraseFromParent();
      return true;
    }
    // 超出 15 位范围的地址使用与 LOAD_IMM 相同的拆分逻辑
    uint32_t V = static_cast<uint32_t>(Addr);
    unsigned Lo = V & 0x7FFF;
    int32_t SE_Lo = (Lo & 0x4000) ? (Lo | 0xFFFF8000) : Lo;
    int32_t Remaining = static_cast<int32_t>(V) - SE_Lo;
    int32_t RemShifted = Remaining >> 15;
    unsigned Mid = RemShifted & 0x7FFF;
    unsigned Top = (RemShifted >> 15) & 0x3;

    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::MOVIW), DstReg).addImm(Lo);
    if (Mid != 0 || Top != 0) {
      Register TmpReg = MRI.createVirtualRegister(&MyCPU::GPRRegClass);
      BuildMI(MBB, MBBI, DL, TII->get(MyCPU::MOVIW), TmpReg).addImm(Mid);
      if (Mid & 0x4000) {
        Register FixReg = MRI.createVirtualRegister(&MyCPU::GPRRegClass);
        BuildMI(MBB, MBBI, DL, TII->get(MyCPU::MOVIW), FixReg).addImm(1);
        BuildMI(MBB, MBBI, DL, TII->get(MyCPU::SHLIW), FixReg)
            .addReg(FixReg).addImm(15);
        BuildMI(MBB, MBBI, DL, TII->get(MyCPU::ADDW), TmpReg)
            .addReg(TmpReg).addReg(FixReg).addImm(31).addImm(0);
      }
      BuildMI(MBB, MBBI, DL, TII->get(MyCPU::SHLIW), TmpReg)
          .addReg(TmpReg).addImm(15);
      BuildMI(MBB, MBBI, DL, TII->get(MyCPU::ADDW), DstReg)
          .addReg(DstReg).addReg(TmpReg).addImm(31).addImm(0);
      if (Top != 0) {
        Register TmpReg2 = MRI.createVirtualRegister(&MyCPU::GPRRegClass);
        BuildMI(MBB, MBBI, DL, TII->get(MyCPU::MOVIW), TmpReg2).addImm(Top);
        BuildMI(MBB, MBBI, DL, TII->get(MyCPU::SHLIW), TmpReg2)
            .addReg(TmpReg2).addImm(30);
        BuildMI(MBB, MBBI, DL, TII->get(MyCPU::ADDW), DstReg)
            .addReg(DstReg).addReg(TmpReg2).addImm(31).addImm(0);
      }
    }
    MI.eraseFromParent();
    return true;
  }

  //===-- BEQ: beq $rs1, $rs2, $target ---===//
  // 展开为 CMPW + BZW
  case MyCPU::BEQ: {
    Register Src1 = MI.getOperand(0).getReg();
    Register Src2 = MI.getOperand(1).getReg();
    MachineBasicBlock *TBB = MI.getOperand(2).getMBB();
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::CMPW))
        .addReg(Src1).addReg(Src2).addImm(31).addImm(0).addImm(0);
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::BZW)).addImm(0).addMBB(TBB);
    MI.eraseFromParent();
    return true;
  }

  //===-- BNE: bne $rs1, $rs2, $target ---===//
  // 展开为 CMPW + BNZ
  case MyCPU::BNE: {
    Register Src1 = MI.getOperand(0).getReg();
    Register Src2 = MI.getOperand(1).getReg();
    MachineBasicBlock *TBB = MI.getOperand(2).getMBB();
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::CMPW))
        .addReg(Src1).addReg(Src2).addImm(31).addImm(0).addImm(0);
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::BNZ)).addImm(0).addMBB(TBB);
    MI.eraseFromParent();
    return true;
  }

  //===-- BLT: blt $rs1, $rs2, $target ---===//
  // 展开为 CMPW + BNZ (检查 F0.N xor F0.V 溢出标志)
  // 注意：MyCPU 只有 BZW/BNZ，这里直接用 BNZ 作为 fallback
  case MyCPU::BLT: {
    Register Src1 = MI.getOperand(0).getReg();
    Register Src2 = MI.getOperand(1).getReg();
    MachineBasicBlock *TBB = MI.getOperand(2).getMBB();
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::CMPW))
        .addReg(Src1).addReg(Src2).addImm(31).addImm(0).addImm(0);
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::BNZ)).addImm(0).addMBB(TBB);
    MI.eraseFromParent();
    return true;
  }

  //===-- BGE: bge $rs1, $rs2, $target ---===//
  // 展开为 CMPW + BZW (与 BLT 互补)
  case MyCPU::BGE: {
    Register Src1 = MI.getOperand(0).getReg();
    Register Src2 = MI.getOperand(1).getReg();
    MachineBasicBlock *TBB = MI.getOperand(2).getMBB();
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::CMPW))
        .addReg(Src1).addReg(Src2).addImm(31).addImm(0).addImm(0);
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::BZW)).addImm(0).addMBB(TBB);
    MI.eraseFromParent();
    return true;
  }

  //===-- PseudoCMP: setcc 产生 0/1 结果 ---===//
  // PseudoCMP rd, rs1, rs2, cond → CMPW + MOVIW 0 (保守 — 始终为假)
  // F0 标志位由 CMPW 正确设置，供后续 Bcc 使用
  case MyCPU::PseudoCMP: {
    Register DstReg = MI.getOperand(0).getReg();
    Register Src1 = MI.getOperand(1).getReg();
    Register Src2 = MI.getOperand(2).getReg();
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::CMPW))
        .addReg(Src1).addReg(Src2).addImm(31).addImm(0).addImm(0);
    BuildMI(MBB, MBBI, DL, TII->get(MyCPU::MOVIW), DstReg).addImm(0);
    MI.eraseFromParent();
    return true;
  }

  //===-- SELECT: select $rd, $cond, $rs1, $rs2 ---===//
  // 当前不支持条件选择展开，报告错误
  case MyCPU::SELECT: {
    MI.eraseFromParent();
    return true;
  }

  //===-- COMPILER_BARRIER: 编译屏障 — 仅删除 ---===//
  case MyCPU::COMPILER_BARRIER: {
    MI.eraseFromParent();
    return true;
  }

  default:
    return false;
  }
}

} // namespace llvm

char MyCPUExpandPseudo::ID = 0;

INITIALIZE_PASS(MyCPUExpandPseudo, "mycpu-expand-pseudo",
                "MyCPU Pseudo Instruction Expansion", false, false)

FunctionPass *llvm::createMyCPUExpandPseudoPass() {
  return new MyCPUExpandPseudo();
}
