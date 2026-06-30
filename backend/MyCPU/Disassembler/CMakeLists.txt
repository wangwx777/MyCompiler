add_llvm_component_library(LLVMMyCPUDisassembler
  MyCPUDissassembler.cpp

  ADD_TO_COMPONENT
  MyCPU

  LINK_COMPONENTS
  MC
  MCDisassembler
  MyCPUDesc
  MyCPUInfo
  Support
)
