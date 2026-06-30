#!/bin/bash
# ============================================================
# MyCPU Toolchain — 快速编译脚本
# ============================================================
# 用法: ./build.sh [build目录]
# ============================================================
BUILD_DIR="${1:-$HOME/llvm-project/build}"

echo "Building MyCPU toolchain..."
ninja -C "$BUILD_DIR" clang llvm-mc LLVMMyCPUCodeGen LLVMMyCPUDesc
echo "Done."
