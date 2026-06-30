; MyCPU ISA simulator test suite (v2 - verified mnemonics)
; Each test: expected value in r4, cmp+branch to check.
; All tests pass → r1=0, halt. Any fail → r1=1, halt.

.text
.globl _start
_start:

; ── Test 1: ADD.w ────────────────────────────────
    movi.w  r1, 5
    movi.w  r2, 3
    add.w   r1, r2[31:0]
    movi.w  r4, 8
    cmp.w   r4, r1[31:0]
    bnz     .Lfail

; ── Test 2: SUB.w ────────────────────────────────
    movi.w  r1, 100
    movi.w  r2, 33
    sub.w   r1, r2[31:0]
    movi.w  r4, 67
    cmp.w   r4, r1[31:0]
    bnz     .Lfail

; ── Test 3: AND.w ────────────────────────────────
    movi.w  r1, 255
    movi.w  r2, 15
    and.w   r1, r2[31:0]
    movi.w  r4, 15
    cmp.w   r4, r1[31:0]
    bnz     .Lfail

; ── Test 4: OR.w ─────────────────────────────────
    movi.w  r1, 240
    movi.w  r2, 15
    or.w    r1, r2[31:0]
    movi.w  r4, 255
    cmp.w   r4, r1[31:0]
    bnz     .Lfail

; ── Test 5: XOR.w ────────────────────────────────
    movi.w  r1, 255
    movi.w  r2, 15
    xor.w   r1, r2[31:0]
    movi.w  r4, 240
    cmp.w   r4, r1[31:0]
    bnz     .Lfail

; ── Test 6: SHLI.w + SHRI.w ──────────────────────
    movi.w  r1, 1
    shli.w  r1, r1, 4
    movi.w  r4, 16
    cmp.w   r4, r1[31:0]
    bnz     .Lfail
    shri.w  r1, r1, 2
    movi.w  r4, 4
    cmp.w   r4, r1[31:0]
    bnz     .Lfail

; ── Test 7: BSET + BTST ──────────────────────────
    movi.w  r1, 0
    bset    r1, 7
    movi.w  r4, 128
    cmp.w   r4, r1[31:0]
    bnz     .Lfail
    btst    r1, 7, 0
    bz      .Lfail

; ── Test 8: mov.w store/load ─────────────────────
    movi.w  r1, 12345
    movi.w  r2, 0x1000
    mov.w   [r2 + 0], r1
    movi.w  r1, 0
    mov.w   r1, [r2 + 0]
    movi.w  r4, 12345
    cmp.w   r4, r1[31:0]
    bnz     .Lfail

; ── Test 9: COP ld.w / st.w ──────────────────────
    movi.w  r1, 9999
    st.w    r1, 3, 16
    movi.w  r1, 0
    ld.w    r1, 3, 16
    movi.w  r4, 9999
    cmp.w   r4, r1[31:0]
    bnz     .Lfail

; ── Test 10: CALL / RET ──────────────────────────
    call    .Lcallee
    movi.w  r4, 99
    cmp.w   r4, r1[31:0]
    bnz     .Lfail
    jmp     .Lpass

.Lcallee:
    movi.w  r1, 99
    ret

.Lfail:
    movi.w  r1, 1
    halt
.Lpass:
    movi.w  r1, 0
    halt
