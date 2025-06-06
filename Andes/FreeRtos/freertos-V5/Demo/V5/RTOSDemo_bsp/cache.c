/*
 * Copyright (c) 2012-2021 Andes Technology Corporation
 * All rights reserved.
 *
 */

#include "cache.h"
#include "platform.h"

/* CSR bit-field */

/* MICM_CFG, MDCM_CFG */
#define NDS_ISET				0x7
#define NDS_IWAY				0x38
#define NDS_ISIZE				0x1C0
#define NDS_DSET				0x7
#define NDS_DWAY				0x38
#define NDS_DSIZE				0x1C0

/* MMSC_CFG, MMSC_CFG2 */
#define NDS_IOCP				0x8000
#define NDS_MSC_EXT				0x80000000

/* L1 CCTL Command */
#define CCTL_L1D_VA_INVAL			0
#define CCTL_L1D_VA_WB				1
#define CCTL_L1D_VA_WBINVAL			2
#define CCTL_L1D_VA_LOCK			3
#define CCTL_L1D_VA_UNLOCK			4
#define CCTL_L1D_WBINVAL_ALL			6
#define CCTL_L1D_WB_ALL				7
#define CCTL_L1I_VA_INVAL			8
#define CCTL_L1I_VA_LOCK			11
#define CCTL_L1I_VA_UNLOCK			12
#define CCTL_L1D_IX_INVAL			16
#define CCTL_L1D_IX_WB				17
#define CCTL_L1D_IX_WBINVAL			18
#define CCTL_L1D_IX_RTAG			19
#define CCTL_L1D_IX_RDATA			20
#define CCTL_L1D_IX_WTAG			21
#define CCTL_L1D_IX_WDATA			22
#define CCTL_L1D_INVAL_ALL			23
#define CCTL_L1I_IX_INVAL			24
#define CCTL_L1I_IX_RTAG			27
#define CCTL_L1I_IX_RDATA			28
#define CCTL_L1I_IX_WTAG			29
#define CCTL_L1I_IX_WDATA			30

/* L2 CCTL Command */
#define CCTL_L2_IX_INVAL			0x00
#define CCTL_L2_IX_WB				0x01
#define CCTL_L2_IX_WBINVAL			0x02
#define CCTL_L2_PA_INVAL			0x08
#define CCTL_L2_PA_WB				0x09
#define CCTL_L2_PA_WBINVAL			0x0a
#define CCTL_L2_TGT_WRITE			0x10
#define CCTL_L2_TGT_READ			0x11
#define CCTL_L2_WBINVAL_ALL			0x12

/* L2 CCTL Register bit-field */
#define CCTL_ST_IDLE				0
#define CCTL_ST_RUNNING				1
#define CCTL_ST_ILLEGAL				2

#define CCTL_ST_MASK				0xF

/* Helper Macros */
#define ALWAYS_INLINE				inline __attribute__((always_inline))

/*
 * Access 32-bit high/low part of 64-bit HW registers.
 * It's helpful to avoid redundant load/store to 64-bit registers in RV32.
 */
#define REG64_HI(reg_addr)			( ((volatile uint32_t *) (reg_addr))[1] )
#define REG64_LO(reg_addr)			( ((volatile uint32_t *) (reg_addr))[0] )

/* Value of x rounded up to the next multiple of align. align must be power of 2. */
#define ROUND_UP(x, align)			(((x) + (align) - 1) & ~((align) - 1))

/* Value of x rounded down to the prev multiple of align. align must be power of 2. */
#define ROUND_DOWN(x, align)			((x) & ~((align) - 1))

struct _cache_info {
	uint8_t is_init;
	uint8_t has_l2_cache;
	uint8_t has_dma_coherency;
	unsigned long cacheline_size;
};

/* Cache APIs */
struct _cache_info cache_info = {.is_init = 0, .has_l2_cache = 0, .has_dma_coherency = 0};
enum cache_t {ICACHE, DCACHE};

static void get_cache_info(void);

/* Critical section APIs */
static ALWAYS_INLINE unsigned long GIE_SAVE(void)
{
	/* Disable global interrupt for core */
	return clear_csr(NDS_MSTATUS, MSTATUS_MIE) & MSTATUS_MIE;
}

static ALWAYS_INLINE void GIE_RESTORE(unsigned long var)
{
	set_csr(NDS_MSTATUS, var);
}

static inline __attribute__((always_inline)) unsigned long cache_line_size()
{
	if (!cache_info.is_init) {
		get_cache_info();
	}

	return cache_info.cacheline_size;
}

static inline __attribute__((always_inline)) unsigned long has_l2_cache()
{
	if (!cache_info.is_init) {
		get_cache_info();
	}

	return cache_info.has_l2_cache;
}

static inline __attribute__((always_inline)) unsigned long has_dma_coherency()
{
	if (!cache_info.is_init) {
		get_cache_info();
	}

	return cache_info.has_dma_coherency;
}

static void get_cache_info(void)
{
	/* Critical Section */
	unsigned long saved_gie = GIE_SAVE();

	/* Check cache_info has been initialized again in the critical section. */
	if (!cache_info.is_init) {
		/* Get cacheline size */
		unsigned long size = (read_csr(NDS_MDCM_CFG) & NDS_DSIZE) >> 6;
		if ((size > 0) && (size <= 5)) {
			cache_info.cacheline_size = 1 << (size + 2);
		}
		else {
			cache_info.cacheline_size = 0;
		}

		/* Check L2 cache is supported */
		if (DEV_SMU->SYSTEMCFG & (1 << 8)) {
			cache_info.has_l2_cache = 1;
		}

		/* Assume that DMA is attached to coherence port.
		   Set flag has_dma_coherency if HW support */
		if (read_csr(NDS_MCACHE_CTL) & (1 << 19)) {
			/* Check if cache coherence is enabled */
			if (read_csr(NDS_MCACHE_CTL) & (1 << 20)) {
				/* Check IOCP is supported */
#if __riscv_xlen == 32
				if (read_csr(NDS_MMSC_CFG) & NDS_MSC_EXT) {
					if (read_csr(NDS_MMSC_CFG2) & NDS_IOCP) {
						cache_info.has_dma_coherency = 1;
					}
				}
#else
				if (read_csr(NDS_MMSC_CFG) & ((unsigned long)NDS_IOCP << 32)) {
					cache_info.has_dma_coherency = 1;
				}
#endif
			}
		}

		/* Finish initialization */
		cache_info.is_init = 1;
	}

	GIE_RESTORE(saved_gie);
}

static inline unsigned long cache_set(enum cache_t cache)
{
	if (cache == ICACHE)
		return ((read_csr(NDS_MICM_CFG) & NDS_ISET) < 7) ? (unsigned long)(1 << ((read_csr(NDS_MICM_CFG) & NDS_ISET) + 6)) : 0;
	else
		return ((read_csr(NDS_MDCM_CFG) & NDS_DSET) < 7) ? (unsigned long)(1 << ((read_csr(NDS_MDCM_CFG) & NDS_DSET) + 6)) : 0;
}

static inline unsigned long cache_way(enum cache_t cache)
{
	if (cache == ICACHE)
		return (unsigned long)(((read_csr(NDS_MICM_CFG) & NDS_IWAY) >> 3) + 1);
	else
		return (unsigned long)(((read_csr(NDS_MDCM_CFG) & NDS_DWAY) >> 3) + 1);
}

/* Low-level Cache APIs */

/*
 * Note: Low-level CCTL functions may not be thread-safe if it uses more than
 * 1 CCTL register because IRQ can pollute CCTL register. Thus, caller needs to
 * protect thread-safety of them.
 */

static void nds_l1c_icache_invalidate_range(unsigned long start, unsigned long size)
{
	unsigned long line_size = cache_line_size();

	unsigned long last_byte = start + size - 1;
	start = ROUND_DOWN(start, line_size);

	while (start <= last_byte) {
		write_csr(NDS_MCCTLBEGINADDR, start);
		write_csr(NDS_MCCTLCOMMAND, CCTL_L1I_VA_INVAL);
		start += line_size;
	}
}

/*
 * nds_l1c_icache_invalidate_all(void)
 *
 * invalidate all L1 I-cached data.
 *
 * Note: CCTL L1 doesn't support icache invalidate all operation,
 * so this function emulates "Invalidate all" operation by invalidating
 * each cache line of cache (index-based CCTL).
 */
static void nds_l1c_icache_invalidate_all(void)
{
	unsigned long line_size = cache_line_size();
	unsigned long end = cache_way(ICACHE) * cache_set(ICACHE) * line_size;

	for (int i = 0; i < end; i += line_size) {
		write_csr(NDS_MCCTLBEGINADDR, i);
		write_csr(NDS_MCCTLCOMMAND, CCTL_L1I_IX_INVAL);
	}
}

static void nds_l1c_dcache_writeback_range(unsigned long start, unsigned long size)
{
	unsigned long line_size = cache_line_size();

	unsigned long last_byte = start + size - 1;
	start = ROUND_DOWN(start, line_size);

	while (start <= last_byte) {
		write_csr(NDS_MCCTLBEGINADDR, start);
		write_csr(NDS_MCCTLCOMMAND, CCTL_L1D_VA_WB);
		start += line_size;
	}
}

static void nds_l1c_dcache_invalidate_range(unsigned long start, unsigned long size)
{
	unsigned long line_size = cache_line_size();

	unsigned long last_byte = start + size - 1;
	start = ROUND_DOWN(start, line_size);

	while (start <= last_byte) {
		write_csr(NDS_MCCTLBEGINADDR, start);
		write_csr(NDS_MCCTLCOMMAND, CCTL_L1D_VA_INVAL);
		start += line_size;
	}
}

static void nds_l1c_dcache_flush_range(unsigned long start, unsigned long size)
{
	unsigned long line_size = cache_line_size();

	unsigned long last_byte = start + size - 1;
	start = ROUND_DOWN(start, line_size);

	while (start <= last_byte) {
		write_csr(NDS_MCCTLBEGINADDR, start);
		write_csr(NDS_MCCTLCOMMAND, CCTL_L1D_VA_WBINVAL);
		start += line_size;
	}
}

static ALWAYS_INLINE void nds_l1c_dcache_writeback_all(void)
{
	write_csr(NDS_MCCTLCOMMAND, CCTL_L1D_WB_ALL);
}

static ALWAYS_INLINE void nds_l1c_dcache_invalidate_all(void)
{
	write_csr(NDS_MCCTLCOMMAND, CCTL_L1D_INVAL_ALL);
}

static ALWAYS_INLINE void nds_l1c_dcache_flush_all(void)
{
	write_csr(NDS_MCCTLCOMMAND, CCTL_L1D_WBINVAL_ALL);
}

static void nds_l2c_cache_writeback_range(unsigned long start, unsigned long size)
{
	if (!has_l2_cache()) {
		return;
	}

	unsigned long line_size = cache_line_size();

	unsigned long last_byte = start + size - 1;
	start = ROUND_DOWN(start, line_size);

	while (start <= last_byte) {
		DEV_L2C->CORECCTL[0].CCTLACC = start;
		REG64_LO(&DEV_L2C->CORECCTL[0].CCTLCMD) = CCTL_L2_PA_WB;
		start += line_size;

		/* Wait L2 CCTL Commands finished. */
		while((REG64_LO(&DEV_L2C->CORECCTL[0].CCTLSTATUS) & CCTL_ST_MASK)
			== CCTL_ST_RUNNING);
	}
}

static void nds_l2c_cache_invalidate_range(unsigned long start, unsigned long size)
{
	if (!has_l2_cache()) {
		return;
	}

	unsigned long line_size = cache_line_size();

	unsigned long last_byte = start + size - 1;
	start = ROUND_DOWN(start, line_size);

	while (start <= last_byte) {
		DEV_L2C->CORECCTL[0].CCTLACC = start;
		REG64_LO(&DEV_L2C->CORECCTL[0].CCTLCMD) = CCTL_L2_PA_INVAL;
		start += line_size;

		/* Wait L2 CCTL Commands finished. */
		while((REG64_LO(&DEV_L2C->CORECCTL[0].CCTLSTATUS) & CCTL_ST_MASK)
			== CCTL_ST_RUNNING);

	}
}

static void nds_l2c_cache_flush_range(unsigned long start, unsigned long size)
{
	if (!has_l2_cache()) {
		return;
	}

	unsigned long line_size = cache_line_size();

	unsigned long last_byte = start + size - 1;
	start = ROUND_DOWN(start, line_size);

	while (start <= last_byte) {
		DEV_L2C->CORECCTL[0].CCTLACC = start;
		REG64_LO(&DEV_L2C->CORECCTL[0].CCTLCMD) = CCTL_L2_PA_WBINVAL;
		start += line_size;

		/* Wait L2 CCTL Commands finished. */
		while((REG64_LO(&DEV_L2C->CORECCTL[0].CCTLSTATUS) & CCTL_ST_MASK)
			== CCTL_ST_RUNNING);
	}
}

static ALWAYS_INLINE void nds_l2c_cache_flush_all(void)
{
	if (!has_l2_cache()) {
		return;
	}

	REG64_LO(&DEV_L2C->CORECCTL[0].CCTLCMD) = CCTL_L2_WBINVAL_ALL;

	/* Wait L2 CCTL Commands finished. */
	while((REG64_LO(&DEV_L2C->CORECCTL[0].CCTLSTATUS) & CCTL_ST_MASK)
		== CCTL_ST_RUNNING);
}

/* High-Level Cache APIs */

void nds_icache_invalidate_range(unsigned long start, unsigned long size)
{
	unsigned long saved_gie = GIE_SAVE();
	nds_l1c_icache_invalidate_range(start, size);
	GIE_RESTORE(saved_gie);
}

/*
 * nds_icache_invalidate_all(void)
 *
 * invalidate all I-cached data.
 */
void nds_icache_invalidate_all(void)
{
	unsigned long saved_gie = GIE_SAVE();
	nds_l1c_icache_invalidate_all();
	GIE_RESTORE(saved_gie);
}

void nds_dcache_writeback_range(unsigned long start, unsigned long size)
{
	unsigned long saved_gie = GIE_SAVE();
	nds_l1c_dcache_writeback_range(start, size);
	nds_l2c_cache_writeback_range(start, size);
	GIE_RESTORE(saved_gie);
}

void nds_dcache_invalidate_range(unsigned long start, unsigned long size)
{
	unsigned long saved_gie = GIE_SAVE();
	nds_l2c_cache_invalidate_range(start, size);
	nds_l1c_dcache_invalidate_range(start, size);
	GIE_RESTORE(saved_gie);
}

void nds_dcache_flush_range(unsigned long start, unsigned long size)
{
	if (!has_l2_cache()) {
		/* Only handle L1 cache */
		unsigned long saved_gie = GIE_SAVE();
		nds_l1c_dcache_flush_range(start, size);
		GIE_RESTORE(saved_gie);
	}
	else {
		/* Handle L1 + L2 cache */
		unsigned long saved_gie = GIE_SAVE();
		nds_l1c_dcache_writeback_range(start, size);
		nds_l2c_cache_flush_range(start, size);
		nds_l1c_dcache_invalidate_range(start, size);
		GIE_RESTORE(saved_gie);
	}
}

void nds_dcache_flush_all(void)
{
	/*
	 * Note: To keep no data loss in full L1C invalidation, there
	 * shouldn't any store instruction to cacheable region between
	 * L1C writeback and L1C invalidation.
	 */

	if (!cache_info.is_init) {
		/*
		 * Make sure get_cache_info() isn't called between CCTL
		 * operations because this function does some store
		 * instructions.
		 */
		get_cache_info();
	}

	if (!cache_info.has_l2_cache) {
		/* Only handle L1 cache */
		unsigned long saved_gie = GIE_SAVE();
		nds_l1c_dcache_flush_all();
		GIE_RESTORE(saved_gie);
	} else {
		/* Handle L1 + L2 cache */
		unsigned long saved_gie = GIE_SAVE();
		nds_l1c_dcache_writeback_all();
		nds_l2c_cache_flush_all();
		nds_l1c_dcache_invalidate_all();
		GIE_RESTORE(saved_gie);
	}
}

static ALWAYS_INLINE void nds_dcache_invalidate_addr(unsigned long addr)
{
	nds_dcache_invalidate_range(addr, 1);
}

static ALWAYS_INLINE void nds_dcache_flush_addr(unsigned long addr)
{
	nds_dcache_flush_range(addr, 1);
}

static ALWAYS_INLINE void unaligned_cache_line_move(unsigned char* src, unsigned char* dst, unsigned long len)
{
	int i;
	unsigned char* src_p = src;
	unsigned char* dst_p = dst;

	for (i = 0; i < len; ++i)
		*(dst_p+i)=*(src_p+i);
}

static void nds_dcache_invalidate_partial_line(unsigned long start, unsigned long end)
{
	unsigned long line_size = cache_line_size();
	unsigned char buf[line_size];

	unsigned long aligned_start = ROUND_DOWN(start, line_size);
	unsigned long aligned_end   = ROUND_UP(end, line_size);
	unsigned long end_offset    = end & (line_size-1);

	/* handle cache line unaligned */
	if (aligned_start < start) {
		unaligned_cache_line_move((unsigned char*)aligned_start, buf, start - aligned_start);
	}
	if (end < aligned_end) {
		unaligned_cache_line_move((unsigned char*)end, buf + end_offset, aligned_end - end);
	}

	nds_dcache_invalidate_addr(start);

	/* handle cache line unaligned */
	if (aligned_start < start) {
		unaligned_cache_line_move(buf, (unsigned char*)aligned_start, start - aligned_start);
	}
	if (end < aligned_end) {
		unaligned_cache_line_move(buf + end_offset, (unsigned char*)end, aligned_end - end);
	}
}

void nds_dma_writeback_range(unsigned long start, unsigned long size)
{
	if (has_dma_coherency()) {
		return;
	}

	nds_dcache_writeback_range(start, size);
}

/*
 * nds_dma_invalidate_range(start, size)
 *
 * Invalidate D-Cache of specified unaligned region. It's also acceptable to
 * writeback D-Cache instead of invalidation at unaligned boundary in region.
 */
void nds_dma_invalidate_range(unsigned long start, unsigned long size)
{
	if (has_dma_coherency()) {
		return;
	}

	unsigned long saved_gie = GIE_SAVE();

	unsigned long line_size = cache_line_size();
	unsigned long end = start + size;
	unsigned long aligned_start = ROUND_UP(start, line_size);
	unsigned long aligned_end   = ROUND_DOWN(end, line_size);

	if (aligned_start > aligned_end) {
		nds_dcache_flush_addr(start);
	}
	else {
		if (start < aligned_start) {
			nds_dcache_flush_addr(start);
		}
		if (aligned_start < aligned_end) {
			nds_dcache_invalidate_range(aligned_start, aligned_end - aligned_start);
		}
		if (aligned_end < end) {
			nds_dcache_flush_addr(end);
		}
	}

	GIE_RESTORE(saved_gie);
}

/*
 * nds_dma_invalidate_range2(start, size)
 *
 * Invalidate D-Cache of specified unaligned region. It's NOT acceptable to
 * writeback D-Cache at unaligned boundary in region.
 */
void nds_dma_invalidate_range2(unsigned long start, unsigned long size)
{
	if (has_dma_coherency()) {
		return;
	}

	unsigned long saved_gie = GIE_SAVE();

	unsigned long line_size = cache_line_size();
	unsigned long end = start + size;
	unsigned long aligned_start = ROUND_UP(start, line_size);
	unsigned long aligned_end   = ROUND_DOWN(end, line_size);

	if (aligned_start > aligned_end) {
		nds_dcache_invalidate_partial_line(start, end);
	}
	else {
		if (start < aligned_start) {
			nds_dcache_invalidate_partial_line(start, aligned_start);
		}
		if (aligned_start < aligned_end) {
			nds_dcache_invalidate_range(aligned_start, aligned_end - aligned_start);
		}
		if (aligned_end < end) {
			nds_dcache_invalidate_partial_line(aligned_end, end);
		}
	}

	GIE_RESTORE(saved_gie);
}
