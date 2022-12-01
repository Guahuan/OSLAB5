.align 3
.section .text.entry
.equ reg_size, 0x8
.globl trap_s
.globl __switch_to
.globl init_epc
.extern handler_s

trap_s:

	sret

init_epc:
	la t0, dead_loop
	csrw sepc, t0
	sret

__switch_to:
	
	ret
