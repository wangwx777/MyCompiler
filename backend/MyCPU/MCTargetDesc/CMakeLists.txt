add_llvm_component_library(LLVMMyCPUDesc
  MyCPUMCTargetDesc.cpp
  MyCPUInstPrinter.cpp
  MyCPUMCAsmInfo.cpp
  MyCPUMCCodeEmitter.cpp
  MyCPUAsmBackend.cpp
  MyCPUELFStreamer.cpp

  ADD_TO_COMPONENT
  MyCPU

  LINK_COMPONENTS
  MC
  MyCPUInfo
  Support
  Target
)
