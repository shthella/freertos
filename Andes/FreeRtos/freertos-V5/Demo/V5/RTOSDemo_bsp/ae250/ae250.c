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

	/* Enable PLIC features */
	if (read_csr(NDS_MMISC_CTL) & (1 << 1)) {
		/* External PLIC interrupt is vectored */
		__nds__plic_set_feature(NDS_PLIC_FEATURE_PREEMPT | NDS_PLIC_FEATURE_VECTORED);
	} else {
		/* External PLIC interrupt is NOT vectored */
		__nds__plic_set_feature(NDS_PLIC_FEATURE_PREEMPT);
	}

	/* Enable misaligned access and non-blocking load. */
	set_csr(NDS_MMISC_CTL, (1 << 8) | (1 << 6));

#ifdef CFG_CACHE_ENABLE
	/*
	 * Enable I/D cache with HW prefetcher,
	 * D-cache write-around (threshold: 4 cache lines),
	 * and CM (Coherence Manager).
	 */
	clear_csr(NDS_MCACHE_CTL, (3 << 13));
	set_csr(NDS_MCACHE_CTL, (1 << 19) | (1 << 13) | (1 << 10) | (1 << 9) | (1 << 1) | (1 << 0));

	/* Check if CPU support CM or not. */
	if (read_csr(NDS_MCACHE_CTL) & (1 << 19)) {
		/* Wait for cache coherence enabling completed */
		while((read_csr(NDS_MCACHE_CTL) & (1 << 20)) == 0);
	}

	/* Enable L2 cache if HW supports */
	if (AE250_SMU->SYSTEMCFG & (1 << 8)) {
		/* Configure L2 related functions. */
		AE250_L2C->CTL |= ((3 << 5) | (3 << 3));
		/* For GEN1, L2 cache is disabled by default and needs to be configured twice when enabling L2 cache*/
		if (((AE250_L2C->CFG & (0xFF << 24)) >> 24) < 16) {
			AE250_L2C->CTL |= (1 << 0);
		}
	}
#else
	/* Disable L2 cache. For GEN2, L2 cache is enabled by default and needs to be disbaled. */
	if (AE250_SMU->SYSTEMCFG & (1 << 8)) {
		AE250_L2C->CTL &= ~(1 << 0);
	}
#endif
}
