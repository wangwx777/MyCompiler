# MyCPU 工具链 — 新 PC 环境搭建指南

> 从零开始，在新 PC 上跑通 MyCPU 编译器工具链。两条路径：**直接用**（5 分钟）或**从源码编**（1-2 小时）。

---

## 目录

1. [路径A: 直接使用预编译工具链（推荐）](#路径a-直接使用预编译工具链推荐)
2. [路径B: 从源码完整编译](#路径b-从源码完整编译)
3. [验证安装](#验证安装)
4. [常见问题排查](#常见问题排查)
5. [附录: 常用命令速查](#附录-常用命令速查)

---

## 路径A: 直接使用预编译工具链（推荐）

> 适用场景：只需用编译器，不修改后端源码。任何 Windows PC 均可。

### A1. 复制 dist/ 目录

```
将整个 mycpu-toolchain/dist/ 目录复制到目标 PC，例如:
  C:\mycpu-toolchain\dist\
    ├── bin\          ← 编译器 + 工具
    ├── include\      ← MyCPU 内建函数头文件
    └── lib\          ← clang 内置头文件 (stdint.h 等)
```

### A2. 添加到 PATH

**方法一：临时生效（当前终端）**

```batch
set PATH=C:\mycpu-toolchain\dist\bin;%PATH%
```

**方法二：永久生效**

1. `Win + R` → 输入 `sysdm.cpl` → 回车
2. 高级 → 环境变量 → 系统变量 → Path → 编辑
3. 添加 `C:\mycpu-toolchain\dist\bin`
4. 确定，重新打开终端

### A3. 测试

```batch
REM 确认工具可用
clang --version
llvm-mc --version

REM 编译一个 C 文件
mycc test.c -o test.o

REM 反汇编查看机器码
mycc --dis test.o
```

> `mycc.bat` 是对 `clang -target mycpu-unknown-elf` 的封装，让使用更简单。

### A4. dist/ 包含的文件

| 文件 | 大小 | 功能 |
|------|------|------|
| `clang.exe` | ~191 MB | C 编译器 (已内置 MyCPU target) |
| `llvm-mc.exe` | ~10 MB | 汇编器 / 反汇编器 |
| `llc.exe` | ~97 MB | LLVM IR → 目标文件 |
| `llvm-objdump.exe` | ~26 MB | ELF 反汇编 |
| `mycc.bat` | ~5 KB | Windows 封装脚本 |
| `mycc` | ~5 KB | Linux/Bash 封装脚本 |
| `include/mycpu_cop.h` | — | 协处理器内置函数 |
| `lib/clang/23/include/` | — | stdint.h, stddef.h 等 |

---

## 路径B: 从源码完整编译

> 适用场景：需要修改 MyCPU 后端源码，或需要最新的开发版本。

### B1. 环境准备

#### 硬件要求

| 项目 | 最低 | 推荐 |
|------|------|------|
| 内存 | 8 GB | 16 GB+ |
| 磁盘 (LLVM 源码) | ~2 GB | — |
| 磁盘 (编译产物) | ~15 GB | 20 GB+ (用 Debug 构建更大) |
| CPU 核心 | 4 | 8+ (并行编译) |

#### 软件依赖

**Windows:**

```batch
REM 1. Git for Windows
REM    https://git-scm.com/download/win

REM 2. CMake (≥3.20)
REM    https://cmake.org/download/
REM    安装时勾选 "Add CMake to system PATH"

REM 3. Ninja (构建工具)
REM    https://github.com/ninja-build/ninja/releases
REM    下载 ninja-win.zip, 解压 ninja.exe 到 PATH 中的某目录

REM 4. Visual Studio 2022 (Build Tools 即可)
REM    https://visualstudio.microsoft.com/downloads/
REM    安装时勾选 "Desktop development with C++"

REM 5. Python 3 (LLVM 构建脚本需要)
REM    https://www.python.org/downloads/
```

验证安装：

```batch
git --version
cmake --version
ninja --version
cl        REM MSVC 编译器, 应在 PATH 中
python --version
```

**Linux (Ubuntu/Debian):**

```bash
sudo apt update
sudo apt install -y git cmake ninja-build build-essential python3
```

### B2. 下载 LLVM 源码

```bash
# 克隆 LLVM 项目 (较大, ~1.5 GB, 需要几分钟)
git clone https://github.com/llvm/llvm-project.git
cd llvm-project

# 切换到 LLVM 23.x 分支 (根据你的 LLVM 版本)
git checkout release/23.x
# 或使用特定 commit:
# git checkout <commit-hash>
```

### B3. 安装 MyCPU 后端到 LLVM 源树

```bash
# 假设目录结构:
#   ~/mycpu-toolchain/     ← MyCPU 后端源码
#   ~/llvm-project/        ← LLVM 源码

cd ~/mycpu-toolchain
./scripts/setup-llvm.sh ~/llvm-project ~/llvm-project/build
```

`setup-llvm.sh` 自动完成：
1. 复制 `backend/MyCPU/` → `llvm/lib/Target/MyCPU/`
2. 复制 `frontend/MyCPU.{h,cpp}` → `clang/lib/Basic/Targets/`
3. 检查通用文件补丁状态

### B4. 手动打补丁（仅首次）

`setup-llvm.sh` 会提示哪些补丁尚未合并。需要手动将以下文件的内容合并到 LLVM 源树：

| 补丁文件 | 目标文件 | 修改内容 |
|----------|----------|----------|
| `backend/llvm-patches/Triple.h.patched` | `llvm/include/llvm/TargetParser/Triple.h` | 添加 `mycpu` 三元组 |
| `backend/llvm-patches/Triple.cpp.patched` | `llvm/lib/TargetParser/Triple.cpp` | 添加 `mycpu` case |
| `backend/llvm-patches/TargetDataLayout.cpp.patched` | `llvm/lib/TargetParser/TargetDataLayout.cpp` | 注册 MyCPUDataLayout |
| `backend/llvm-patches/LLVM_CMakeLists.txt.patched` | `llvm/CMakeLists.txt` | 注册 MyCPU target |
| `frontend/Targets.cpp.patched` | `clang/lib/Basic/Targets.cpp` | 包含 MyCPU.h |
| `frontend/CMakeLists.txt.patched` | `clang/lib/Basic/CMakeLists.txt` | 编译 MyCPU.cpp |

> **提示**：补丁文件中的 `+` 行是需要**添加**的，`-` 行是需要**删除**的。也可以在 LLVM 源文件中搜索 `RISCV` 或 `X86` 找到类似的注册位置，参照添加 MyCPU 的注册代码。

### B5. 编译

```bash
cd ~/llvm-project

# CMake 配置 + 编译 (setup-llvm.sh 已做过 CMake 配置, 如果已配置可跳过)
# ./mycpu-toolchain/scripts/setup-llvm.sh ~/llvm-project ~/llvm-project/build

# 编译 (首次全量编译约 30-60 分钟, 取决于 CPU 核心数)
ninja -C build clang llvm-mc
```

编译产物位置：

```
build/bin/
├── clang.exe          ← MyCPU 编译器
├── llvm-mc.exe        ← 汇编器
├── llc.exe            ← LLVM IR 编译器
├── llvm-objdump.exe   ← 反汇编器
└── ...
```

### B6. 提取到 dist/（可选）

```bash
cp build/bin/clang.exe        ~/mycpu-toolchain/dist/bin/
cp build/bin/clang++.exe      ~/mycpu-toolchain/dist/bin/   # 如果存在
cp build/bin/llvm-mc.exe      ~/mycpu-toolchain/dist/bin/
cp build/bin/llc.exe          ~/mycpu-toolchain/dist/bin/
cp build/bin/llvm-objdump.exe ~/mycpu-toolchain/dist/bin/
```

### B7. 增量编译（修改源码后）

```bash
# 1. 将修改后的后端代码安装到 LLVM 源树
cd ~/mycpu-toolchain
./scripts/setup-llvm.sh ~/llvm-project ~/llvm-project/build

# 2. 增量编译 (只改了几个文件, 一般 1-5 分钟)
ninja -C ~/llvm-project/build clang llvm-mc

# 3. 更新 dist/
cp ~/llvm-project/build/bin/clang.exe dist/bin/
cp ~/llvm-project/build/bin/llvm-mc.exe dist/bin/
```

---

## 验证安装

### 1. 单条指令汇编

```bash
# 测试汇编 → 机器码
echo 'add.w r1, r2[31:0]' | llvm-mc -triple=mycpu-unknown-elf -show-encoding
# 期望输出: add.w r1, r2[31:0] ; encoding: [0x00,0x08,0x46,0x7c]

echo 'movab.w r3, r5' | llvm-mc -triple=mycpu-unknown-elf -show-encoding
echo 'jmp 0x100' | llvm-mc -triple=mycpu-unknown-elf -show-encoding
```

### 2. 编译 C 程序

```c
// 创建 test.c
int add(int a, int b) {
    return a + b;
}
```

```bash
# 编译为目标文件
clang -target mycpu-unknown-elf -c -O2 test.c -o test.o

# 反汇编
llvm-objdump -d test.o

# 生成汇编
clang -target mycpu-unknown-elf -S -O2 test.c -o test.s
cat test.s
```

### 3. 使用 mycc 封装脚本

```bash
# Windows
mycc.bat test.c -o test.o
mycc.bat -S test.c
mycc.bat --dis test.o

# Linux
./dist/bin/mycc test.c -o test.o
./dist/bin/mycc --dis test.o
```

### 4. 运行测试套件

```bash
cd ~/mycpu-toolchain
./scripts/test.sh ~/llvm-project/build
```

### 5. 使用模拟器运行

```bash
# 编译模拟器
gcc -O2 -o mycpu-sim sim/mycpu-sim.c

# 运行编译好的 .o 文件
./mycpu-sim test.o --dump-regs --max-steps 100
```

---

## 常见问题排查

### Q1: `clang: error: unable to execute command: program not executable`

**原因**: 缺少 MSVC 运行库。

**解决**: 安装 [Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe)。

### Q2: `ninja: error: loading 'build.ninja': The system cannot find the file`

**原因**: CMake 配置未完成。

**解决**:
```bash
cd ~/llvm-project
mkdir -p build
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_TARGETS_TO_BUILD="MyCPU" \
  -DLLVM_ENABLE_PROJECTS="clang" \
  -S llvm -B build
```

### Q3: CMake 报错 `Could NOT find Python3`

**解决**: 安装 Python 3 并确保在 PATH 中，或者指定 Python 路径：
```bash
cmake ... -DPython3_EXECUTABLE=C:/Python313/python.exe
```

### Q4: `fatal error: 'MyCPUGenRegisterInfo.inc' file not found`

**原因**: TableGen 未生成 .inc 文件。

**解决**: 先确保 `llvm-tblgen` 已编译，然后只编译 MyCPU 后端：
```bash
ninja -C build llvm-tblgen          # 先编译 TableGen 工具
ninja -C build LLVMMyCPUCodeGen     # 再编译 MyCPU 后端 (会自动调用 tblgen)
```

### Q5: 编译时内存不足

**解决**: 限制并行编译数：
```bash
ninja -C build clang llvm-mc -j 2   # 只用 2 个并行任务
```
或者换用 `-DCMAKE_BUILD_TYPE=Release`（Release 比 Debug 内存占用少很多）。

### Q6: `mycpu-unknown-elf` target not recognized

**原因**: LLVM 通用文件补丁未打（Triple.h / Triple.cpp 等）。

**解决**: 回到 [B4. 手动打补丁](#b4-手动打补丁仅首次)，确保所有 6 个补丁文件已合并。

### Q7: 在 Linux 上使用 dist/ 中的 .exe 文件

**原因**: `dist/bin/` 中的是 Windows 可执行文件。

**解决**: 在 Linux 上需要从源码编译（路径B），或使用 Wine：
```bash
sudo apt install wine
wine dist/bin/clang.exe -target mycpu-unknown-elf -c test.c -o test.o
```

---

## 附录: 常用命令速查

### mycc 封装脚本

```bash
# 编译 C → .o
mycc test.c -o test.o
mycc -O2 -c test.c -o test.o
mycc -O0 -c test.c

# 生成汇编
mycc -S test.c               # → test.s

# 汇编 .s → .o
mycc --asm test.s -o test.o

# 反汇编
mycc --dis test.o

# LLVM IR → .o
mycc --llc test.ll -o test.o

# 版本
mycc --version
```

### clang 直接使用

```bash
# 编译
clang -target mycpu-unknown-elf -c -O2 input.c -o output.o

# 汇编输出
clang -target mycpu-unknown-elf -S -O2 input.c -o output.s

# 查看 LLVM IR
clang -target mycpu-unknown-elf -S -emit-llvm input.c -o output.ll

# 指定优化级别
clang -target mycpu-unknown-elf -c -O0 input.c -o output.o
clang -target mycpu-unknown-elf -c -Os input.c -o output.o  # 优化大小
```

### llvm-mc 汇编器

```bash
# 单条指令汇编 → 编码
echo 'add.w r1, r2[31:0]' | llvm-mc -triple=mycpu-unknown-elf -show-encoding

# 汇编文件
llvm-mc -triple=mycpu-unknown-elf -filetype=obj input.s -o output.o

# 反汇编
llvm-mc -triple=mycpu-unknown-elf -disassemble input.o
```

### llvm-objdump 反汇编

```bash
# 反汇编所有节
llvm-objdump -d test.o

# 显示重定位信息
llvm-objdump -r test.o

# 显示节头
llvm-objdump -h test.o
```

---

> **文档版本**: 2026-07-01
> **适用版本**: LLVM 23.x + MyCPU backend
