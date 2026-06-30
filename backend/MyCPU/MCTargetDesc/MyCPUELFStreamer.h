//===-- MyCPUELFStreamer.h - MyCPU ELF Target Streamer -----------*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_MYCPU_MCTARGETDESC_MYCPUELFSTREAMER_H
#define LLVM_LIB_TARGET_MYCPU_MCTARGETDESC_MYCPUELFSTREAMER_H

#include "llvm/MC/MCObjectWriter.h"
#include <memory>

namespace llvm {

std::unique_ptr<MCObjectTargetWriter>
createMyCPUELFObjectWriter(uint8_t OSABI);

} // namespace llvm

#endif
