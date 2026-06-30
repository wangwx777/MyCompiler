add_llvm_component_library(LLVMMyCPUAsmParser
  MyCPUAsmParser.cpp

  ADD_TO_COMPONENT
  MyCPU

  LINK_COMPONENTS
  MC
  MCParser
  MyCPUDesc
  MyCPUInfo
  Support
  Target
)
