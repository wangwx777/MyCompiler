# MyCPU LLVM Toolchain

基于 LLVM 23.x 的 MyCPU 自定义 CPU 编译器工具链。

## 目录结构

```
mycpu-toolchain/
├── backend/
│   ├── MyCPU/              # LLVM 后端完整源码 (TableGen + C++)
│   └── llvm-patches/       # 需要打入 LLVM 的通用代码补丁(参考)
├── frontend/
│   ├── MyCPU.h             # Clang MyCPU 目标定义
│   ├── MyCPU.cpp           # Clang MyCPU 目标实现
│   └── *.patched           # LLVM/Clang 通用文件补丁(参考)
├── tests/                  # 测试用例
├── examples/
│   ├── demo.cpp            # 网络包处理 demo
│   └── read_linear_table.s # 协处理器查表(汇编实现)
├── docs/                   # ISA 规格文档
├── scripts/
│   ├── setup-llvm.sh       # 一键搭建 LLVM 编译环境
│   └── test.sh             # 测试脚本
└── README.md
```

## 快速开始

### 1. 准备 LLVM 源码

```bash
git clone https://github.com/llvm/llvm-project.git
cd llvm-project
git checkout <你的 LLVM 版本>
```

### 2. 安装 MyCPU 后端

```bash
cd mycpu-toolchain
./scripts/setup-llvm.sh ~/llvm-project ~/llvm-project/build
```

**注意**：首次使用时需要手动合并以下补丁：
- `backend/llvm-patches/Triple.h.patched` → `llvm/include/llvm/TargetParser/Triple.h`
- `backend/llvm-patches/Triple.cpp.patched` → `llvm/lib/TargetParser/Triple.cpp`
- `backend/llvm-patches/TargetDataLayout.cpp.patched` → `llvm/lib/TargetParser/TargetDataLayout.cpp`
- `frontend/Targets.cpp.patched` → `clang/lib/Basic/Targets.cpp`
- `frontend/CMakeLists.txt.patched` → `clang/lib/Basic/CMakeLists.txt`
- `backend/llvm-patches/LLVM_CMakeLists.txt.patched` → `llvm/CMakeLists.txt`

### 3. 编译

```bash
ninja -C ~/llvm-project/build clang llvm-mc
```

### 4. 测试

```bash
./scripts/test.sh ~/llvm-project/build
```

## 使用

```bash
# 编译 C 代码
clang -target mycpu-unknown-elf -c -o output.o input.c

# 生成汇编
clang -target mycpu-unknown-elf -S -o output.s input.c

# 汇编单条指令
echo 'movab.w r1, r2' | llvm-mc -triple=mycpu-unknown-elf -show-encoding
```


32 位定长指令，三种粒度 (Word/Half/Byte)，opcode 共享、size 区分。
