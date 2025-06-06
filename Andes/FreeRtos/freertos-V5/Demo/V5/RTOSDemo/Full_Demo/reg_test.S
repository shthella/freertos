/*
 * FreeRTOS Kernel V10.3.1
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

#if __riscv_xlen == 64
# define STORE    sd
# define LOAD     ld
# define REGBYTES 8
#else
# define STORE    sw
# define LOAD     lw
# define REGBYTES 4
#endif

	.global vRegTest1Implementation
	.global vRegTest2Implementation
	.extern ullRegTest1LoopCounter
	.extern ullRegTest2LoopCounter

	.text

	/* This function is explained in the comments at the top of main-full.c. */
.type vRegTest1Implementation, %function
vRegTest1Implementation:

	/* Fill each general purpose register with a known value. */
	li	x1, 0x01
	/* x2 (sp) is the Stack pointer  - don't mess with x2. */
	/* x3 (gp) is the Global pointer  - don't mess with x3. */
	/* x4 (tp) is the Thread pointer  - don't mess with x4. */
	/* x5 (t0) reserved for assembler and compiler temporaries. */
	li	x6, 0x06
	li	x7, 0x07
	li	x8, 0x08
	li	x9, 0x09
	li	x10, 0x10
	li	x11, 0x11
	li	x12, 0x12
	li	x13, 0x13
	li	x14, 0x14
	li	x15, 0x15
#ifndef __riscv_32e
	li	x16, 0x16
	li	x17, 0x17
	li	x18, 0x18
	li	x19, 0x19
	li	x20, 0x20
	li	x21, 0x21
	li	x22, 0x22
	li	x23, 0x23
	li	x24, 0x24
	li	x25, 0x25
	li	x26, 0x26
	li	x27, 0x27
	li	x28, 0x28
	li	x29, 0x29
	li	x30, 0x30
	li	x31, 0x31
#endif

#ifdef __riscv_flen
	/* Fill each FPU register with a known value. */
	fmv.w.x	f0, x0
	fmv.w.x f1, x1
	fmv.w.x f2, x0
	fmv.w.x f3, x0
	fmv.w.x f4, x0
	fmv.w.x f5, x0
	fmv.w.x f6, x6
	fmv.w.x	f7, x7
	fmv.w.x	f8, x8
	fmv.w.x	f9, x9
	fmv.w.x	f10, x10
	fmv.w.x	f11, x11
	fmv.w.x	f12, x12
	fmv.w.x	f13, x13
	fmv.w.x	f14, x14
	fmv.w.x	f15, x15
	fmv.w.x	f16, x16
	fmv.w.x	f17, x17
	fmv.w.x	f18, x18
	fmv.w.x	f19, x19
	fmv.w.x	f20, x20
	fmv.w.x	f21, x21
	fmv.w.x	f22, x22
	fmv.w.x	f23, x23
	fmv.w.x	f24, x24
	fmv.w.x	f25, x25
	fmv.w.x	f26, x26
	fmv.w.x	f27, x27
	fmv.w.x	f28, x28
	fmv.w.x	f29, x29
	fmv.w.x	f30, x30
	fmv.w.x	f31, x31
	fscsr x31
#endif

	/* Loop, checking each itteration that each register still contains the
	expected value. */
reg1_loop:

	/* Test each general purpose register to check that it still contains the
	expected known value, jumping to reg1_error_loop if any register contains
	an unexpected value. */
	xori	t0, x1, 0x01
	bnez	t0, reg1_error_loop
	xori	t0, x6, 0x06
	bnez	t0, reg1_error_loop
	xori	t0, x7, 0x07
	bnez	t0, reg1_error_loop
	xori	t0, x8, 0x08
	bnez	t0, reg1_error_loop
	xori	t0, x9, 0x09
	bnez	t0, reg1_error_loop
	xori	t0, x10, 0x010
	bnez	t0, reg1_error_loop
	xori	t0, x11, 0x011
	bnez	t0, reg1_error_loop
	xori	t0, x12, 0x012
	bnez	t0, reg1_error_loop
	xori	t0, x13, 0x013
	bnez	t0, reg1_error_loop
	xori	t0, x14, 0x014
	bnez	t0, reg1_error_loop
	xori	t0, x15, 0x015
	bnez	t0, reg1_error_loop
#ifndef __riscv_32e
	xori	t0, x16, 0x016
	bnez	t0, reg1_error_loop
	xori	t0, x17, 0x017
	bnez	t0, reg1_error_loop
	xori	t0, x18, 0x018
	bnez	t0, reg1_error_loop
	xori	t0, x19, 0x019
	bnez	t0, reg1_error_loop
	xori	t0, x20, 0x020
	bnez	t0, reg1_error_loop
	xori	t0, x21, 0x021
	bnez	t0, reg1_error_loop
	xori	t0, x22, 0x022
	bnez	t0, reg1_error_loop
	xori	t0, x23, 0x023
	bnez	t0, reg1_error_loop
	xori	t0, x24, 0x024
	bnez	t0, reg1_error_loop
	xori	t0, x25, 0x025
	bnez	t0, reg1_error_loop
	xori	t0, x26, 0x026
	bnez	t0, reg1_error_loop
	xori	t0, x27, 0x027
	bnez	t0, reg1_error_loop
	xori	t0, x28, 0x028
	bnez	t0, reg1_error_loop
	xori	t0, x29, 0x029
	bnez	t0, reg1_error_loop
	xori	t0, x30, 0x030
	bnez	t0, reg1_error_loop
	xori	t0, x31, 0x031
	bnez	t0, reg1_error_loop
#endif

#ifdef __riscv_flen
	/* Check every floating point register to ensure it contains the expected
	value. */
	fmv.x.w	t0, f0
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f1
	xori	t0, t0, 0x01
	bnez	t0, reg1_error_loop
	fmv.x.w t0, f2
	bnez	t0, reg1_error_loop
	fmv.x.w t0, f3
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f4
	bnez	t0, reg1_error_loop
	fmv.x.w t0, f5
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f6
	xori	t0, t0, 0x06
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f7
	xori	t0, t0, 0x07
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f8
	xori	t0, t0, 0x08
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f9
	xori	t0, t0, 0x09
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f10
	xori	t0, t0, 0x010
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f11
	xori	t0, t0, 0x011
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f12
	xori	t0, t0, 0x012
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f13
	xori	t0, t0, 0x013
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f14
	xori	t0, t0, 0x014
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f15
	xori	t0, t0, 0x015
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f16
	xori	t0, t0, 0x016
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f17
	xori	t0, t0, 0x017
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f18
	xori	t0, t0, 0x018
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f19
	xori	t0, t0, 0x019
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f20
	xori	t0, t0, 0x020
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f21
	xori	t0, t0, 0x021
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f22
	xori	t0, t0, 0x022
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f23
	xori	t0, t0, 0x023
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f24
	xori	t0, t0, 0x024
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f25
	xori	t0, t0, 0x025
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f26
	xori	t0, t0, 0x026
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f27
	xori	t0, t0, 0x027
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f28
	xori	t0, t0, 0x028
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f29
	xori	t0, t0, 0x029
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f30
	xori	t0, t0, 0x030
	bnez	t0, reg1_error_loop
	fmv.x.w	t0, f31
	xori	t0, t0, 0x031
	bnez	t0, reg1_error_loop
	frcsr	t0
	xori	t0, t0, 0x031
	bnez	t0, reg1_error_loop
#endif

	/* Everything passed, increment the loop counter. */
	addi	sp, sp, -16*REGBYTES
	STORE	t1, 0(sp)
	LOAD	t1, ullRegTest1LoopCounter
	addi	t1, t1, 1
	STORE	t1, ullRegTest1LoopCounter, t0
	LOAD	t1, 0(sp)
	addi	sp, sp, 16*REGBYTES

	/* Start again. */
	j	reg1_loop

reg1_error_loop:
	/* If this line is hit then there was an error in a core register value.
	The loop ensures the loop counter stops incrementing. */
	j	reg1_error_loop
	nop

/*-----------------------------------------------------------*/

.type vRegTest2Implementation, %function
vRegTest2Implementation:

	/* Fill each general purpose register with a known value. */
	li	x1, 0x010
	/* x2 (sp) is the Stack pointer  - don't mess with x2. */
	/* x3 (gp) is the Global pointer  - don't mess with x3. */
	/* x4 (tp) is the Thread pointer  - don't mess with x4. */
	/* x5 (t0) reserved for assembler and compiler temporaries. */
	li	x6, 0x060
	li	x7, 0x070
	li	x8, 0x080
	li	x9, 0x090
	li	x10, 0x100
	li	x11, 0x110
	li	x12, 0x120
	li	x13, 0x130
	li	x14, 0x140
	li	x15, 0x150
#ifndef __riscv_32e
	li	x16, 0x160
	li	x17, 0x170
	li	x18, 0x180
	li	x19, 0x190
	li	x20, 0x200
	li	x21, 0x210
	li	x22, 0x220
	li	x23, 0x230
	li	x24, 0x240
	li	x25, 0x250
	li	x26, 0x260
	li	x27, 0x270
	li	x28, 0x280
	li	x29, 0x290
	li	x30, 0x300
	li	x31, 0x310
#endif

#ifdef __riscv_flen
	/* Fill each FPU register with a known value. */
	fmv.w.x	f0, x0
	fmv.w.x f1, x1
	fmv.w.x f2, x0
	fmv.w.x f3, x0
	fmv.w.x f4, x0
	fmv.w.x f5, x0
	fmv.w.x f6, x6
	fmv.w.x	f7, x7
	fmv.w.x	f8, x8
	fmv.w.x	f9, x9
	fmv.w.x	f10, x10
	fmv.w.x	f11, x11
	fmv.w.x	f12, x12
	fmv.w.x	f13, x13
	fmv.w.x	f14, x14
	fmv.w.x	f15, x15
	fmv.w.x	f16, x16
	fmv.w.x	f17, x17
	fmv.w.x	f18, x18
	fmv.w.x	f19, x19
	fmv.w.x	f20, x20
	fmv.w.x	f21, x21
	fmv.w.x	f22, x22
	fmv.w.x	f23, x23
	fmv.w.x	f24, x24
	fmv.w.x	f25, x25
	fmv.w.x	f26, x26
	fmv.w.x	f27, x27
	fmv.w.x	f28, x28
	fmv.w.x	f29, x29
	fmv.w.x	f30, x30
	fmv.w.x	f31, x31
	fscsr x1
#endif

	/* Loop, checking each itteration that each register still contains the
	expected value. */
reg2_loop:

	/* Test each general purpose register to check that it still contains the
	expected known value, jumping to reg2_error_loop if any register contains
	an unexpected value. */
	xori	t0, x1, 0x010
	bnez	t0, reg2_error_loop
	xori	t0, x6, 0x060
	bnez	t0, reg2_error_loop
	xori	t0, x7, 0x070
	bnez	t0, reg2_error_loop
	xori	t0, x8, 0x080
	bnez	t0, reg2_error_loop
	xori	t0, x9, 0x090
	bnez	t0, reg2_error_loop
	xori	t0, x10, 0x100
	bnez	t0, reg2_error_loop
	xori	t0, x11, 0x110
	bnez	t0, reg2_error_loop
	xori	t0, x12, 0x120
	bnez	t0, reg2_error_loop
	xori	t0, x13, 0x130
	bnez	t0, reg2_error_loop
	xori	t0, x14, 0x140
	bnez	t0, reg2_error_loop
	xori	t0, x15, 0x150
	bnez	t0, reg2_error_loop
#ifndef __riscv_32e
	xori	t0, x16, 0x160
	bnez	t0, reg2_error_loop
	xori	t0, x17, 0x170
	bnez	t0, reg2_error_loop
	xori	t0, x18, 0x180
	bnez	t0, reg2_error_loop
	xori	t0, x19, 0x190
	bnez	t0, reg2_error_loop
	xori	t0, x20, 0x200
	bnez	t0, reg2_error_loop
	xori	t0, x21, 0x210
	bnez	t0, reg2_error_loop
	xori	t0, x22, 0x220
	bnez	t0, reg2_error_loop
	xori	t0, x23, 0x230
	bnez	t0, reg2_error_loop
	xori	t0, x24, 0x240
	bnez	t0, reg2_error_loop
	xori	t0, x25, 0x250
	bnez	t0, reg2_error_loop
	xori	t0, x26, 0x260
	bnez	t0, reg2_error_loop
	xori	t0, x27, 0x270
	bnez	t0, reg2_error_loop
	xori	t0, x28, 0x280
	bnez	t0, reg2_error_loop
	xori	t0, x29, 0x290
	bnez	t0, reg2_error_loop
	xori	t0, x30, 0x300
	bnez	t0, reg2_error_loop
	xori	t0, x31, 0x310
	bnez	t0, reg2_error_loop
#endif

#ifdef __riscv_flen
	/* Check every floating point register to ensure it contains the expected
	value. */
	fmv.x.w	t0, f0
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f1
	xori	t0, t0, 0x10
	bnez	t0, reg2_error_loop
	fmv.x.w t0, f2
	bnez	t0, reg2_error_loop
	fmv.x.w t0, f3
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f4
	bnez	t0, reg2_error_loop
	fmv.x.w t0, f5
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f6
	xori	t0, t0, 0x60
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f7
	xori	t0, t0, 0x70
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f8
	xori	t0, t0, 0x80
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f9
	xori	t0, t0, 0x90
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f10
	xori	t0, t0, 0x100
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f11
	xori	t0, t0, 0x110
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f12
	xori	t0, t0, 0x120
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f13
	xori	t0, t0, 0x130
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f14
	xori	t0, t0, 0x140
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f15
	xori	t0, t0, 0x150
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f16
	xori	t0, t0, 0x160
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f17
	xori	t0, t0, 0x170
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f18
	xori	t0, t0, 0x180
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f19
	xori	t0, t0, 0x190
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f20
	xori	t0, t0, 0x200
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f21
	xori	t0, t0, 0x210
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f22
	xori	t0, t0, 0x220
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f23
	xori	t0, t0, 0x230
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f24
	xori	t0, t0, 0x240
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f25
	xori	t0, t0, 0x250
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f26
	xori	t0, t0, 0x260
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f27
	xori	t0, t0, 0x270
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f28
	xori	t0, t0, 0x280
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f29
	xori	t0, t0, 0x290
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f30
	xori	t0, t0, 0x300
	bnez	t0, reg2_error_loop
	fmv.x.w	t0, f31
	xori	t0, t0, 0x310
	bnez	t0, reg2_error_loop
	frcsr	t0
	xori	t0, t0, 0x10
	bnez	t0, reg2_error_loop
#endif

	/* Everything passed, increment the loop counter. */
	addi	sp, sp, -16*REGBYTES
	STORE	t1, 0(sp)
	LOAD	t1, ullRegTest2LoopCounter
	addi	t1, t1, 1
	STORE   t1, ullRegTest2LoopCounter, t0
	LOAD	t1, 0(sp)
	addi	sp, sp, 16*REGBYTES

	/* Start again. */
	j	reg2_loop

reg2_error_loop:
	/* If this line is hit then there was an error in a core register value.
	The loop ensures the loop counter stops incrementing. */
	j	reg2_error_loop
	nop

	.end
