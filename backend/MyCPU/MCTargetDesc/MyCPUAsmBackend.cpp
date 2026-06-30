//===-- MyCPUAsmBackend.cpp - MyCPU Assembler Backend ---------------------===//

#include "MCTargetDesc/MyCPUMCTargetDesc.h"
#include "MCTargetDesc/MyCPUFixupKinds.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

#define DEBUG_TYPE "mycpu-asm-backend"

namespace {

class MyCPUAsmBackend : public MCAsmBackend {
public:
  MyCPUAsmBackend(const Target &T, const MCSubtargetInfo &STI)
      : MCAsmBackend(llvm::endianness::little) {}

  void applyFixup(const MCFragment &F, const MCFixup &Fixup,
                  const MCValue &Target, uint8_t *Data, uint64_t Value,
                  bool IsResolved) override {
    unsigned Kind = Fixup.getKind();

    if (!Value)
      return;

    adjustFixupValue(Fixup, Value);

    // Data is already biased by Fixup.getOffset() by the caller
    // (MCAssembler.cpp:768-769). Do NOT add Fixup.getOffset() again.
    switch (Kind) {
    case MyCPU::fixup_MyCPU_16:
    case MyCPU::fixup_MyCPU_PCRel_16: {
      // 15-bit value placed into bits[31:17] (fieldA+B+C) for BR instructions.
      // For ST/LD, the fixup spans different fields (see encoder), but we
      // still apply to bits[31:17] as the encoder leaves those bits clear
      // when emitting expression operands.
      // byte 2 bits[7:1] = V[6:0], bit[0] = f[2] (preserved).
      // byte 3 bits[7:0] = V[14:7].
      uint64_t V = Value & 0x7FFF;
      Data[2] |= uint8_t((V << 1) & 0xFE);
      Data[3] |= uint8_t((V >> 7) & 0xFF);
      break;
    }
    case MyCPU::fixup_MyCPU_32:
      // 32-bit value covering the full instruction word.
      for (unsigned i = 0; i < 4; ++i)
        Data[i] |= uint8_t((Value >> (i * 8)) & 0xFF);
      break;
    }
  }

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createMyCPUELFObjectWriter(0);
  }

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override {
    const static MCFixupKindInfo Infos[MyCPU::NumTargetFixupKinds] = {
        {"fixup_MyCPU_32",       0, 32, 0},
        {"fixup_MyCPU_16",       0, 16, 0},
        {"fixup_MyCPU_PCRel_16", 0, 16, 0},
    };

    if (Kind < FirstTargetFixupKind)
      return MCAsmBackend::getFixupKindInfo(Kind);

    assert(unsigned(Kind - FirstTargetFixupKind) < std::size(Infos) &&
           "Invalid kind!");
    return Infos[Kind - FirstTargetFixupKind];
  }

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override {
    // NOP encoding: 0xA6 with size=Word(2) in bits[7:6], so 0x00A6
    uint32_t NopEnc = 0x000000A6;
    for (uint64_t i = 0; i < Count; i += 4) {
      for (unsigned j = 0; j < 4; ++j)
        OS.write(uint8_t((NopEnc >> (j * 8)) & 0xFF));
    }
    return true;
  }

  bool fixupNeedsRelaxationAdvanced(const MCFragment &, const MCFixup &,
                                     const MCValue &, uint64_t,
                                     bool Resolved) const override {
    return false;
  }

  unsigned getMaximumNopSize(const MCSubtargetInfo &STI) const override {
    return 4;
  }

private:
  void adjustFixupValue(const MCFixup &Fixup, uint64_t &Value) const {
    unsigned Kind = Fixup.getKind();
    switch (Kind) {
    case MyCPU::fixup_MyCPU_PCRel_16:
      // PC-relative: offset from fixup location
      Value -= 2;
      break;
    default:
      break;
    }
  }
};

} // namespace

MCAsmBackend *llvm::createMyCPUAsmBackend(const Target &T,
                                            const MCSubtargetInfo &STI,
                                            const MCRegisterInfo &MRI,
                                            const MCTargetOptions &Options) {
  return new MyCPUAsmBackend(T, STI);
}
