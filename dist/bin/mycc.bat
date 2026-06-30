@echo off
REM ============================================================
REM  MyCPU C Compiler Wrapper - mycc.bat
REM  Usage: mycc [options] <input.c|input.s|input.o>
REM ============================================================
REM
REM  Standalone toolchain - no LLVM source needed.
REM  Copy the entire dist/ directory to any PC.
REM
REM  Usage on another PC:
REM    set PATH=X:\path\to\dist\bin;%PATH%
REM    mycc test.c -o test.o
REM
REM ============================================================
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "CLANG=%SCRIPT_DIR%clang.exe"
set "LLVM_MC=%SCRIPT_DIR%llvm-mc.exe"
set "LLVM_OBJDUMP=%SCRIPT_DIR%llvm-objdump.exe"
set "LLC=%SCRIPT_DIR%llc.exe"

set "TARGET=mycpu-unknown-elf"
set "BASE_FLAGS=-ffreestanding -include stdint.h -include stddef.h -include stdbool.h"
set "OPT_LEVEL=-O2"
set "MODE=c"
set "OUTPUT="
set "INPUT="
set "ACTION=compile"
set "EXTRA_ARGS="

REM -- Parse arguments -----------------------------------------
:parse_args
if "%~1"=="" goto :done_parsing

if "%~1"=="-O0" ( set "OPT_LEVEL=-O0" & shift & goto :parse_args )
if "%~1"=="-O1" ( set "OPT_LEVEL=-O1" & shift & goto :parse_args )
if "%~1"=="-O2" ( set "OPT_LEVEL=-O2" & shift & goto :parse_args )
if "%~1"=="-Os" ( set "OPT_LEVEL=-Os" & shift & goto :parse_args )
if "%~1"=="-Oz" ( set "OPT_LEVEL=-Oz" & shift & goto :parse_args )
if "%~1"=="-S"  ( set "MODE=S"  & shift & goto :parse_args )
if "%~1"=="-c"  ( set "MODE=c"  & shift & goto :parse_args )
if "%~1"=="-v"  ( set "EXTRA_ARGS=!EXTRA_ARGS! -v" & shift & goto :parse_args )
if "%~1"=="-o"  ( set "OUTPUT=%~2" & shift & shift & goto :parse_args )
if "%~1"=="--asm"     ( set "ACTION=assemble"    & shift & goto :parse_args )
if "%~1"=="--dis"     ( set "ACTION=disassemble" & shift & goto :parse_args )
if "%~1"=="--llc"     ( set "ACTION=llc"         & shift & goto :parse_args )
if "%~1"=="--help"    ( goto :show_help )
if "%~1"=="-h"        ( goto :show_help )
if "%~1"=="--version" ( "%CLANG%" --version & exit /b 0 )

REM first non-option arg = input file
if not defined INPUT (
    set "INPUT=%~1"
    shift
    goto :parse_args
)

REM extra clang args passed through
set "EXTRA_ARGS=!EXTRA_ARGS! %~1"
shift
goto :parse_args

:done_parsing

REM -- Validate input ------------------------------------------
if not defined INPUT (
    echo [mycc] ERROR: No input file. Use mycc --help.
    exit /b 1
)
if not exist "%INPUT%" (
    echo [mycc] ERROR: Input file '%INPUT%' not found.
    exit /b 1
)

REM -- Auto-deduce output name ---------------------------------
if not defined OUTPUT (
    for %%F in ("%INPUT%") do (
        set "BASENAME=%%~nF"
    )
    if "!MODE!"=="S"  ( set "OUTPUT=!BASENAME!.s" )
    if "!MODE!"=="c"  ( set "OUTPUT=!BASENAME!.o" )
)

REM -- Execute action ------------------------------------------

if "!ACTION!"=="assemble" (
    echo [mycc] Assembling !INPUT! -^> !OUTPUT! (target: %TARGET%)
    "%LLVM_MC%" -triple=%TARGET% -filetype=obj -o "!OUTPUT!" "!INPUT!"
    exit /b !errorlevel!
)

if "!ACTION!"=="disassemble" (
    echo [mycc] Disassembling !INPUT!
    "%LLVM_OBJDUMP%" -d --triple=mycpu "!INPUT!"
    exit /b !errorlevel!
)

if "!ACTION!"=="llc" (
    echo [mycc] Compiling LLVM IR !INPUT! -^> !OUTPUT! (%OPT_LEVEL%)
    "%LLC%" -mtriple=%TARGET% %OPT_LEVEL% -filetype=obj -o "!OUTPUT!" "!INPUT!"
    exit /b !errorlevel!
)

REM -- Default: C compile --------------------------------------
echo [mycc] Compiling !INPUT! -^> !OUTPUT! (target: %TARGET%, %OPT_LEVEL%)

if "!MODE!"=="S" (
    "%CLANG%" -target %TARGET% %BASE_FLAGS% %OPT_LEVEL% -S -o "!OUTPUT!" "!INPUT!" !EXTRA_ARGS!
) else (
    "%CLANG%" -target %TARGET% %BASE_FLAGS% %OPT_LEVEL% -c -o "!OUTPUT!" "!INPUT!" !EXTRA_ARGS!
)

if !errorlevel! neq 0 (
    echo [mycc] Compilation FAILED.
    exit /b !errorlevel!
)
echo [mycc] OK - !OUTPUT!
exit /b 0

REM -- Help ----------------------------------------------------
:show_help
echo.
echo   MyCPU C Compiler Toolchain - mycc.bat
echo   =====================================
echo.
echo   Usage: mycc [options] ^<input.c^>
echo.
echo   Compile options:
echo     -O0, -O1, -O2, -Os, -Oz   optimization level (default -O2)
echo     -c                         compile to .o object (default)
echo     -S                         compile to .s assembly
echo     -o ^<file^>                  output filename
echo     -v                         verbose output
echo.
echo   Other tools:
echo     --asm  ^<input.s^>            assemble .s to .o (llvm-mc)
echo     --dis  ^<input.o^>            disassemble .o to machine code
echo     --llc  ^<input.ll^>           compile LLVM IR to .o
echo.
echo   Version:
echo     --version                  show clang version
echo.
echo   Examples:
echo     mycc test.c                # compile to test.o (O2)
echo     mycc -O0 -S test.c         # compile to test.s (assembly)
echo     mycc --asm test.s -o test.o # assemble .s to .o
echo     mycc --dis test.o          # disassemble .o
echo.
echo   System headers:
echo     -ffreestanding, auto-include: stdint.h stddef.h stdbool.h
echo     No need to #include them in source files.
echo.
echo   Package layout:
echo     dist\bin\    clang, llvm-mc, llvm-objdump, llc, mycc
echo     dist\lib\    clang built-in headers
echo     dist\include\ MyCPU headers (mycpu_cop.h)
echo.
exit /b 0
