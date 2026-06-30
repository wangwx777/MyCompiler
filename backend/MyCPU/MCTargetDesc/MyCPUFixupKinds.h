//===-- MyCPUFixupKinds.h - MyCPU Fixup Kinds -------------------*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_MYCPU_MCTARGETDESC_MYCPUFIXUPKINDS_H
#define LLVM_LIB_TARGET_MYCPU_MCTARGETDESC_MYCPUFIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

namespace llvm {
namespace MyCPU {

enum Fixups {
  // 32-bit absolute address (e.g. MOVIW loading a global symbol)
  fixup_MyCPU_32 = FirstTargetFixupKind,

  // 16-bit absolute field (15-bit immediate in JMP/CALL/MOVIW)
  fixup_MyCPU_16,

  // 16-bit PC-relative (branch targets)
  fixup_MyCPU_PCRel_16,

  // Marker
  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};

} // namespace MyCPU
} // namespace llvm

#endif
