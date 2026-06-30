; read_linear_table — 协处理器查表实现
; void read_linear_table(uint32_t table_id, uint32_t idx, void *dst);
; 调用约定: r1=table_id, r2=idx, r3=dst
; 协处理器指令: LD=读(opcode 0x14), ST=写(opcode 0x15)

	.text
	.globl	read_linear_table
	.type	read_linear_table,@function

read_linear_table:
	; 保存 dst 指针到 r4 (T0), 因为协处理器读会覆盖 r3
	movab.w	r4, r3

	; 步骤1: 写 idx 到协处理器触发查表 (ST opcode 0x15)
	st.w	r2, r1, 0

	; 步骤2: 从协处理器读取查表结果 (LD opcode 0x14)
	ld.w	r3, r1, 0

	; 步骤3: 将结果写入 dst 指针指向的内存 (MOV opcode 0x28)
	mov.w	r3, [r4 + 0]

	ret
.Lfunc_end0:
	.size	read_linear_table, .Lfunc_end0-read_linear_table
