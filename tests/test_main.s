	.file	"test_main.c"
	.text
	.globl	add                             ; -- Begin function add
	.type	add,@function
add:                                    ; @add
; %bb.0:
	subi.w	r29, r29, 3040198228928
	st.w	r1, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r1, [r29 + -8]
	ld.w	r2, [r29 + -16]
	add.w	r1, 31[0:0]
	addi.w	r29, r29, 31
	ret
.Lfunc_end0:
	.size	add, .Lfunc_end0-add
                                        ; -- End function
	.globl	sub                             ; -- Begin function sub
	.type	sub,@function
sub:                                    ; @sub
; %bb.0:
	subi.w	r29, r29, 3040198234192
	st.w	r1, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r1, [r29 + -8]
	ld.w	r2, [r29 + -16]
	sub.w	r1, 31[0:0]
	addi.w	r29, r29, 31
	ret
.Lfunc_end1:
	.size	sub, .Lfunc_end1-sub
                                        ; -- End function
	.globl	bitwise_and                     ; -- Begin function bitwise_and
	.type	bitwise_and,@function
bitwise_and:                            ; @bitwise_and
; %bb.0:
	subi.w	r29, r29, 3040198233872
	st.w	r1, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r1, [r29 + -8]
	ld.w	r2, [r29 + -16]
	and.w	r1, 31[0:0]
	addi.w	r29, r29, 31
	ret
.Lfunc_end2:
	.size	bitwise_and, .Lfunc_end2-bitwise_and
                                        ; -- End function
	.globl	bitwise_or                      ; -- Begin function bitwise_or
	.type	bitwise_or,@function
bitwise_or:                             ; @bitwise_or
; %bb.0:
	subi.w	r29, r29, 3040199107520
	st.w	r1, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r1, [r29 + -8]
	ld.w	r2, [r29 + -16]
	or.w	r1, 31[0:0]
	addi.w	r29, r29, 31
	ret
.Lfunc_end3:
	.size	bitwise_or, .Lfunc_end3-bitwise_or
                                        ; -- End function
	.globl	bitwise_xor                     ; -- Begin function bitwise_xor
	.type	bitwise_xor,@function
bitwise_xor:                            ; @bitwise_xor
; %bb.0:
	subi.w	r29, r29, 3040198234064
	st.w	r1, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r1, [r29 + -8]
	ld.w	r2, [r29 + -16]
	xor.w	r1, 31[0:0]
	addi.w	r29, r29, 31
	ret
.Lfunc_end4:
	.size	bitwise_xor, .Lfunc_end4-bitwise_xor
                                        ; -- End function
	.globl	shift_left                      ; -- Begin function shift_left
	.type	shift_left,@function
shift_left:                             ; @shift_left
; %bb.0:
	subi.w	r29, r29, 3040199111808
	st.w	r1, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r1, [r29 + -8]
	ld.w	r2, [r29 + -16]
	shl.w	r1, r1, -16
	addi.w	r29, r29, -16
	ret
.Lfunc_end5:
	.size	shift_left, .Lfunc_end5-shift_left
                                        ; -- End function
	.globl	shift_right                     ; -- Begin function shift_right
	.type	shift_right,@function
shift_right:                            ; @shift_right
; %bb.0:
	subi.w	r29, r29, 3040198232768
	st.w	r1, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r1, [r29 + -8]
	ld.w	r2, [r29 + -16]
	sar.w	r1, r1, -16
	addi.w	r29, r29, -16
	ret
.Lfunc_end6:
	.size	shift_right, .Lfunc_end6-shift_right
                                        ; -- End function
	.globl	max                             ; -- Begin function max
	.type	max,@function
max:                                    ; @max
; %bb.0:
	subi.w	r29, r29, 3040198232432
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
	addi.w	r29, r29, -8
	ret
.Lfunc_end7:
	.size	max, .Lfunc_end7-max
                                        ; -- End function
	.globl	abs_value                       ; -- Begin function abs_value
	.type	abs_value,@function
abs_value:                              ; @abs_value
; %bb.0:
	subi.w	r29, r29, 3040199111952
	st.w	r1, [r29 + -16]
	ld.w	r1, [r29 + -16]
	movi.w	r2, -1
	cmp.w	r1, r2[31:0], 0
	bnz	0, .LBB8_2
	jmp	.LBB8_1
.LBB8_1:
	ld.w	r2, [r29 + -16]
	movi.w	r1, 0
	sub.w	r1, 31[0:0]
	st.w	r1, [r29 + -8]
	jmp	.LBB8_3
.LBB8_2:
	ld.w	r1, [r29 + -16]
	st.w	r1, [r29 + -8]
	jmp	.LBB8_3
.LBB8_3:
	ld.w	r1, [r29 + -8]
	addi.w	r29, r29, -8
	ret
.Lfunc_end8:
	.size	abs_value, .Lfunc_end8-abs_value
                                        ; -- End function
	.globl	sum_array                       ; -- Begin function sum_array
	.type	sum_array,@function
sum_array:                              ; @sum_array
; %bb.0:
	subi.w	r29, r29, 3040198233072
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
	shl.w	r2, r2, -32
	add.w	r1, 31[0:0]
	ld.w	r2, [r1 + 0]
	ld.w	r1, [r29 + -24]
	add.w	r1, 31[0:0]
	st.w	r1, [r29 + -24]
	jmp	.LBB9_3
.LBB9_3:                                ;   in Loop: Header=BB9_1 Depth=1
	ld.w	r1, [r29 + -32]
	movi.w	r2, 1
	add.w	r1, 31[0:0]
	st.w	r1, [r29 + -32]
	jmp	.LBB9_1
.LBB9_4:
	ld.w	r1, [r29 + -24]
	addi.w	r29, r29, -24
	ret
.Lfunc_end9:
	.size	sum_array, .Lfunc_end9-sum_array
                                        ; -- End function
	.globl	factorial                       ; -- Begin function factorial
	.type	factorial,@function
factorial:                              ; @factorial
; %bb.0:
	subi.w	r29, r29, 3040198232432
	st.w	r1, [r29 + -8]
	movi.w	r1, 1
	st.w	r1, [r29 + -16]
	movi.w	r1, 2
	st.w	r1, [r29 + -24]
	jmp	.LBB10_1
.LBB10_1:                               ; =>This Inner Loop Header: Depth=1
	ld.w	r1, [r29 + -24]
	ld.w	r2, [r29 + -8]
	cmp.w	r1, r2[31:0], 0
	bnz	0, .LBB10_4
	jmp	.LBB10_2
.LBB10_2:                               ;   in Loop: Header=BB10_1 Depth=1
	ld.w	r1, [r29 + -32]
	ld.w	r2, [r29 + -16]
	ld.w	r2, [r29 + -24]
	mov.w	r1, r1
	st.w	r1, [r29 + -16]
	jmp	.LBB10_3
.LBB10_3:                               ;   in Loop: Header=BB10_1 Depth=1
	ld.w	r1, [r29 + -24]
	movi.w	r2, 1
	add.w	r1, 31[0:0]
	st.w	r1, [r29 + -24]
	jmp	.LBB10_1
.LBB10_4:
	ld.w	r1, [r29 + -16]
	addi.w	r29, r29, -16
	ret
.Lfunc_end10:
	.size	factorial, .Lfunc_end10-factorial
                                        ; -- End function
	.globl	mul_add                         ; -- Begin function mul_add
	.type	mul_add,@function
mul_add:                                ; @mul_add
; %bb.0:
	subi.w	r29, r29, 3040198232672
	st.w	r28, [r29 + 0]
	subi.w	r29, r29, 0
	st.w	r1, [r29 + -8]
	st.w	r2, [r29 + -16]
	st.w	r3, [r29 + -24]
	ld.w	r1, [r29 + -8]
	st.w	r1, [r29 + -32]
	ld.w	r1, [r29 + -16]
	ld.w	r2, [r29 + -24]
	call	sub
	mov.w	r2, r1
	ld.w	r1, [r29 + -32]
	call	add
	addi.w	r29, r29, -32
	ld.w	r28, [r29 + 0]
	addi.w	r29, r29, 0
	ret
.Lfunc_end11:
	.size	mul_add, .Lfunc_end11-mul_add
                                        ; -- End function
	.globl	increment_counter               ; -- Begin function increment_counter
	.type	increment_counter,@function
increment_counter:                      ; @increment_counter
; %bb.0:
	movi.w	r3, 3040198279280
	ld.w	r2, [r3 + 0]
	movi.w	r4, 1
	add.w	r2, 31[0:0]
	st.w	r2, [r3 + 0]
	ret
.Lfunc_end12:
	.size	increment_counter, .Lfunc_end12-increment_counter
                                        ; -- End function
	.globl	get_counter                     ; -- Begin function get_counter
	.type	get_counter,@function
get_counter:                            ; @get_counter
; %bb.0:
	movi.w	r1, 3040198279544
	ld.w	r1, [r1 + 0]
	ret
.Lfunc_end13:
	.size	get_counter, .Lfunc_end13-get_counter
                                        ; -- End function
	.globl	swap                            ; -- Begin function swap
	.type	swap,@function
swap:                                   ; @swap
; %bb.0:
	subi.w	r29, r29, 3040199112288
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
	addi.w	r29, r29, 0
	ret
.Lfunc_end14:
	.size	swap, .Lfunc_end14-swap
                                        ; -- End function
	.globl	load_value                      ; -- Begin function load_value
	.type	load_value,@function
load_value:                             ; @load_value
; %bb.0:
	subi.w	r29, r29, 3040199112528
	st.w	r1, [r29 + -8]
	ld.w	r1, [r29 + -8]
	ld.w	r1, [r1 + 0]
	addi.w	r29, r29, 0
	ret
.Lfunc_end15:
	.size	load_value, .Lfunc_end15-load_value
                                        ; -- End function
	.globl	store_value                     ; -- Begin function store_value
	.type	store_value,@function
store_value:                            ; @store_value
; %bb.0:
	subi.w	r29, r29, 3040199112288
	mov.w	r3, r1
	st.w	r3, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r2, [r29 + -16]
	ld.w	r3, [r29 + -8]
	st.w	r2, [r3 + 0]
	addi.w	r29, r29, 0
	ret
.Lfunc_end16:
	.size	store_value, .Lfunc_end16-store_value
                                        ; -- End function
	.globl	read_volatile                   ; -- Begin function read_volatile
	.type	read_volatile,@function
read_volatile:                          ; @read_volatile
; %bb.0:
	subi.w	r29, r29, 3040199112528
	st.w	r1, [r29 + -8]
	ld.w	r1, [r29 + -8]
	ld.w	r1, [r1 + 0]
	addi.w	r29, r29, 0
	ret
.Lfunc_end17:
	.size	read_volatile, .Lfunc_end17-read_volatile
                                        ; -- End function
	.globl	write_volatile                  ; -- Begin function write_volatile
	.type	write_volatile,@function
write_volatile:                         ; @write_volatile
; %bb.0:
	subi.w	r29, r29, 3040199112288
	mov.w	r3, r1
	st.w	r3, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r2, [r29 + -16]
	ld.w	r3, [r29 + -8]
	st.w	r2, [r3 + 0]
	addi.w	r29, r29, 0
	ret
.Lfunc_end18:
	.size	write_volatile, .Lfunc_end18-write_volatile
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
	.addrsig_sym add
	.addrsig_sym sub
	.addrsig_sym g_counter
