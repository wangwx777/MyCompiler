	.file	"test_add.c"
	.text
	.globl	add_char                        ; -- Begin function add_char
	.type	add_char,@function
add_char:                               ; @add_char
; %bb.0:
	subi.w	r29, r29, 8
	ld.w	r1, [r29 + -16]
	ld.w	r2, [r29 + 8]
	mov.b	r2, r2
	ld.w	r3, [r29 + 0]
	mov.b	r3, r3
	st.b	r3, [r29 + -2]
	st.b	r2, [r29 + -4]
	mov.w	r1, r1
	addi.w	r29, r29, 8
	ret
.Lfunc_end0:
	.size	add_char, .Lfunc_end0-add_char
                                        ; -- End function
	.globl	add_short                       ; -- Begin function add_short
	.type	add_short,@function
add_short:                              ; @add_short
; %bb.0:
	subi.w	r29, r29, 8
	ld.w	r1, [r29 + -16]
	ld.w	r2, [r29 + 8]
	mov.h	r2, r2
	ld.w	r3, [r29 + 0]
	mov.h	r3, r3
	st.h	r3, [r29 + -4]
	st.h	r2, [r29 + -8]
	mov.w	r1, r1
	addi.w	r29, r29, 8
	ret
.Lfunc_end1:
	.size	add_short, .Lfunc_end1-add_short
                                        ; -- End function
	.globl	add_int                         ; -- Begin function add_int
	.type	add_int,@function
add_int:                                ; @add_int
; %bb.0:
	subi.w	r29, r29, 8
	st.w	r1, [r29 + -8]
	st.w	r2, [r29 + -16]
	ld.w	r1, [r29 + -8]
	ld.w	r2, [r29 + -16]
	add.w	r1, r2[31:0]
	addi.w	r29, r29, 8
	ret
.Lfunc_end2:
	.size	add_int, .Lfunc_end2-add_int
                                        ; -- End function
	.ident	"clang version 23.0.0git (https://github.com/llvm/llvm-project.git fb04e8fbb5db10d3dedffbafa86d2b24234123cf)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
