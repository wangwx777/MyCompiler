	.file	"test_add.c"
	.text
	.globl	add_char                        ; -- Begin function add_char
	.type	add_char,@function
add_char:                               ; @add_char
; %bb.0:
	mov.w	r1, r1
	ret
.Lfunc_end0:
	.size	add_char, .Lfunc_end0-add_char
                                        ; -- End function
	.globl	add_short                       ; -- Begin function add_short
	.type	add_short,@function
add_short:                              ; @add_short
; %bb.0:
	mov.w	r1, r1
	ret
.Lfunc_end1:
	.size	add_short, .Lfunc_end1-add_short
                                        ; -- End function
	.globl	add_int                         ; -- Begin function add_int
	.type	add_int,@function
add_int:                                ; @add_int
; %bb.0:
	add.w	r2, r1[31:0]
	mov.w	r1, r2
	ret
.Lfunc_end2:
	.size	add_int, .Lfunc_end2-add_int
                                        ; -- End function
	.ident	"clang version 23.0.0git (https://github.com/llvm/llvm-project.git fb04e8fbb5db10d3dedffbafa86d2b24234123cf)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
