/*
 * Copyright (c) 2012-2021 Andes Technology Corporation
 * All rights reserved.
 *
 */
#include "core_v5.h"

.extern freertos_risc_v_trap_handler

	.section .nds_vector, "ax"

	.global reset_vector

reset_vector:
	/* Decide whether this is an NMI or cold reset */
	csrr t0, mcause
	bnez t0, nmi_handler

	.global _start
	.type _start,@function

_start:
	/* Initialize global pointer */
	.option push
	.option norelax
	la gp, __global_pointer$
	.option pop

	/* Initialize stack pointer */
	la t0, _stack
	mv sp, t0

#ifdef __nds_execit
	/* Initialize EXEC.IT table */
	la t0, _ITB_BASE_
	csrw uitb, t0
#endif

#ifdef __riscv_flen
	/* Enable FPU */
	li t0, MSTATUS_FS
	csrrs t0, mstatus, t0

	/* Initialize FCSR */
	fscsr zero
#endif

	/* Initial machine non-vectored trap base */
	la t0, freertos_risc_v_trap_handler
	ori t0, t0, 3
	csrw mtvec, t0

	/* Initial machine vector table base */
	la t0, __vectors
	csrw mtvt, t0

	/*
	 * Initial and enable machine interrupt common entry base
	 * if ECLIC (mmsc_cfg.ECLIC==1) feature is supported.
	 */
	csrr t0, mmsc_cfg
	bbc t0, 28, 1f
	la t0, freertos_risc_v_trap_handler
	ori t0, t0, 1
	csrw mirq_entry, t0

	/* Do system low level setup. It must be a leaf function */
	call __platform_init

	/* System reset handler */
	call reset_handler

	/* Infinite loop, if returned accidently */
1:	j 1b

	.section .text

	.weak nmi_handler
nmi_handler:
1:	j 1b

	.weak trap_handler
trap_handler:
1:	j 1b

	.global default_irq_entry
	.align 2
default_irq_entry:
1:	j 1b

	.weak __platform_init
__platform_init:
	ret

	/* Non-SHV interrupt common code entry */
	.global irq_entry
	.align 2

irq_entry:
#ifdef __riscv_flen
	addi sp, sp, -32*REGBYTES - 16*FPREGBYTES
#else
	addi sp, sp, -32*REGBYTES
#endif

	/* Save caller registers */
	STORE x1, 0*REGBYTES(sp)
	STORE x4, 1*REGBYTES(sp)
	STORE x5, 2*REGBYTES(sp)
	STORE x6, 3*REGBYTES(sp)
	STORE x7, 4*REGBYTES(sp)
	STORE x10, 5*REGBYTES(sp)
	STORE x11, 6*REGBYTES(sp)
	STORE x12, 7*REGBYTES(sp)
	STORE x13, 8*REGBYTES(sp)
	STORE x14, 9*REGBYTES(sp)
	STORE x15, 10*REGBYTES(sp)
#ifndef __riscv_32e
	STORE x16, 11*REGBYTES(sp)
	STORE x17, 12*REGBYTES(sp)
	STORE x28, 13*REGBYTES(sp)
	STORE x29, 14*REGBYTES(sp)
	STORE x30, 15*REGBYTES(sp)
	STORE x31, 16*REGBYTES(sp)
#endif

	/*
	 * Save mcause and mepc
	 * There is no need to store mstauts, because MPP and MPIE are included in mcause.
	 */
	csrrwi x0, pushmcause, 17
	csrrwi x0, pushmepc, 18

#if SUPPORT_PFT_ARCH
	/* Save mxstatus */
	csrrwi x0, pushmxstatus, 19
#endif

#ifdef __riscv_flen
	/* Read FPU status fcsr register */
	frcsr x5

	/* Save fp caller registers */
	FPSTORE f0, (20*REGBYTES + 0*FPREGBYTES)(sp)
	FPSTORE f1, (20*REGBYTES + 1*FPREGBYTES)(sp)
	FPSTORE f2, (20*REGBYTES + 2*FPREGBYTES)(sp)
	FPSTORE f3, (20*REGBYTES + 3*FPREGBYTES)(sp)
	FPSTORE f4, (20*REGBYTES + 4*FPREGBYTES)(sp)
	FPSTORE f5, (20*REGBYTES + 5*FPREGBYTES)(sp)
	FPSTORE f6, (20*REGBYTES + 6*FPREGBYTES)(sp)
	FPSTORE f7, (20*REGBYTES + 7*FPREGBYTES)(sp)
	FPSTORE f10, (20*REGBYTES + 8*FPREGBYTES)(sp)
	FPSTORE f11, (20*REGBYTES + 9*FPREGBYTES)(sp)
	FPSTORE f12, (20*REGBYTES + 10*FPREGBYTES)(sp)
	FPSTORE f13, (20*REGBYTES + 11*FPREGBYTES)(sp)
	FPSTORE f14, (20*REGBYTES + 12*FPREGBYTES)(sp)
	FPSTORE f15, (20*REGBYTES + 13*FPREGBYTES)(sp)
	FPSTORE f16, (20*REGBYTES + 14*FPREGBYTES)(sp)
	FPSTORE f17, (20*REGBYTES + 15*FPREGBYTES)(sp)
	FPSTORE f28, (20*REGBYTES + 16*FPREGBYTES)(sp)
	FPSTORE f29, (20*REGBYTES + 17*FPREGBYTES)(sp)
	FPSTORE f30, (20*REGBYTES + 18*FPREGBYTES)(sp)
	FPSTORE f31, (20*REGBYTES + 19*FPREGBYTES)(sp)

	/* Save fcsr register */
	sw x5, (20*REGBYTES + 20*FPREGBYTES)(sp)
#endif

service_loop:
	csrrw ra, mintsel_jal, ra

#ifdef __riscv_flen
	/* Restore fcsr register */
	lw x5, (20*REGBYTES + 20*FPREGBYTES)(sp)

	/* Restore fp caller registers */
	FPLOAD f0, (20*REGBYTES + 0*FPREGBYTES)(sp)
	FPLOAD f1, (20*REGBYTES + 1*FPREGBYTES)(sp)
	FPLOAD f2, (20*REGBYTES + 2*FPREGBYTES)(sp)
	FPLOAD f3, (20*REGBYTES + 3*FPREGBYTES)(sp)
	FPLOAD f4, (20*REGBYTES + 4*FPREGBYTES)(sp)
	FPLOAD f5, (20*REGBYTES + 5*FPREGBYTES)(sp)
	FPLOAD f6, (20*REGBYTES + 6*FPREGBYTES)(sp)
	FPLOAD f7, (20*REGBYTES + 7*FPREGBYTES)(sp)
	FPLOAD f10, (20*REGBYTES + 8*FPREGBYTES)(sp)
	FPLOAD f11, (20*REGBYTES + 9*FPREGBYTES)(sp)
	FPLOAD f12, (20*REGBYTES + 10*FPREGBYTES)(sp)
	FPLOAD f13, (20*REGBYTES + 11*FPREGBYTES)(sp)
	FPLOAD f14, (20*REGBYTES + 12*FPREGBYTES)(sp)
	FPLOAD f15, (20*REGBYTES + 13*FPREGBYTES)(sp)
	FPLOAD f16, (20*REGBYTES + 14*FPREGBYTES)(sp)
	FPLOAD f17, (20*REGBYTES + 15*FPREGBYTES)(sp)
	FPLOAD f28, (20*REGBYTES + 16*FPREGBYTES)(sp)
	FPLOAD f29, (20*REGBYTES + 17*FPREGBYTES)(sp)
	FPLOAD f30, (20*REGBYTES + 18*FPREGBYTES)(sp)
	FPLOAD f31, (20*REGBYTES + 19*FPREGBYTES)(sp)

	/* Write FPU status fcsr register */
	fscsr x5
#endif

	/* Restore caller registers */
//	LOAD x1, 0*REGBYTES(sp)   # The x1(ra) will be used, so they need to be restore later
	LOAD x4, 1*REGBYTES(sp)
	LOAD x5, 2*REGBYTES(sp)
	LOAD x6, 3*REGBYTES(sp)
	LOAD x7, 4*REGBYTES(sp)
	LOAD x10, 5*REGBYTES(sp)
	LOAD x11, 6*REGBYTES(sp)
	LOAD x12, 7*REGBYTES(sp)
	LOAD x13, 8*REGBYTES(sp)
	LOAD x14, 9*REGBYTES(sp)
	LOAD x15, 10*REGBYTES(sp)
#ifndef __riscv_32e
	LOAD x16, 11*REGBYTES(sp)
	LOAD x17, 12*REGBYTES(sp)
	LOAD x28, 13*REGBYTES(sp)
	LOAD x29, 14*REGBYTES(sp)
	LOAD x30, 15*REGBYTES(sp)
	LOAD x31, 16*REGBYTES(sp)
#endif

	LOAD x1, 17*REGBYTES(sp) # Get saved mcause

	/* Disable interrupts before restoring CSR */
	csrrci x0, mstatus, MSTATUS_MIE

	/* Restore mcause and mepc */
	csrw mcause, x1
	LOAD x1, 18*REGBYTES(sp) # Get saved mepc
	csrw mepc, x1

#if SUPPORT_PFT_ARCH
	/* Restore mxstatus */
	LOAD x1, 19*REGBYTES(sp) # Get saved mxstatus
	csrw mxstatus, x1
#endif

	/* Check if there is pending interrupt */
	csrr x1, mnxti
	bnez x1, service_loop

	/* Restore original x1 value */
	LOAD x1, 0*REGBYTES(sp)

#ifdef __riscv_flen
	addi sp, sp, 32*REGBYTES + 16*FPREGBYTES
#else
	addi sp, sp, 32*REGBYTES
#endif

	mret

	.macro INTERRUPT num
	.weak entry_irq\num
	.set entry_irq\num, default_irq_entry
#if __riscv_xlen == 32
	.long entry_irq\num
#else
	.quad entry_irq\num
#endif
	.endm

	/* Vector table
	 * NOTE:
	 * The Vector Table base alignment requirement has to be :
	 * " 2^ceiling(log2(N)) x 4 " bytes,
	 * if the CLIC device supports N interrupt sources.
	 */
#define VECTOR_NUMINTRS         51	// 19 (system) + 32 (devices)
	.section .vector_table, "a"

	.global __vectors
	.balign 256

__vectors:

	/* CLIC interrupt vector */
	.altmacro
	.set irqno, 0
	.rept VECTOR_NUMINTRS
	INTERRUPT %irqno
	.set irqno, irqno+1
	.endr
