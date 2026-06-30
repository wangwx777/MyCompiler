add_llvm_component_library(LLVMMyCPUInfo
  MyCPUTargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support
  Target

  ADD_TO_COMPONENT
  MyCPU
)
