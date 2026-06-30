//===--- MyCPU.cpp - MyCPU target feature support -------------------------===//

#include "MyCPU.h"
#include "Targets.h"
#include "clang/Basic/MacroBuilder.h"

using namespace clang;
using namespace clang::targets;

const char *const MyCPUTargetInfo::GCCRegNames[] = {
    "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
    "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",
    "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
    "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
};

ArrayRef<const char *> MyCPUTargetInfo::getGCCRegNames() const {
  return llvm::ArrayRef(GCCRegNames);
}

const TargetInfo::GCCRegAlias MyCPUTargetInfo::GCCRegAliases[] = {
    {{"zero"}, "r0"},  {{"a0"}, "r1"},   {{"a1"}, "r2"},
    {{"a2"}, "r3"},    {{"a3"}, "r4"},   {{"a4"}, "r5"},
    {{"a5"}, "r6"},    {{"a6"}, "r7"},   {{"t0"}, "r8"},
    {{"t1"}, "r9"},    {{"t2"}, "r10"},  {{"t3"}, "r11"},
    {{"t4"}, "r12"},   {{"t5"}, "r13"},  {{"t6"}, "r14"},
    {{"t7"}, "r15"},   {{"s0"}, "r16"},  {{"s1"}, "r17"},
    {{"s2"}, "r18"},   {{"s3"}, "r19"},  {{"s4"}, "r20"},
    {{"s5"}, "r21"},   {{"s6"}, "r22"},  {{"s7"}, "r23"},
    {{"t8"}, "r24"},   {{"t9"}, "r25"},  {{"t10"}, "r26"},
    {{"t11"}, "r27"},  {{"lr"}, "r28"},  {{"sp"}, "r29"},
    {{"fp"}, "r30"},   {{"gp"}, "r31"},
};

ArrayRef<TargetInfo::GCCRegAlias> MyCPUTargetInfo::getGCCRegAliases() const {
  return llvm::ArrayRef(GCCRegAliases);
}

bool MyCPUTargetInfo::validateAsmConstraint(
    const char *&Name, TargetInfo::ConstraintInfo &info) const {
  switch (*Name) {
  case 'I': // 15-bit signed immediate
  case 'J': // Zero
  default:
    return false;
  }
  return false;
}

void MyCPUTargetInfo::getTargetDefines(const LangOptions &Opts,
                                        MacroBuilder &Builder) const {
  DefineStd(Builder, "mycpu", Opts);
  Builder.defineMacro("__mycpu__");
  Builder.defineMacro("__REGISTER_PREFIX__", "");
}
