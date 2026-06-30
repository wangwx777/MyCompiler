/*
 * mycpu-sim.c — MyCPU Instruction Set Simulator (v2, 2026-07-01)
 *
 * Functional-level simulator. Reads ELF .o or raw hex.
 * Verified field layouts against LLVM MyCPUBaseInfo.h (2026-07-01).
 *
 * Build:  cc -O2 -o mycpu-sim mycpu-sim.c
 * Usage:  ./mycpu-sim <file.o> [--dump-regs] [--max-steps N]
 *         ./mycpu-sim --hex 0xNNNN --regs r1=5,r2=3 --max-steps 1
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * Configuration
 * ======================================================================== */

#define MEM_SIZE       (1 * 1024 * 1024)
#define MAX_COPROC      64
#define COP_ADDR_SPACE  1024
#define MAX_STEPS_DEFAULT  1000000

/* ========================================================================
 * Register names
 * ======================================================================== */

#define REG_COUNT  32
static const char *reg_names[] = {
    "r0(zero)", "r1(a0)", "r2(a1)", "r3(a2)", "r4(a3)", "r5(a4)", "r6(a5)", "r7(a6)",
    "r8(t0)",  "r9(t1)","r10(t2)","r11(t3)","r12(t4)","r13(t5)","r14(t6)","r15(t7)",
    "r16(s0)","r17(s1)","r18(s2)","r19(s3)","r20(s4)","r21(s5)","r22(s6)","r23(s7)",
    "r24(t8)","r25(t9)","r26(t10)","r27(t11)","r28(lr)","r29(sp)","r30(fp)","r31(gp)"
};

/* ========================================================================
 * CPU state
 * ======================================================================== */

typedef struct {
    uint32_t gpr[REG_COUNT];
    uint8_t  freg[4];
    uint32_t pc;
    bool     halted;
} cpu_state_t;

#define FLAG_Z  0
#define FLAG_C  1
#define FLAG_N  2
#define FLAG_V  3

static uint8_t  memory[MEM_SIZE];
static uint32_t coproc[MAX_COPROC][COP_ADDR_SPACE];
static cpu_state_t cpu;

/* ========================================================================
 * Utility: width/mask helpers
 * ======================================================================== */

static uint32_t width_mask(uint32_t sz) {
    switch (sz) { case 0: return 0xFF; case 1: return 0xFFFF; case 2: default: return 0xFFFFFFFF; }
}
static uint32_t width_bits(uint32_t sz) {
    switch (sz) { case 0: return 8; case 1: return 16; case 2: default: return 32; }
}
static uint32_t slice_mask(uint32_t msb, uint32_t lsb) {
    uint32_t w = msb - lsb + 1;
    return (w >= 32) ? 0xFFFFFFFF : ((1u << w) - 1);
}

/* ========================================================================
 * Flag helpers
 * ======================================================================== */

static void set_flags(uint32_t f_idx, uint32_t res, uint32_t w) {
    if (f_idx > 3) return;
    uint32_t m = ((uint64_t)1 << w) - 1;
    res &= m;
    cpu.freg[f_idx] = 0;
    if ((res & m) == 0)          cpu.freg[f_idx] |= (1 << FLAG_Z);
    if (res & (1u << (w - 1)))   cpu.freg[f_idx] |= (1 << FLAG_N);
}

static void set_flag_carry(uint32_t f_idx, bool c) {
    if (f_idx > 3) return;
    if (c) cpu.freg[f_idx] |= (1 << FLAG_C);
    else   cpu.freg[f_idx] &= ~(1u << FLAG_C);
}

static bool carry_out(uint64_t a, uint64_t b, uint64_t cin, uint32_t w) {
    return ((a + b + cin) >> w) & 1;
}

static bool overflow_add(uint32_t a, uint32_t b, uint32_t w) {
    uint32_t s = 1u << (w - 1);
    uint32_t sa = a & s, sb = b & s, sr = (a + b) & ((1u << w) - 1);
    return (sa == sb) && ((sr & s) != sa);
}

static bool overflow_sub(uint32_t a, uint32_t b, uint32_t w) {
    uint32_t s = 1u << (w - 1);
    uint32_t sa = a & s, sb = b & s, sr = (a - b) & ((1u << w) - 1);
    return (sa != sb) && ((sr & s) != sa);
}

/* ========================================================================
 * Memory helpers (little-endian)
 * ======================================================================== */

static bool chk_addr(uint32_t a, uint32_t sz) {
    if (a + sz > MEM_SIZE) { cpu.halted = true; return false; }
    return true;
}
static uint32_t mem_read(uint32_t a, uint32_t sz /* 0=B 1=H 2=W */) {
    uint32_t n = sz == 2 ? 4 : sz == 1 ? 2 : 1;
    if (!chk_addr(a, n)) return 0;
    uint32_t v = memory[a] | ((uint32_t)memory[a+1] << 8);
    if (n >= 2) v |= (uint32_t)memory[a+2] << 16;
    if (n >= 4) v |= (uint32_t)memory[a+3] << 24;
    return v;
}
static void mem_write(uint32_t a, uint32_t v, uint32_t sz) {
    uint32_t n = sz == 2 ? 4 : sz == 1 ? 2 : 1;
    if (!chk_addr(a, n)) return;
    memory[a] = v & 0xFF; memory[a+1] = (v >> 8) & 0xFF;
    if (n >= 2) { memory[a+2] = (v >> 16) & 0xFF; memory[a+3] = (v >> 24) & 0xFF; }
}

/* ========================================================================
 * Decoder — field extraction verified against MyCPUBaseInfo.h
 *
 * Word(size=2): Fa[31:27]5b Fb[26:22]5b Fc[21:17]5b f[16:14]3b c[13]1b rd[12:8]5b
 * Half(size=1): Fa[31:26]6b Fb[25:22]4b Fc[21:18]4b f[17:15]3b c[14]1b   rd[13:8]6b
 * Byte(size=0): Fa[31:25]7b Fb[24:22]3b Fc[21:19]3b f[18:16]3b c[15]1b   rd[14:8]7b
 * ======================================================================== */

typedef struct {
    uint32_t raw;
    uint32_t opcode;   /* bits[5:0] */
    uint32_t size;     /* bits[7:6] */
    uint32_t fa;       /* fieldA */
    uint32_t fb;       /* fieldB */
    uint32_t fc;       /* fieldC */
    uint32_t f_field;  /* flag register index */
    uint32_t c_bit;    /* carry / R/W */
    uint32_t rd;       /* destination register */
} decoded_t;

static decoded_t decode(uint32_t instr) {
    decoded_t d;
    d.raw = instr;
    d.opcode = (instr >> 0) & 0x3F;
    d.size   = (instr >> 6) & 0x3;
    switch (d.size) {
    case 2: /* Word */
        d.fa = (instr >> 27) & 0x1F;
        d.fb = (instr >> 22) & 0x1F;
        d.fc = (instr >> 17) & 0x1F;
        d.f_field = (instr >> 14) & 0x7;
        d.c_bit   = (instr >> 13) & 0x1;
        d.rd      = (instr >> 8)  & 0x1F;
        break;
    case 1: /* Half */
        d.fa = (instr >> 26) & 0x3F;
        d.fb = (instr >> 22) & 0x0F;
        d.fc = (instr >> 18) & 0x0F;
        d.f_field = (instr >> 15) & 0x7;
        d.c_bit   = (instr >> 14) & 0x1;
        d.rd      = (instr >> 8)  & 0x3F;
        break;
    case 0: /* Byte */
    default:
        d.fa = (instr >> 25) & 0x7F;
        d.fb = (instr >> 22) & 0x07;
        d.fc = (instr >> 19) & 0x07;
        d.f_field = (instr >> 16) & 0x7;
        d.c_bit   = (instr >> 15) & 0x1;
        d.rd      = (instr >> 8)  & 0x7F;
        break;
    }
    return d;
}

/* ========================================================================
 * Compute 15-bit immediate from Fa+Fb+Fc (for RRI / BR instructions)
 * ======================================================================== */
static uint32_t get_imm15(const decoded_t *d) {
    return (d->fa << 10) | (d->fb << 5) | d->fc;
}

/* ========================================================================
 * Compute branch target from 15-bit imm + 2 (backend fixup compensation)
 * ======================================================================== */
static uint32_t branch_target(const decoded_t *d) {
    return get_imm15(d) + 2;
}

/* ========================================================================
 * Compute offset for MOV/COP (width-dependent)
 * ======================================================================== */
static uint32_t make_offset(uint32_t hi, uint32_t lo, uint32_t sz) {
    switch (sz) {
    case 2: return (hi << 5) | lo;
    case 1: return (hi << 4) | lo;
    case 0: return (hi << 3) | lo;
    default: return 0;
    }
}

/* ========================================================================
 * Execute one decoded instruction
 * ======================================================================== */

static bool execute(const decoded_t *d)
{
    uint32_t w    = width_bits(d->size);
    uint32_t mask = width_mask(d->size);
    uint32_t rd   = d->rd;
    uint32_t rs   = d->fa;
    uint32_t f_i  = d->f_field;
    uint32_t res  = 0;

    uint32_t rd_val = (rd == 0) ? 0 : cpu.gpr[rd];
    uint32_t rs_val = (rs == 0) ? 0 : cpu.gpr[rs];

    switch (d->opcode) {

    /* ── 0x00 ADD  ──────────────────────────────────────────────── */
    case 0x00: {
        uint32_t msb = d->fb, lsb = d->fc;
        uint32_t sl = (rs_val >> lsb) & slice_mask(msb, lsb);
        res = (rd_val + sl) & mask;
        set_flags(f_i, res, w);
        set_flag_carry(f_i, carry_out(rd_val, sl, 0, w));
        if (rd != 0) cpu.gpr[rd] = (cpu.gpr[rd] & ~mask) | res;
        break;
    }

    /* ── 0x02 SUB  ──────────────────────────────────────────────── */
    case 0x02: {
        uint32_t msb = d->fb, lsb = d->fc;
        uint32_t sl = (rs_val >> lsb) & slice_mask(msb, lsb);
        res = (rd_val - sl) & mask;
        set_flags(f_i, res, w);
        set_flag_carry(f_i, rd_val < sl);
        if (rd != 0) cpu.gpr[rd] = (cpu.gpr[rd] & ~mask) | res;
        break;
    }

    /* ── 0x04 AND  ──────────────────────────────────────────────── */
    case 0x04: {
        uint32_t msb = d->fb, lsb = d->fc;
        uint32_t sl = (rs_val >> lsb) & slice_mask(msb, lsb);
        res = (rd_val & sl) & mask;
        set_flags(f_i, res, w);
        if (rd != 0) cpu.gpr[rd] = (cpu.gpr[rd] & ~mask) | res;
        break;
    }

    /* ── 0x06 OR   ──────────────────────────────────────────────── */
    case 0x06: {
        uint32_t msb = d->fb, lsb = d->fc;
        uint32_t sl = (rs_val >> lsb) & slice_mask(msb, lsb);
        res = (rd_val | sl) & mask;
        set_flags(f_i, res, w);
        if (rd != 0) cpu.gpr[rd] = (cpu.gpr[rd] & ~mask) | res;
        break;
    }

    /* ── 0x08 XOR  ──────────────────────────────────────────────── */
    case 0x08: {
        uint32_t msb = d->fb, lsb = d->fc;
        uint32_t sl = (rs_val >> lsb) & slice_mask(msb, lsb);
        res = (rd_val ^ sl) & mask;
        set_flags(f_i, res, w);
        if (rd != 0) cpu.gpr[rd] = (cpu.gpr[rd] & ~mask) | res;
        break;
    }

    /* ── 0x0A NOT  ──────────────────────────────────────────────── */
    case 0x0A: {
        uint32_t msb = d->fb, lsb = d->fc;
        uint32_t sl = (rs_val >> lsb) & slice_mask(msb, lsb);
        res = (~sl) & mask;
        set_flags(f_i, res, w);
        if (rd != 0) cpu.gpr[rd] = (cpu.gpr[rd] & ~mask) | res;
        break;
    }

    /* ── 0x16 CMP  ──────────────────────────────────────────────── */
    case 0x16: {
        uint32_t msb = d->fb, lsb = d->fc;
        uint32_t sl = (rs_val >> lsb) & slice_mask(msb, lsb);
        uint32_t diff = (rd_val - sl) & mask;
        set_flags(f_i, diff, w);
        set_flag_carry(f_i, rd_val < sl);
        break;
    }

    /* ── RR Immediate (0x01 ADDI, 0x03 SUBI, 0x05 ANDI, 0x07 ORI,
     *    0x09 XORI, 0x13 MOVI) ──────────────────────────────────── */
    case 0x01: case 0x03: case 0x05: case 0x07: case 0x09: case 0x13: {
        uint32_t imm = get_imm15(d);
        switch (d->opcode) {
        case 0x01: res = (rd_val + imm) & mask; break;
        case 0x03: res = (rd_val - imm) & mask; break;
        case 0x05: res = (rd_val & imm) & mask; break;
        case 0x07: res = (rd_val | imm) & mask; break;
        case 0x09: res = (rd_val ^ imm) & mask; break;
        case 0x13: res = imm & 0x7FFF; break;
        }
        if (d->opcode != 0x13) {
            set_flags(f_i, res, w);
            if (rd != 0) cpu.gpr[rd] = (cpu.gpr[rd] & ~mask) | res;
        } else {
            if (rd != 0) cpu.gpr[rd] = res;
        }
        break;
    }

    /* ── 0x12: reserved (old COP, now unused) ───────────────────── */
    case 0x12:
        break;

    /* ── 0x14 COP LD ──────────────────────────────────────────────
     *   cop_id = fa, addr = (fb << w) | fc, c_bit:0=RD
     * ────────────────────────────────────────────────────────────── */
    case 0x14: {
        uint32_t cop_id = d->fa;
        uint32_t addr = make_offset(d->fb, d->fc, d->size);
        if (cop_id < MAX_COPROC && addr < COP_ADDR_SPACE) {
            res = coproc[cop_id][addr];
            if (rd != 0) {
                if (d->size == 2) cpu.gpr[rd] = res;
                else cpu.gpr[rd] = (cpu.gpr[rd] & ~mask) | res;
            }
        }
        break;
    }

    /* ── 0x15 COP ST ──────────────────────────────────────────────
     *   cop_id = fa, addr = (fb << w) | fc, src = rd
     * ────────────────────────────────────────────────────────────── */
    case 0x15: {
        uint32_t cop_id = d->fa;
        uint32_t addr = make_offset(d->fb, d->fc, d->size);
        if (cop_id < MAX_COPROC && addr < COP_ADDR_SPACE) {
            coproc[cop_id][addr] = (rd == 0) ? 0 : (cpu.gpr[rd] & mask);
        }
        break;
    }

    /* ── 0x0C SHL, 0x0E SHR, 0x10 SAR (reg) ────────────────────── */
    case 0x0C: case 0x0E: case 0x10: {
        uint32_t sh = rs_val & (w - 1);
        uint32_t sv = rd_val & mask;
        switch (d->opcode) {
        case 0x0C: res = (sv << sh) & mask; break;
        case 0x0E: res = sv >> sh; break;
        case 0x10:
            if (sv & (1u << (w - 1))) res = ((sv >> sh) | (~0u << (w - sh))) & mask;
            else res = sv >> sh;
            break;
        }
        set_flags(f_i, res, w);
        if (rd != 0) cpu.gpr[rd] = (cpu.gpr[rd] & ~mask) | res;
        break;
    }

    /* ── 0x0D SHLI, 0x0F SHRI, 0x11 SARI (imm) ────────────────── */
    case 0x0D: case 0x0F: case 0x11: {
        uint32_t sh = d->fa & (d->size == 2 ? 0x1F : (w - 1));
        uint32_t sv = rd_val & mask;
        switch (d->opcode) {
        case 0x0D: res = (sv << sh) & mask; break;
        case 0x0F: res = sv >> sh; break;
        case 0x11:
            if (sv & (1u << (w - 1))) res = ((sv >> sh) | (~0u << (w - sh))) & mask;
            else res = sv >> sh;
            break;
        }
        set_flags(f_i, res, w);
        if (rd != 0) cpu.gpr[rd] = (cpu.gpr[rd] & ~mask) | res;
        break;
    }

    /* ── 0x22 BSET, 0x23 BCLR, 0x24 BNOT, 0x25 BTST ────────────── */
    case 0x22: case 0x23: case 0x24: case 0x25: {
        uint32_t bp = d->fa & (w - 1);
        uint32_t bm = 1u << bp;
        switch (d->opcode) {
        case 0x22: res = (rd_val | bm) & mask; break;
        case 0x23: res = (rd_val & ~bm) & mask; break;
        case 0x24: res = (rd_val ^ bm) & mask; break;
        case 0x25: /* BTST: flags only */
            cpu.freg[f_i] = 0;
            if ((rd_val & bm) == 0) cpu.freg[f_i] |= (1 << FLAG_Z);
            break;
        }
        if (d->opcode != 0x25) {
            set_flags(f_i, res, w);
            if (rd != 0) cpu.gpr[rd] = (cpu.gpr[rd] & ~mask) | res;
        }
        break;
    }

    /* ── 0x28 MOV (ld / st / reg move) ────────────────────────────
     *   c_bit=0: LOAD  → rd = mem[Fa + off], off = (Fb<<w)|Fc
     *   c_bit=1: STORE → mem[Fb + off] = Fa, off = (rd<<w)|Fc
     * ────────────────────────────────────────────────────────────── */
    case 0x28: {
        if (d->c_bit == 1) {
            uint32_t sv = (d->fa == 0) ? 0 : cpu.gpr[d->fa];
            uint32_t bs = (d->fb == 0) ? 0 : cpu.gpr[d->fb];
            uint32_t off = make_offset(d->rd, d->fc, d->size);
            mem_write(bs + off, sv, d->size);
        } else {
            uint32_t bs = (d->fa == 0) ? 0 : cpu.gpr[d->fa];
            uint32_t off = make_offset(d->fb, d->fc, d->size);
            res = mem_read(bs + off, d->size);
            if (rd != 0) {
                if (d->size == 2) cpu.gpr[rd] = res;
                else cpu.gpr[rd] = (cpu.gpr[rd] & ~mask) | res;
            }
        }
        break;
    }

    /* ── 0x18 JMP  ──────────────────────────────────────────────── */
    case 0x18:
        cpu.pc = branch_target(d);
        break;

    /* ── 0x19 JALR ──────────────────────────────────────────────── */
    case 0x19:
        cpu.gpr[28] = cpu.pc;
        cpu.pc = rs_val;
        break;

    /* ── 0x1A BZ  ───────────────────────────────────────────────── */
    case 0x1A:
        if (cpu.freg[f_i] & (1 << FLAG_Z))
            cpu.pc = branch_target(d);
        break;

    /* ── 0x1B BNZ ───────────────────────────────────────────────── */
    case 0x1B:
        if (!(cpu.freg[f_i] & (1 << FLAG_Z)))
            cpu.pc = branch_target(d);
        break;

    /* ── 0x1E CALL ──────────────────────────────────────────────── */
    case 0x1E:
        cpu.gpr[28] = cpu.pc;
        cpu.pc = branch_target(d);
        break;

    /* ── 0x1F RET  ──────────────────────────────────────────────── */
    case 0x1F:
        cpu.pc = cpu.gpr[28];
        break;

    /* ── 0x26 NOP  ──────────────────────────────────────────────── */
    case 0x26:
        break;

    /* ── 0x27 HALT ──────────────────────────────────────────────── */
    case 0x27:
        cpu.halted = true;
        break;

    /* ── Unknown ────────────────────────────────────────────────── */
    default:
        fprintf(stderr, "sim: unimpl opcode 0x%02x at PC=0x%08x\n",
                d->opcode, cpu.pc);
        cpu.halted = true;
        break;
    }

    return !cpu.halted;
}

/* ========================================================================
 * CPU init + main loop
 * ======================================================================== */

static void cpu_reset(void) {
    memset(&cpu, 0, sizeof(cpu));
    memset(memory, 0, sizeof(memory));
    memset(coproc, 0, sizeof(coproc));
    cpu.gpr[29] = MEM_SIZE;
}

static int run(uint32_t start_pc, uint32_t max_steps) {
    cpu.pc = start_pc;
    uint32_t steps = 0;
    while (!cpu.halted && steps < max_steps) {
        uint32_t raw = mem_read(cpu.pc, 2);
        if (cpu.halted) break;
        decoded_t d = decode(raw);
        cpu.pc += 4;
        if (!execute(&d)) break;
        steps++;
    }
    if (steps >= max_steps) {
        fprintf(stderr, "sim: max steps (%u) exceeded\n", max_steps);
        return -1;
    }
    return 0;
}

/* ========================================================================
 * ELF loader (minimal, reads .text section + symbols)
 * ======================================================================== */

typedef uint32_t Elf32_Addr, Elf32_Off, Elf32_Word;
typedef uint16_t Elf32_Half;
typedef int32_t  Elf32_Sword;

#define EI_NIDENT 16

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf32_Half    e_type, e_machine, e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff, e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    Elf32_Word sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info, sh_addralign, sh_entsize;
} Elf32_Shdr;

typedef struct {
    Elf32_Word st_name, st_value, st_size;
    unsigned char st_info, st_other;
    Elf32_Half st_shndx;
} Elf32_Sym;

#define ELF32_ST_TYPE(info) ((info) & 0xf)

static int load_elf(const char *filename, uint32_t *entry_out) {
    FILE *f = fopen(filename, "rb");
    if (!f) { fprintf(stderr, "sim: cannot open %s\n", filename); return -1; }

    Elf32_Ehdr ehdr;
    if (fread(&ehdr, sizeof(ehdr), 1, f) != 1 || memcmp(ehdr.e_ident, "\x7f""ELF", 4) != 0) {
        fprintf(stderr, "sim: not an ELF file\n"); fclose(f); return -1;
    }

    size_t shdr_sz = ehdr.e_shentsize * ehdr.e_shnum;
    Elf32_Shdr *shdrs = (Elf32_Shdr*)malloc(shdr_sz);
    fseek(f, ehdr.e_shoff, SEEK_SET);
    fread(shdrs, shdr_sz, 1, f);

    Elf32_Shdr *shstr = &shdrs[ehdr.e_shstrndx];
    char *shstrtab = (char*)malloc(shstr->sh_size);
    fseek(f, shstr->sh_offset, SEEK_SET);
    fread(shstrtab, shstr->sh_size, 1, f);

    char *strtab = NULL;
    Elf32_Sym *syms = NULL;
    uint32_t nsyms = 0;
    *entry_out = 0;

    for (int i = 0; i < ehdr.e_shnum; i++) {
        Elf32_Shdr *sh = &shdrs[i];
        const char *name = shstrtab + sh->sh_name;

        if (sh->sh_type == 1 /* SHT_PROGBITS */ && strcmp(name, ".text") == 0) {
            if (sh->sh_size > MEM_SIZE) { free(shstrtab); free(shdrs); fclose(f); return -1; }
            fseek(f, sh->sh_offset, SEEK_SET);
            fread(memory, sh->sh_size, 1, f);
            *entry_out = sh->sh_addr;
        } else if (sh->sh_type == 2 /* SHT_SYMTAB */) {
            syms = (Elf32_Sym*)malloc(sh->sh_size);
            nsyms = sh->sh_size / sizeof(Elf32_Sym);
            fseek(f, sh->sh_offset, SEEK_SET);
            fread(syms, sh->sh_size, 1, f);
        }
    }

    for (int i = 0; i < ehdr.e_shnum; i++) {
        if (shdrs[i].sh_type == 3) {
            const char *n = shstrtab + shdrs[i].sh_name;
            if (strcmp(n, ".strtab") == 0) {
                strtab = (char*)malloc(shdrs[i].sh_size);
                fseek(f, shdrs[i].sh_offset, SEEK_SET);
                fread(strtab, shdrs[i].sh_size, 1, f);
                break;
            }
        }
    }

    if (syms && strtab && *entry_out == 0) {
        for (uint32_t i = 0; i < nsyms; i++) {
            if (ELF32_ST_TYPE(syms[i].st_info) == 2 /* STT_FUNC */ && syms[i].st_value != 0) {
                *entry_out = syms[i].st_value;
                break;
            }
        }
    }

    free(strtab); free(syms); free(shstrtab); free(shdrs); fclose(f);
    return 0;
}

/* ========================================================================
 * CLI
 * ======================================================================== */

static void dump_regs(void) {
    printf("\n=== Register Dump ===\n");
    for (int i = 0; i < 32; i++) {
        printf("  %-12s = 0x%08x  %u\n", reg_names[i], cpu.gpr[i], cpu.gpr[i]);
        if (i % 8 == 7) printf("\n");
    }
    printf("  Flags: F0=%02x F1=%02x F2=%02x F3=%02x\n",
           cpu.freg[0], cpu.freg[1], cpu.freg[2], cpu.freg[3]);
    printf("  PC = 0x%08x  Halted = %d\n", cpu.pc, cpu.halted);
}

static void dump_mem(uint32_t addr, uint32_t len) {
    printf("\n=== Memory Dump [0x%08x : 0x%08x] ===\n", addr, addr + len - 1);
    for (uint32_t i = 0; i < len; i += 16) {
        printf("  %08x: ", addr + i);
        for (uint32_t j = 0; j < 16 && (i + j) < len; j++)
            printf("%02x ", memory[addr + i + j]);
        printf("\n");
    }
}

static void print_usage(const char *prog) {
    printf("MyCPU ISA Simulator v2\n"
           "Usage: %s <file.o> [--dump-regs] [--max-steps N] [--dump-mem A:L]\n"
           "       %s --hex 0xNNNN [--regs rN=V] [--cop-in C:A=V] [--max-steps 1]\n",
           prog, prog);
}

int main(int argc, char **argv) {
    const char *filename = NULL;
    uint32_t max_steps = MAX_STEPS_DEFAULT, dump_addr = 0, dump_len = 0, start_pc = 0;
    bool dump_flag = false, raw_hex = false;
    uint32_t hex_word = 0;

    cpu_reset();

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))
            { print_usage(argv[0]); return 0; }
        else if (!strcmp(argv[i], "--max-steps") && i + 1 < argc)
            max_steps = (uint32_t)strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--dump-regs"))
            dump_flag = true;
        else if (!strcmp(argv[i], "--dump-mem") && i + 1 < argc) {
            char *c = strchr(argv[++i], ':');
            if (c) { *c = '\0'; dump_addr = (uint32_t)strtoul(argv[i], NULL, 0);
                     dump_len = (uint32_t)strtoul(c + 1, NULL, 0); }
        } else if (!strcmp(argv[i], "--hex") && i + 1 < argc) {
            raw_hex = true;
            hex_word = (uint32_t)strtoull(argv[++i], NULL, 0);
        } else if (!strcmp(argv[i], "--regs") && i + 1 < argc) {
            char *rs = argv[++i], *t = strtok(rs, ",");
            while (t) { int r; unsigned v;
                if (sscanf(t, "r%d=%u", &r, &v) == 2 && r >= 0 && r < 32) cpu.gpr[r] = v;
                t = strtok(NULL, ","); }
        } else if (!strcmp(argv[i], "--cop-in") && i + 1 < argc) {
            unsigned c, a, v;
            if (sscanf(argv[++i], "%u:%u=%u", &c, &a, &v) == 3 && c < MAX_COPROC && a < COP_ADDR_SPACE)
                coproc[c][a] = v;
        } else if (!strcmp(argv[i], "--start") && i + 1 < argc)
            start_pc = (uint32_t)strtoul(argv[++i], NULL, 0);
        else if (!filename && argv[i][0] != '-')
            filename = argv[i];
    }

    if (raw_hex) {
        mem_write(0, hex_word, 2);
    } else if (filename) {
        if (load_elf(filename, &start_pc) != 0) return 1;
    } else { print_usage(argv[0]); return 1; }

    printf("[sim] Starting at PC=0x%08x, max_steps=%u\n", start_pc, max_steps);
    int rc = run(start_pc, max_steps);
    if (rc == 0) printf("[sim] Halted. Return value (r1) = %u (0x%x)\n", cpu.gpr[1], cpu.gpr[1]);
    if (dump_flag) dump_regs();
    if (dump_len > 0) dump_mem(dump_addr, dump_len);
    return rc;
}
