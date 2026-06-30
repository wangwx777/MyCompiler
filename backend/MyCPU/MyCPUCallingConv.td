//===-- MyCPUCallingConv.td - MyCPU Calling Conventions ------*- tablegen -*-===//
//
// Calling convention definitions for the MyCPU target.
// The actual argument marshalling is handled in C++ (MyCPUISelLowering.cpp).
// This file defines callee-saved register lists.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Callee-saved register lists
//===----------------------------------------------------------------------===//

// Standard callee-saved registers: S0-S7, FP, plus LR if function calls others
def CSR_MyCPU : CalleeSavedRegs<(add S0, S1, S2, S3, S4, S5, S6, S7, FP)>;

// With link register (for non-leaf functions)
def CSR_MyCPU_LR : CalleeSavedRegs<(add CSR_MyCPU, LR)>;

// No registers saved (leaf function without stack)
def CSR_NoRegs : CalleeSavedRegs<(add)>;
