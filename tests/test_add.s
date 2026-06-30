	.file	"test_add.ll"
	.text
	.globl	add_char                        ; -- Begin function add_char
	.type	add_char,@function
add_char:                               ; @add_char
; %bb.0:
	ld.b	r2, [r29 + 8]
	ld.b	r1, [r29 + 0]
	add.b	r1, r2[7:0]
	ret
.Lfunc_end0:
	.size	add_char, .Lfunc_end0-add_char
                                        ; -- End function
	.globl	add_short                       ; -- Begin function add_short
	.type	add_short,@function
add_short:                              ; @add_short
; %bb.0:
	ld.h	r2, [r29 + 8]
	ld.h	r1, [r29 + 0]
	add.h	r1, r2[15:0]
	ret
.Lfunc_end1:
	.size	add_short, .Lfunc_end1-add_short
                                        ; -- End function
	.globl	add_int                         ; -- Begin function add_int
	.type	add_int,@function
add_int:                                ; @add_int
; %bb.0:
	add.w	r1, r2[31:0]
	ret
.Lfunc_end2:
	.size	add_int, .Lfunc_end2-add_int
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
