	.file	"test_mycpu_core.c"
	.text
	.globl	add                             ; -- Begin function add
	.type	add,@function
add:                                    ; @add
; %bb.0:
	subi.w	r29, r29, 8
	st.w	r1, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r1, [r29 + -8]
	ld.w	r2, [r29 + -16]
	add.w	r1, r2[31:0]
	addi.w	r29, r29, 8
	ret
.Lfunc_end0:
	.size	add, .Lfunc_end0-add
                                        ; -- End function
	.globl	sub                             ; -- Begin function sub
	.type	sub,@function
sub:                                    ; @sub
; %bb.0:
	subi.w	r29, r29, 8
	st.w	r1, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r1, [r29 + -8]
	ld.w	r2, [r29 + -16]
	sub.w	r1, r2[31:0]
	addi.w	r29, r29, 8
	ret
.Lfunc_end1:
	.size	sub, .Lfunc_end1-sub
                                        ; -- End function
	.globl	bit_and                         ; -- Begin function bit_and
	.type	bit_and,@function
bit_and:                                ; @bit_and
; %bb.0:
	subi.w	r29, r29, 8
	st.w	r1, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r1, [r29 + -8]
	ld.w	r2, [r29 + -16]
	and.w	r1, r2[31:0]
	addi.w	r29, r29, 8
	ret
.Lfunc_end2:
	.size	bit_and, .Lfunc_end2-bit_and
                                        ; -- End function
	.globl	bit_or                          ; -- Begin function bit_or
	.type	bit_or,@function
bit_or:                                 ; @bit_or
; %bb.0:
	subi.w	r29, r29, 8
	st.w	r1, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r1, [r29 + -8]
	ld.w	r2, [r29 + -16]
	or.w	r1, r2[31:0]
	addi.w	r29, r29, 8
	ret
.Lfunc_end3:
	.size	bit_or, .Lfunc_end3-bit_or
                                        ; -- End function
	.globl	bit_xor                         ; -- Begin function bit_xor
	.type	bit_xor,@function
bit_xor:                                ; @bit_xor
; %bb.0:
	subi.w	r29, r29, 8
	st.w	r1, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r1, [r29 + -8]
	ld.w	r2, [r29 + -16]
	xor.w	r1, r2[31:0]
	addi.w	r29, r29, 8
	ret
.Lfunc_end4:
	.size	bit_xor, .Lfunc_end4-bit_xor
                                        ; -- End function
	.globl	shift_left                      ; -- Begin function shift_left
	.type	shift_left,@function
shift_left:                             ; @shift_left
; %bb.0:
	subi.w	r29, r29, 8
	st.w	r1, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r1, [r29 + -8]
	ld.w	r2, [r29 + -16]
	shl.w	r1, r1, r2
	addi.w	r29, r29, 8
	ret
.Lfunc_end5:
	.size	shift_left, .Lfunc_end5-shift_left
                                        ; -- End function
	.globl	shift_right                     ; -- Begin function shift_right
	.type	shift_right,@function
shift_right:                            ; @shift_right
; %bb.0:
	subi.w	r29, r29, 8
	st.w	r1, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r1, [r29 + -8]
	ld.w	r2, [r29 + -16]
	sar.w	r1, r1, r2
	addi.w	r29, r29, 8
	ret
.Lfunc_end6:
	.size	shift_right, .Lfunc_end6-shift_right
                                        ; -- End function
	.globl	max                             ; -- Begin function max
	.type	max,@function
max:                                    ; @max
; %bb.0:
	subi.w	r29, r29, 12
	st.w	r1, [r29 + -16]
	st.w	r2, [r29 + -24]
	ld.w	r1, [r29 + -16]
	ld.w	r2, [r29 + -24]
	cmp.w	r1, r2[31:0], 0
	bnz	0, .LBB7_2
	jmp	.LBB7_1
.LBB7_1:
	ld.w	r1, [r29 + -16]
	st.w	r1, [r29 + -8]
	jmp	.LBB7_3
.LBB7_2:
	ld.w	r1, [r29 + -24]
	st.w	r1, [r29 + -8]
	jmp	.LBB7_3
.LBB7_3:
	ld.w	r1, [r29 + -8]
	addi.w	r29, r29, 12
	ret
.Lfunc_end7:
	.size	max, .Lfunc_end7-max
                                        ; -- End function
	.globl	abs_val                         ; -- Begin function abs_val
	.type	abs_val,@function
abs_val:                                ; @abs_val
; %bb.0:
	subi.w	r29, r29, 8
	st.w	r1, [r29 + -16]
	ld.w	r1, [r29 + -16]
	movi.w	r2, -1
	cmp.w	r1, r2[31:0], 0
	bnz	0, .LBB8_2
	jmp	.LBB8_1
.LBB8_1:
	ld.w	r2, [r29 + -16]
	movi.w	r1, 0
	sub.w	r1, r2[31:0]
	st.w	r1, [r29 + -8]
	jmp	.LBB8_3
.LBB8_2:
	ld.w	r1, [r29 + -16]
	st.w	r1, [r29 + -8]
	jmp	.LBB8_3
.LBB8_3:
	ld.w	r1, [r29 + -8]
	addi.w	r29, r29, 8
	ret
.Lfunc_end8:
	.size	abs_val, .Lfunc_end8-abs_val
                                        ; -- End function
	.globl	sum_array                       ; -- Begin function sum_array
	.type	sum_array,@function
sum_array:                              ; @sum_array
; %bb.0:
	subi.w	r29, r29, 16
	st.w	r1, [r29 + -8]
	st.w	r2, [r29 + -16]
	movi.w	r1, 0
	st.w	r1, [r29 + -24]
	st.w	r1, [r29 + -32]
	jmp	.LBB9_1
.LBB9_1:                                ; =>This Inner Loop Header: Depth=1
	ld.w	r1, [r29 + -32]
	ld.w	r2, [r29 + -16]
	cmp.w	r1, r2[31:0], 0
	bnz	0, .LBB9_4
	jmp	.LBB9_2
.LBB9_2:                                ;   in Loop: Header=BB9_1 Depth=1
	ld.w	r1, [r29 + -8]
	ld.w	r2, [r29 + -32]
	movi.w	r3, 2
	shl.w	r2, r2, r3
	add.w	r1, r2[31:0]
	ld.w	r2, [r1 + 0]
	ld.w	r1, [r29 + -24]
	add.w	r1, r2[31:0]
	st.w	r1, [r29 + -24]
	jmp	.LBB9_3
.LBB9_3:                                ;   in Loop: Header=BB9_1 Depth=1
	ld.w	r1, [r29 + -32]
	movi.w	r2, 1
	add.w	r1, r2[31:0]
	st.w	r1, [r29 + -32]
	jmp	.LBB9_1
.LBB9_4:
	ld.w	r1, [r29 + -24]
	addi.w	r29, r29, 16
	ret
.Lfunc_end9:
	.size	sum_array, .Lfunc_end9-sum_array
                                        ; -- End function
	.globl	increment                       ; -- Begin function increment
	.type	increment,@function
increment:                              ; @increment
; %bb.0:
	movi.w	r3, 2012845381744
	ld.w	r2, [r3 + 0]
	movi.w	r4, 1
	add.w	r2, r4[31:0]
	st.w	r2, [r3 + 0]
	ret
.Lfunc_end10:
	.size	increment, .Lfunc_end10-increment
                                        ; -- End function
	.globl	get_counter                     ; -- Begin function get_counter
	.type	get_counter,@function
get_counter:                            ; @get_counter
; %bb.0:
	movi.w	r1, 2012845382008
	ld.w	r1, [r1 + 0]
	ret
.Lfunc_end11:
	.size	get_counter, .Lfunc_end11-get_counter
                                        ; -- End function
	.globl	swap                            ; -- Begin function swap
	.type	swap,@function
swap:                                   ; @swap
; %bb.0:
	subi.w	r29, r29, 12
	mov.w	r3, r1
	st.w	r3, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r2, [r29 + -8]
	ld.w	r2, [r2 + 0]
	st.w	r2, [r29 + -24]
	ld.w	r2, [r29 + -16]
	ld.w	r2, [r2 + 0]
	ld.w	r3, [r29 + -8]
	st.w	r2, [r3 + 0]
	ld.w	r2, [r29 + -24]
	ld.w	r3, [r29 + -16]
	st.w	r2, [r3 + 0]
	addi.w	r29, r29, 12
	ret
.Lfunc_end12:
	.size	swap, .Lfunc_end12-swap
                                        ; -- End function
	.globl	load_val                        ; -- Begin function load_val
	.type	load_val,@function
load_val:                               ; @load_val
; %bb.0:
	subi.w	r29, r29, 4
	st.w	r1, [r29 + -8]
	ld.w	r1, [r29 + -8]
	ld.w	r1, [r1 + 0]
	addi.w	r29, r29, 4
	ret
.Lfunc_end13:
	.size	load_val, .Lfunc_end13-load_val
                                        ; -- End function
	.globl	store_val                       ; -- Begin function store_val
	.type	store_val,@function
store_val:                              ; @store_val
; %bb.0:
	subi.w	r29, r29, 8
	mov.w	r3, r1
	st.w	r3, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r2, [r29 + -16]
	ld.w	r3, [r29 + -8]
	st.w	r2, [r3 + 0]
	addi.w	r29, r29, 8
	ret
.Lfunc_end14:
	.size	store_val, .Lfunc_end14-store_val
                                        ; -- End function
	.globl	read_reg                        ; -- Begin function read_reg
	.type	read_reg,@function
read_reg:                               ; @read_reg
; %bb.0:
	subi.w	r29, r29, 4
	st.w	r1, [r29 + -8]
	ld.w	r1, [r29 + -8]
	ld.w	r1, [r1 + 0]
	addi.w	r29, r29, 4
	ret
.Lfunc_end15:
	.size	read_reg, .Lfunc_end15-read_reg
                                        ; -- End function
	.globl	write_reg                       ; -- Begin function write_reg
	.type	write_reg,@function
write_reg:                              ; @write_reg
; %bb.0:
	subi.w	r29, r29, 8
	mov.w	r3, r1
	st.w	r3, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r2, [r29 + -16]
	ld.w	r3, [r29 + -8]
	st.w	r2, [r3 + 0]
	addi.w	r29, r29, 8
	ret
.Lfunc_end16:
	.size	write_reg, .Lfunc_end16-write_reg
                                        ; -- End function
	.type	g_counter,@object               ; @g_counter
	.section	.bss,"aw",@nobits
	.globl	g_counter
	.p2align	2, 0x0
g_counter:
	.word	0                               ; 0x0
	.size	g_counter, 4

	.ident	"clang version 23.0.0git (https://github.com/llvm/llvm-project.git fb04e8fbb5db10d3dedffbafa86d2b24234123cf)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym g_counter
