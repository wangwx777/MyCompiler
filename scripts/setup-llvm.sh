#!/bin/bash
# ============================================================
# MyCPU Toolchain — LLVM 环境搭建脚本
# ============================================================
# 用法: ./setup-llvm.sh <LLVM源码目录> <build目录>
# 示例: ./setup-llvm.sh ~/llvm-project ~/llvm-project/build
# ============================================================

set -e

LLVM_SRC="${1:-$HOME/llvm-project}"
BUILD_DIR="${2:-$LLVM_SRC/build}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MYCPU_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== MyCPU Toolchain Setup ==="
echo "LLVM Source : $LLVM_SRC"
echo "Build Dir   : $BUILD_DIR"
echo "MyCPU Root  : $MYCPU_ROOT"

# 1. Symlink MyCPU backend into LLVM source tree
echo ""
echo "[1/5] Installing MyCPU backend..."
rm -rf "$LLVM_SRC/llvm/lib/Target/MyCPU"
cp -r "$MYCPU_ROOT/backend/MyCPU" "$LLVM_SRC/llvm/lib/Target/MyCPU"
echo "  → llvm/lib/Target/MyCPU/"

# 2. Copy Clang MyCPU target
echo "[2/5] Installing MyCPU Clang target..."
cp "$MYCPU_ROOT/frontend/MyCPU.h" "$LLVM_SRC/clang/lib/Basic/Targets/"
cp "$MYCPU_ROOT/frontend/MyCPU.cpp" "$LLVM_SRC/clang/lib/Basic/Targets/"
echo "  → clang/lib/Basic/Targets/MyCPU.{h,cpp}"

# 3. Patch LLVM common files
echo "[3/5] Patching LLVM common files..."
# Triple.h — add mycpu triple
if ! grep -q "mycpu" "$LLVM_SRC/llvm/include/llvm/TargetParser/Triple.h" 2>/dev/null; then
  echo "  WARNING: Triple.h needs manual patching (see backend/llvm-patches/)"
fi
# Triple.cpp — add mycpu case
if ! grep -q "mycpu" "$LLVM_SRC/llvm/lib/TargetParser/Triple.cpp" 2>/dev/null; then
  echo "  WARNING: Triple.cpp needs manual patching (see backend/llvm-patches/)"
fi
# TargetDataLayout.cpp — register MyCPUDataLayout
if ! grep -q "MyCPU" "$LLVM_SRC/llvm/lib/TargetParser/TargetDataLayout.cpp" 2>/dev/null; then
  echo "  WARNING: TargetDataLayout.cpp needs manual patching"
fi
# clang/lib/Basic/Targets.cpp — include MyCPU.h
if ! grep -q "MyCPU" "$LLVM_SRC/clang/lib/Basic/Targets.cpp" 2>/dev/null; then
  echo "  WARNING: Targets.cpp needs manual patching (see frontend/)"
fi
# clang/lib/Basic/CMakeLists.txt — add MyCPU.cpp
if ! grep -q "MyCPU" "$LLVM_SRC/clang/lib/Basic/CMakeLists.txt" 2>/dev/null; then
  echo "  WARNING: CMakeLists.txt needs manual patching"
fi
# llvm/CMakeLists.txt — add MyCPU target
if ! grep -q "MyCPU" "$LLVM_SRC/llvm/CMakeLists.txt" 2>/dev/null; then
  echo "  WARNING: llvm/CMakeLists.txt needs manual patching"
fi

# 4. Configure the build
echo "[4/5] Configuring CMake..."
mkdir -p "$BUILD_DIR"
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_TARGETS_TO_BUILD="MyCPU" \
  -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD="" \
  -DLLVM_ENABLE_PROJECTS="clang" \
  -DLLVM_OPTIMIZED_TABLEGEN=ON \
  -DLLVM_BUILD_LLVM_DYLIB=OFF \
  -DLLVM_LINK_LLVM_DYLIB=OFF \
  -S "$LLVM_SRC/llvm" \
  -B "$BUILD_DIR"

echo ""
echo "[5/5] Setup complete!"
echo ""
echo "To build:"
echo "  ninja -C $BUILD_DIR clang llvm-mc"
echo ""
echo "To test:"
echo "  $MYCPU_ROOT/scripts/test.sh $BUILD_DIR"
