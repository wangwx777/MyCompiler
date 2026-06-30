//===-- MyCPURegisterInfo.td - MyCPU Register Definition -----*- tablegen -*-===//
//
// Register file definitions for the MyCPU target.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Register types
//===----------------------------------------------------------------------===//

// ★ HWEncoding 是 bits<16> 类型，Enc 必须匹配
class MyCPUReg<bits<16> Enc, string N, list<string> alt = []>
    : Register<N, alt> {
  let HWEncoding = Enc;
  let Namespace = "MyCPU";
}

class MyCPURegClass<list<ValueType> regTypes, dag regList>
    : RegisterClass<"MyCPU", regTypes, 32, regList> {
  let Size = 32;
  let Alignment = 32;
}

//===----------------------------------------------------------------------===//
// General Purpose Registers (GPRs) - 32 registers, 32-bit wide
//===----------------------------------------------------------------------===//

// R0 is hardwired zero
def ZERO : MyCPUReg<0,  "r0",  ["zero"]>;
// Argument / return value registers
def A0   : MyCPUReg<1,  "r1",  ["a0"]>;
def A1   : MyCPUReg<2,  "r2",  ["a1"]>;
def A2   : MyCPUReg<3,  "r3",  ["a2"]>;
def A3   : MyCPUReg<4,  "r4",  ["a3"]>;
def A4   : MyCPUReg<5,  "r5",  ["a4"]>;
def A5   : MyCPUReg<6,  "r6",  ["a5"]>;
def A6   : MyCPUReg<7,  "r7",  ["a6"]>;
// Caller-saved temporaries
def T0   : MyCPUReg<8,  "r8",  ["t0"]>;
def T1   : MyCPUReg<9,  "r9",  ["t1"]>;
def T2   : MyCPUReg<10, "r10", ["t2"]>;
def T3   : MyCPUReg<11, "r11", ["t3"]>;
def T4   : MyCPUReg<12, "r12", ["t4"]>;
def T5   : MyCPUReg<13, "r13", ["t5"]>;
def T6   : MyCPUReg<14, "r14", ["t6"]>;
def T7   : MyCPUReg<15, "r15", ["t7"]>;
// Callee-saved registers
def S0   : MyCPUReg<16, "r16", ["s0"]>;
def S1   : MyCPUReg<17, "r17", ["s1"]>;
def S2   : MyCPUReg<18, "r18", ["s2"]>;
def S3   : MyCPUReg<19, "r19", ["s3"]>;
def S4   : MyCPUReg<20, "r20", ["s4"]>;
def S5   : MyCPUReg<21, "r21", ["s5"]>;
def S6   : MyCPUReg<22, "r22", ["s6"]>;
def S7   : MyCPUReg<23, "r23", ["s7"]>;
// Extended caller-saved temporaries
def T8   : MyCPUReg<24, "r24", ["t8"]>;
def T9   : MyCPUReg<25, "r25", ["t9"]>;
def T10  : MyCPUReg<26, "r26", ["t10"]>;
def T11  : MyCPUReg<27, "r27", ["t11"]>;
// Special registers
def LR   : MyCPUReg<28, "r28", ["lr"]>;
def SP   : MyCPUReg<29, "r29", ["sp"]>;
def FP   : MyCPUReg<30, "r30", ["fp"]>;
def GP   : MyCPUReg<31, "r31", ["gp"]>;

//===----------------------------------------------------------------------===//
// Flag registers (Freg) - 4 entries, each holds Z/C/N/V flags
//===----------------------------------------------------------------------===//

class MyCPUFreg<bits<16> Enc, string N> : Register<N> {
  let HWEncoding = Enc;
  let Namespace = "MyCPU";
}

def F0 : MyCPUFreg<0, "f0">;
def F1 : MyCPUFreg<1, "f1">;
def F2 : MyCPUFreg<2, "f2">;
def F3 : MyCPUFreg<3, "f3">;

//===----------------------------------------------------------------------===//
// Register classes
//===----------------------------------------------------------------------===//

def GPR : MyCPURegClass<[i32, i16, i8], (add
  ZERO,
  A0, A1, A2, A3, A4, A5, A6,
  T0, T1, T2, T3, T4, T5, T6, T7,
  S0, S1, S2, S3, S4, S5, S6, S7,
  T8, T9, T10, T11,
  LR, SP, FP, GP
)>;

def FREG : RegisterClass<"MyCPU", [i32], 32, (add F0, F1, F2, F3)> {
  let Size = 8;  // 8 bits per Freg: Z(0)/C(1)/N(2)/V(3), bits[7:4] reserved
}

// ★ 将标准伪指令中的 ptr_rc 操作数映射为 GPR
defm : RemapAllTargetPseudoPointerOperands<GPR>;
