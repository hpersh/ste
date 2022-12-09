
	.text

	.globl ste_task_cntxt_save
ste_task_cntxt_save:
	# %rdi = Ptr to where to save sp
	movq	(%rsp),%rax	# Get return address
	pushq	%rbx
	pushq	%rcx
	pushq	%rdx
	pushq	%rsi
	pushq	%rdi
	pushq	%rbp
	pushq	%r8
	pushq	%r9
	pushq	%r10
	pushq	%r11
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	movq	%rsp,(%rdi)	# Save sp
	jmpq	*%rax		# Return non-zero

	.globl ste_task_cntxt_restore
ste_task_cntxt_restore:
	# %rdi = New sp
	movq	%rdi,%rsp
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%r11
	popq	%r10
	popq	%r9
	popq	%r8
	popq	%rbp
	popq	%rdi
	popq	%rsi
	popq	%rdx
	popq	%rcx
	popq	%rbx
	xorq	%rax,%rax	# Return 0
	ret

	.globl ste_task_cntxt_init
ste_task_cntxt_init:
	# %rdi = top of stack, %rsi = entry point, %rdx = arg, %rcx = exit
	lea	(16 * -8)(%rdi),%rax
	movq	%rcx,(15 * 8)(%rax)
	movq	%rsi,(14 * 8)(%rax)
	movq	%rdx,(9 * 8)(%rax) # %rdi when popped, see above
	ret			# Return new sp
	
	.globl cpu_intr_disable
cpu_intr_disable:
	ret

	.globl cpu_intr_restore
cpu_intr_restore:
	ret	

	.extern sleep
	.globl cpu_intr_wait
cpu_intr_wait:
	movq	$1,%rdi
	call	sleep
	ret
