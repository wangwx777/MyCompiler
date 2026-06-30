//===--- MyCPU.h - MyCPU target feature support -----------------*- C++ -*-===//
//
// Clang-side TargetInfo for the MyCPU 32-bit little-endian target.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_MYCPU_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_MYCPU_H

#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"

namespace clang {
namespace targets {

class LLVM_LIBRARY_VISIBILITY MyCPUTargetInfo : public TargetInfo {
  static const TargetInfo::GCCRegAlias GCCRegAliases[];
  static const char *const GCCRegNames[];

public:
  MyCPUTargetInfo(const llvm::Triple &Triple, const TargetOptions &Opts)
      : TargetInfo(Triple) {
    // 32-bit little-endian bare-metal ELF target
    BigEndian = false;
    NoAsmVariants = true;
    LongDoubleWidth = LongDoubleAlign = 64;
    LongDoubleFormat = &llvm::APFloat::IEEEdouble();
    IntMaxType = SignedLongLong;
    Int64Type = SignedLongLong;
    IntWidth = IntAlign = 32;
    LongWidth = LongAlign = 32;
    LongLongWidth = LongLongAlign = 64;
    PointerWidth = PointerAlign = 32;
    SizeType = UnsignedInt;
    IntPtrType = SignedInt;
    PtrDiffType = SignedInt;
    resetDataLayout("e-m:e-p:32:32-i64:64-n32");
    SuitableAlign = 32;
    MaxAtomicPromoteWidth = MaxAtomicInlineWidth = 32;
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  llvm::SmallVector<Builtin::InfosShard> getTargetBuiltins() const override {
    return {};
  }

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::VoidPtrBuiltinVaList;
  }

  ArrayRef<const char *> getGCCRegNames() const override;
  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override;

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &info) const override;

  std::string_view getClobbers() const override { return ""; }
  bool hasBitIntType() const override { return true; }
};

} // namespace targets
} // namespace clang

#endif // LLVM_CLANG_LIB_BASIC_TARGETS_MYCPU_H
