//===-- MyCPUELFStreamer.cpp - MyCPU ELF Target Streamer ------------------===//

#include "MyCPUELFStreamer.h"
#include "MyCPUFixupKinds.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCValue.h"

using namespace llvm;

// MyCPU ELF machine type (user-defined, not in standard ELF.h)
enum { EM_MYCPU = 0x8472 };

// MyCPU ELF relocation types
enum {
  R_MYCPU_NONE = 0,
  R_MYCPU_32 = 1,
  R_MYCPU_16 = 2,
  R_MYCPU_PCRel_16 = 3,
};

namespace {

class MyCPUELFObjectWriter : public MCELFObjectTargetWriter {
public:
  MyCPUELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(false, OSABI, EM_MYCPU, false) {}

  unsigned getRelocType(const MCFixup &Fixup, const MCValue &Target,
                        bool IsPCRel) const override {
    unsigned Kind = Fixup.getKind();
    switch (Kind) {
    case MyCPU::fixup_MyCPU_32:
      return R_MYCPU_32;
    case MyCPU::fixup_MyCPU_16:
      return R_MYCPU_16;
    case MyCPU::fixup_MyCPU_PCRel_16:
      return R_MYCPU_PCRel_16;
    default:
      return R_MYCPU_NONE;
    }
  }
};

} // namespace

std::unique_ptr<MCObjectTargetWriter>
llvm::createMyCPUELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<MyCPUELFObjectWriter>(OSABI);
}
