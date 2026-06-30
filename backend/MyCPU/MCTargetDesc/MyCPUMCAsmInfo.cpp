//===-- MyCPUMCAsmInfo.cpp - MyCPU MC AsmInfo -----------------------------===//

#include "MyCPUMCAsmInfo.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

MyCPUMCAsmInfo::MyCPUMCAsmInfo(const MCTargetOptions &Options,
                                 const Triple &TT)
    : MCAsmInfoELF(Options) {
  CodePointerSize = 4;
  CalleeSaveStackSlotSize = 4;

  CommentString = ";";

  SupportsDebugInformation = true;
  HasIdentDirective = true;

  HasDotTypeDotSizeDirective = true;
  HasSingleParameterDotFile = true;
  MinInstAlignment = 4;

  Data8bitsDirective = "\t.byte\t";
  Data16bitsDirective = "\t.half\t";
  Data32bitsDirective = "\t.word\t";

  ZeroDirective = "\t.space\t";
  AsciiDirective = "\t.ascii\t";
  AscizDirective = "\t.asciz\t";

  UsesELFSectionDirectiveForBSS = true;
  HasFunctionAlignment = true;
  HasDotTypeDotSizeDirective = true;
}
