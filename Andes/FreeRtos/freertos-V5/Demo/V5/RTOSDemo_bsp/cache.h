/*
 * Copyright (c) 2012-2021 Andes Technology Corporation
 * All rights reserved.
 *
 */

#ifndef __CACHE_H__
#define __CACHE_H__

#include <stdint.h>

#include "core_v5.h"

/* Cache ops */
extern void nds_icache_invalidate_range(unsigned long start, unsigned long size);
extern void nds_icache_invalidate_all(void);
extern void nds_dcache_writeback_range(unsigned long start, unsigned long size);
extern void nds_dcache_invalidate_range(unsigned long start, unsigned long size);
extern void nds_dcache_flush_range(unsigned long start, unsigned long size);
extern void nds_dcache_flush_all(void);

/* DMA-specific ops */
extern void nds_dma_writeback_range(unsigned long start, unsigned long size);
extern void nds_dma_invalidate_range(unsigned long start, unsigned long size);
extern void nds_dma_invalidate_range2(unsigned long start, unsigned long size);

#endif /* __CACHE_H__ */
