/*
 * Copyright (c) 2012-2021 Andes Technology Corporation
 * All rights reserved.
 *
 */

#include "platform.h"

extern void reset_vector(void);

/* This must be a leaf function, no child function */
void __platform_init (void) __attribute__((naked));
void __platform_init(void)
{
	/* Do your platform low-level initial */

	__asm("ret");
}

void c_startup(void)
{
#define MEMCPY(des, src, n)     __builtin_memcpy ((des), (src), (n))
#define MEMSET(s, c, n)         __builtin_memset ((s), (c), (n))
	/* Data section initialization */
	extern char  _edata, _end;
	unsigned int size;
#ifndef CFG_BURN
	/* We don't need to do it in BURN mode because it has been done by loader. */
	extern char __data_lmastart, __data_start;

	/* Copy data section from LMA to VMA */
	size = &_edata - &__data_start;
	MEMCPY(&__data_start, &__data_lmastart, size);
#endif

	/* Clear bss section */
	size = &_end - &_edata;
	MEMSET(&_edata, 0, size);
}

void system_init(void)
{
	/*
	 * Do your system reset handling here
	 */
	/* Reset the CPU reset vector for this program. */
	AE250_SMU->RESET_VECTOR = (unsigned int)(long)reset_vector;

	/* Initial CLIC with Machine mode, 2-bits interrupt level. */
	__nds__clic_set_cliccfg(0, 2);

	/* Enable misaligned access and non-blocking load. */
	set_csr(NDS_MMISC_CTL, (1 << 8) | (1 << 6));

#if 0
#ifdef CFG_CACHE_ENABLE
	/* Note: RTOS Tracer doesn't support L2 cache currently. */
	#if( configUSE_ANDES_TRACER != 1 )
		/* If HW supports L2 cache */
		if (AE250_SMU->SYSTEMCFG & (1 << 8)) {
			/* Enable L2 cache */
			AE250_L2C->CTL |= (1 << 0);
		}
	#endif
#endif
#endif
}
