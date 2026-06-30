#!/bin/bash
# ============================================================
# MyCPU Toolchain — 测试脚本
# ============================================================
BUILD_DIR="${1:-$HOME/llvm-project/build}"
CLANG="$BUILD_DIR/bin/clang"
LLVM_MC="$BUILD_DIR/bin/llvm-mc"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MYCPU_ROOT="$(dirname "$SCRIPT_DIR")"
TESTS="$MYCPU_ROOT/tests"
EXAMPLES="$MYCPU_ROOT/examples"

TARGET="mycpu-unknown-elf"
PASS=0
FAIL=0

echo "=== MyCPU Toolchain Tests ==="
echo ""

# 1. Basic instruction encoding tests
echo "--- Instruction Encoding ---"
for inst in \
  "movab.w r1, r2" \
  "mov.w r3, [r4 + 0]" \
  "ld.w r5, 1, 32" \
  "st.w r6, 2, 64" \
  "movi.w r7, 1000" \
  "movi.h r8, 100" \
  "movi.b r9, 50" \
  "add.w r1, r2[31:0]" \
  "cmp.w r1, r2[31:0], 0" \
  "jmp 0" \
  "call 0" \
  "ret"
do
  result=$(echo "$inst" | "$LLVM_MC" -triple=$TARGET -show-encoding 2>&1)
  if [ $? -eq 0 ]; then
    echo "  ✅ $inst"
    PASS=$((PASS+1))
  else
    echo "  ❌ $inst"
    FAIL=$((FAIL+1))
  fi
done

# 2. C compilation tests
echo ""
echo "--- C Compilation ---"
for f in "$TESTS"/*.c; do
  name=$(basename "$f")
  "$CLANG" -target $TARGET -c -o /dev/null "$f" 2>/dev/null
  if [ $? -eq 0 ]; then
    echo "  ✅ $name"
    PASS=$((PASS+1))
  else
    echo "  ❌ $name"
    FAIL=$((FAIL+1))
  fi
done

# 3. Demo compilation
echo ""
echo "--- Demo ---"
if [ -f "$EXAMPLES/demo.cpp" ]; then
  "$CLANG" -target $TARGET -S -o /tmp/demo_test.s "$EXAMPLES/demo.cpp" -O0 2>/dev/null
  if [ $? -eq 0 ]; then
    echo "  ✅ demo.cpp → assembly"
    PASS=$((PASS+1))
  else
    echo "  ❌ demo.cpp"
    FAIL=$((FAIL+1))
  fi
fi

# 4. Coprocessor assembly
if [ -f "$EXAMPLES/read_linear_table.s" ]; then
  "$LLVM_MC" -triple=$TARGET -filetype=obj -o /dev/null "$EXAMPLES/read_linear_table.s" 2>/dev/null
  if [ $? -eq 0 ]; then
    echo "  ✅ read_linear_table.s → object"
    PASS=$((PASS+1))
  else
    echo "  ❌ read_linear_table.s"
    FAIL=$((FAIL+1))
  fi
fi

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
