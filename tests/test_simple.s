	.file	"test_simple.ll"
	.text
	.globl	add                             ; -- Begin function add
	.type	add,@function
add:                                    ; @add
; %bb.0:
	add.w	r1, r2[31:0]
	ret
.Lfunc_end0:
	.size	add, .Lfunc_end0-add
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
