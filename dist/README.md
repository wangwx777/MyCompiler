# MyCPU Standalone Toolchain

**自包含 MyCPU C 编译器工具链** — 复制 `dist/` 目录到任何 Windows PC 即可使用，无需 LLVM 源码。

## 快速开始

```batch
REM 1. 将 dist/ 复制到目标 PC
REM 2. 添加 bin 目录到 PATH
set PATH=C:\mycpu-toolchain\dist\bin;%PATH%

REM 3. 编译 C 代码
mycc test.c -o test.o

REM 4. 查看生成的机器码
mycc --dis test.o
```

## 目录结构

```
dist/
├── bin/
│   ├── clang.exe          # C/C++ 编译器 (已内置 MyCPU target)
│   ├── llvm-mc.exe        # 汇编器/反汇编器
│   ├── llvm-objdump.exe   # ELF 目标文件反汇编
│   ├── llc.exe            # LLVM IR → 目标文件
│   ├── mycc.bat           # ★ Windows 封装脚本
│   └── mycc               # ★ Bash/Linux 封装脚本
├── lib/
│   └── clang/23/include/  # clang 内置头文件 (stdint.h, stddef.h, ...)
└── include/
    └── mycpu_cop.h        # MyCPU 协处理器内置函数
```

## 用法

### 编译 C → 目标文件 (.o)

```batch
mycc test.c                  # 默认 -O2 -c → test.o
mycc -O0 -c test.c -o out.o # 指定优化级别和输出
mycc -O1 test.c              # O1 优化
```

### 编译 C → 汇编 (.s)

```batch
mycc -S test.c               # → test.s
mycc -O2 -S test.c           # 带优化的汇编输出
```

### 汇编 .s → .o

```batch
mycc --asm test.s -o test.o  # llvm-mc 汇编
```

### 反汇编 .o 查看机器码

```batch
mycc --dis test.o            # 查看 32-bit 定长 MyCPU 指令
```

### LLVM IR → .o

```batch
mycc --llc test.ll -o test.o # llc 编译 LLVM IR
```

### 版本信息

```batch
mycc --version               # 显示 clang/LLVM 版本
```

## MyCPU 指令集概要

| Opcode | 助记符          | 功能              |
|--------|-----------------|-------------------|
| 0x00-0x0A | `add/sub/and/or/xor/not` | 算术逻辑  |
| 0x0C-0x11 | `shl/shr/sar`   | 移位              |
| 0x12   | `movab.w/h/b`   | 寄存器传送        |
| 0x13   | `movi.w/h/b`    | 立即数加载        |
| 0x14   | `ld.w/h/b`      | 协处理器读        |
| 0x15   | `st.w/h/b`      | 协处理器写        |
| 0x16-0x17 | `cmp/cmpi`  | 比较              |
| 0x18-0x1F | `jmp/jalr/bz/bnz/call/ret` | 控制流 |
| 0x22-0x27 | `bset/bclr/bnot/btst/nop/halt` | 位操作 |
| 0x28   | `mov.w/h/b`     | 内存访存          |

32-bit 定长指令，三种粒度 (Word: `.w` / Half: `.h` / Byte: `.b`)。

## 如何打包到另一台 PC

```batch
REM 在本机打包
cd E:\llvm_workspace\mycpu-toolchain
7z a mycpu-toolchain.7z dist\

REM 或者直接复制目录到 U 盘/网络共享
xcopy /E dist\ X:\mycpu-toolchain\
```

> **注意**: clang.exe (~191MB) 是最大的文件。整个 dist 包约 ~300MB。

## 限制

- **仅支持 `-c` (编译到 .o)**，目前没有 linker 用于最终链接
- **Bare-metal 目标** — 无 libc / 标准库
- **O0 稳定**，O2 对简单函数可用，含递归/复杂分支可能挂死
- 输出格式: 32-bit little-endian ELF
- 构建日期: 2026-06-26, clang 23.0.0git
