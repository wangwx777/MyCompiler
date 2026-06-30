add_llvm_component_group(MyCPU)

# === TableGen generation ===

set(LLVM_TARGET_DEFINITIONS MyCPU.td)

tablegen(LLVM MyCPUGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM MyCPUGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM MyCPUGenSubtargetInfo.inc -gen-subtarget)
tablegen(LLVM MyCPUGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM MyCPUGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM MyCPUGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM MyCPUGenDisassemblerTables.inc -gen-disassembler)

add_public_tablegen_target(MyCPUCommonTableGen)

# === MyCPU CodeGen target ===

add_llvm_target(MyCPUCodeGen
  MyCPUTargetMachine.cpp
  MyCPUSubtarget.cpp
  MyCPUInstrInfo.cpp
  MyCPURegisterInfo.cpp
  MyCPUFrameLowering.cpp
  MyCPUISelLowering.cpp
  MyCPUISelDAGToDAG.cpp
  MyCPUAsmPrinter.cpp
  MyCPUCallingConv.cpp
  MyCPUMachineFunctionInfo.cpp

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  CodeGen
  Core
  MC
  MyCPUDesc
  MyCPUInfo
  SelectionDAG
  Support
  Target

  ADD_TO_COMPONENT
  MyCPU
)

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)
add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
