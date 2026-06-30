	.file	"test_mycpu.ll"
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
	.globl	max                             ; -- Begin function max
	.type	max,@function
max:                                    ; @max
; %bb.0:
	cmp.w	r1, r2[31:0], 0
	bnz	0, .LBB1_2
	jmp	.LBB1_1
.LBB1_1:                                ; %then
	ret
.LBB1_2:                                ; %else
	mov.w	r1, r2
	ret
.Lfunc_end1:
	.size	max, .Lfunc_end1-max
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
