#include "FreeRTOS.h"

#if( configUSE_ANDES_TRACER == 1 )

#include <nds_intrinsic.h>

#include "transfer.h"
#include "cache.h"

/* Macros for CCTL-related CSRs fields */
#define prvCCTL_L1D_VA_INVAL		( 0 )
#define prvCCTL_L1D_VA_WB			( 1 )
#define prvCCTL_L1D_VA_WBINVAL		( 2 )
#define prvCCTL_L1D_WBINVAL_ALL		( 6 )

/* Macros for PMA-related CSRs fields */
#define prvPMA_NAMO_OFF				(  0 << 6 )
#define prvPMA_NAMO_ON				(  1 << 6 )
#define prvPMA_MTYP_DEVICE			(  1 << 2 )
#define prvPMA_MTYP_NONCACHEABLE	(  3 << 2 )
#define prvPMA_MTYP_CACHEABLE		( 11 << 2 )
#define prvPMA_ETYP_NAPOT			(  3 )
#define prvPMA_ETYP_TOR				(  1 )
#define prvPMA_ETYP_PMA_OFF			(  0 )

/* Macros for Misc CSRs fields */
#define prvMMSC_CFG_CCTLCSR			( 0x1 << 16 )
#define prvMMSC_CFG_PPMA			( 0x1 << 30 )

/* Macros for CSR helpers */
#define prvTO_PMA_ADDR( addr )		( ( addr ) >> 2 )
#define prvNAPOT_BASE( base, size )	prvTO_PMA_ADDR( prvROUND_DOWN( base, size ) )
#define prvNAPOT_SIZE( size )		prvTO_PMA_ADDR( ( ( size ) - 1 ) >> 1 )
#define prvNAPOT( base, size )		( prvNAPOT_BASE( base, size ) | prvNAPOT_SIZE( size ) )

/* Macros for TransferCtrlBlock_t */
#define prvSTATUS_CCTL_EN			( 0x1 )

#define prvSTATUS_STATE_MASK		( 0x7 << 1 )

/* Transfer Control Block is in the init process. */
#define prvSTATUS_STATE_INIT		( 0x1 << 1 )

/* Transfer Control Block is ready for use by host. */
#define prvSTATUS_STATE_READY		( 0x2 << 1 )

/* Macros for TransferConfig_t */
#define prvCACHE_MODE_NONE			( 0 )
#define prvCACHE_MODE_CCTL			( 1 )
#define prvCACHE_MODE_PMA			( 2 )

/* Macros for save/restore global interrupt (mstatus.mie) */
#define prvMIE_SAVE()				prvMieSave()
#define prvMIE_RESTORE( x )			prvMieRestore( x )

#define prvMEMCPY( des, src, n )	__builtin_memcpy ( ( des ), ( src ), ( n ) )
#define prvMEMSET( s, c, n )		__builtin_memset ( ( s ), ( c ), ( n ) )

/* Macros for explicit conversion between pointer and integer. */
#define prvPTR_TO_UINT( x )			( ( uintptr_t )( x ) )
#define prvUINT_TO_PTR( x )			( ( void * )( uintptr_t )( x ) )

/*
 * Macros for cache alignment
 */

/* Value of x rounded up to the next multiple of align. align must be power of 2. */
#define prvROUND_UP( x, align )		( ( ( x ) + ( align ) - 1 ) & ~( ( align ) - 1 ) )
/* Value of x rounded down to the prev multiple of align. align must be power of 2. */
#define prvROUND_DOWN( x, align )	( ( x ) & ~( ( align ) - 1 ) )

/* Do the cache-alignment to the buffer. */
#define prvCACHE_ALIGN				__attribute__(( aligned( tracerCACHE_LINE_SIZE ) ))

/*
 * Macros for cache-aligned size and padding
 */

/* TransferCtrlBlock_t should be 8-byte alignment */
#define prvTRANS_CTRL_ALIGN			 __attribute__(( aligned( 8 ) ))

#define prvLOG_BUFFER_ALIGNED_SIZE	prvROUND_UP( configTRACER_LOG_BUFFER_SIZE, tracerCACHE_LINE_SIZE )
#define prvCMD_BUFFER_ALIGNED_SIZE	prvROUND_UP( tracerCMD_BUFFER_SIZE, tracerCACHE_LINE_SIZE )

#define prvTARGET_BLOCK_PADDING		prvROUND_UP( sizeof( TargetBlock_t ), tracerCACHE_LINE_SIZE )
#define prvHOST_BLOCK_PADDING		prvROUND_UP( sizeof( HostBlock_t ), tracerCACHE_LINE_SIZE )

/*
 * Note: PMA region alignment requirements:
 *
 * PMA NAPOT mode's region size should be power-of-2, and region base should
 * be aligned to size.
 *
 * The region size computation is hard-coded by SW and maximum size supported
 * is hard-coded to 16K. If region size is larger than maximum size supported,
 * Transfer Control Block should fallback to PMA TOR mode or CCTL mode.
 *
 * Currently, HW supports minimal alignment of NAPOT mode is 4K and of TOR mode
 * is 4-byte.
 */
#define prvPMA_NAPOT_MIN_ALIGN			( 0x1000 )

#define prvPMA_NAPOT_ALIGN_SUPPORTED	( ( sizeof( UncacheBuffer_t ) <= 0x4000 ) ? 1 : 0 )
#define prvPMA_ALIGN_SIZE				( ( sizeof( UncacheBuffer_t ) <= 0x1000 ) ? 0x1000 : \
										  ( sizeof( UncacheBuffer_t ) <= 0x2000 ) ? 0x2000 : \
										  ( sizeof( UncacheBuffer_t ) <= 0x4000 ) ? 0x4000 : 1 )

/* Uncache Buffer: PMA region base/size alignment */
#define prvUNCACHE_BUFFER_ALIGN			prvTRANS_CTRL_ALIGN __attribute__(( aligned( prvPMA_ALIGN_SIZE ) ))
#define prvUNCACHE_BUFFER_PADDING		prvROUND_UP( sizeof( UncacheBuffer_t ), prvPMA_ALIGN_SIZE )

/*-----------------------------------------------------------*/

/*
 * TransferCtrlBlock struct and it's struct members definitions
 *
 * TransferCtrlBlock is accessed by the host, so the ABI of TransferCtrlBlock
 * is the protocol between target and host program.
 *
 * Note: head & tail pointer should be 4/8 bytes-alignment in RV32/64, so
 * both target and host can access these pointers in the atomic way.
 */
typedef struct
{
	uint64_t ullBufStart;
	uint8_t ucBufSize[ 8 ];
	uint8_t ucHead[ 8 ];
	volatile uint8_t ucTail[ 8 ];
} LogBuffer_t;

typedef struct
{
	uint64_t ullBufStart;
	uint8_t ucBufSize[ 8 ];
	volatile uint8_t ucHead[ 8 ];
	uint8_t ucTail[ 8 ];
} CmdBuffer_t;

typedef struct __attribute__ (( packed )) prvTRANS_CTRL_ALIGN
{
	uint8_t ucID[ 16 ];
	union {
		volatile uint8_t *pucStartFlagPtr;
		uint64_t ullPadding;
	} u;
	uint32_t ulStatus;
	uint8_t ucReserved[ 4 ];
	LogBuffer_t xLogBuffer;
	CmdBuffer_t xCmdBuffer;
} TransferCtrlBlock_t;

/* Config of Transfer Control Block from HW or API arguments */
typedef struct
{
	uint8_t ucCacheEnable;
	uint8_t ucCacheMode;
} TransferConfig_t;

/* TargetBlock_t and HostBlock_t are only used in CCTL mode */
typedef struct __attribute__ (( packed, aligned( 8 ) ))
{
	uint8_t ucLogHead[ 8 ];
	uint8_t ucCmdTail[ 8 ];
} TargetBlock_t;

typedef union prvCACHE_ALIGN
{
	TargetBlock_t xData;
	uint8_t ucPadding[ prvTARGET_BLOCK_PADDING ];
} TargetBlockAlign_t;

typedef struct __attribute__ (( packed, aligned( 8 ) ))
{
	volatile uint8_t ucStartFlag;
	uint8_t ucReserved[ 7 ];
	uint8_t ucLogTail[ 8 ];
	uint8_t ucCmdHead[ 8 ];
} HostBlock_t;

typedef union prvCACHE_ALIGN
{
	HostBlock_t xData;
	uint8_t ucPadding[ prvHOST_BLOCK_PADDING ];
} HostBlockAlign_t;

/* Common struct of head/tail pointer to access both 2 layouts in same way. */
typedef struct
{
	uint64_t ullBufStart;
	uintptr_t uxBufSize;
	/* Touched by target */
	uintptr_t *puxLogHead;
	/* Touched by host */
	volatile uintptr_t *puxLogTail;
} LogBufferManage_t;

typedef struct
{
	uint64_t ullBufStart;
	uintptr_t uxBufSize;
	/* Touched by host */
	volatile uintptr_t *puxCmdHead;
	/* Touched by target */
	uintptr_t *puxCmdTail;
} CmdBufferManage_t;

/* UncacheBuffer_t is the structure to gather all data and structs should be uncached in PMA mode */
typedef struct
{
	/* Transfer Control Block should be 1st member of UncacheBuffer_t so the symbol
	__rtos_tracer_transfer_ctrl_block will point to it */
	TransferCtrlBlock_t xTransfer;

	/*
	 * Log and Command Buffer
	 *
	 * Note: Base and size should be cache-alignment in CCTL mode.
	 */
	prvCACHE_ALIGN uint8_t ucLogBuffer[ prvLOG_BUFFER_ALIGNED_SIZE ];
	prvCACHE_ALIGN volatile uint8_t ucCommandBuffer[ prvCMD_BUFFER_ALIGNED_SIZE ];

	/*
	 * HostBlock: only host can write access to it.
	 *
	 * Note: Base and size should be cache-alignment in CCTL mode.
	 */
	prvCACHE_ALIGN HostBlockAlign_t xHostBlock;
} UncacheBuffer_t;

/* UncacheBufferAlign_t should be a PMA region so that it should fit
PMA region alignment requirements. */
typedef union prvUNCACHE_BUFFER_ALIGN
{
	UncacheBuffer_t xData;
	uint8_t ucPadding[ prvUNCACHE_BUFFER_PADDING ];
} UncacheBufferAlign_t;

/*
 * Transfer Control Block (in the Uncache Buffer) and it's config
 */
static UncacheBufferAlign_t __attribute__(( section( ".data.pma" ) )) __rtos_tracer_transfer_ctrl_block = {
	.xData = {
		.xTransfer = {
			.ucID = {},
			.u = { .pucStartFlagPtr = &( __rtos_tracer_transfer_ctrl_block.xData.xHostBlock.xData.ucStartFlag ), },
			.ulStatus = prvSTATUS_STATE_INIT,
		},

		/* Initialize 3 fields to zero. */
		.xHostBlock = { .xData = {
			.ucStartFlag = 0,
			.ucLogTail = {},
			.ucCmdHead = {},
		}, },
	},
};

static TransferCtrlBlock_t *pxTracerTCB = &( __rtos_tracer_transfer_ctrl_block.xData.xTransfer );
static TransferConfig_t xTransferConfig = { 0, prvCACHE_MODE_NONE };

/*
 * TargetBlock: only target can write access to it.
 *
 * Note: It is only used in CCTL mode.
 * Note: Base and size should be cache-alignment in CCTL mode.
 */
static prvCACHE_ALIGN TargetBlockAlign_t xTargetBlock;

static LogBufferManage_t xLogBufferManage;
static CmdBufferManage_t xCmdBufferManage;

/*-----------------------------------------------------------*/

static portFORCE_INLINE UBaseType_t prvMieSave()
{
UBaseType_t uxSavedStatusValue;

	__asm volatile( "csrrc %0, mstatus, 0x8":"=r"( uxSavedStatusValue ) );
	return uxSavedStatusValue;
}

static portFORCE_INLINE void prvMieRestore( UBaseType_t uxSavedStatusValue )
{
	__asm volatile( "csrw mstatus, %0"::"r"( uxSavedStatusValue ) );
}

/* Cache API helpers: only send cache operation in CCTL mode */
static portFORCE_INLINE void prvDcacheInvalidateRangeCCTL( UBaseType_t uxBase, UBaseType_t uxSize  )
{
	if( xTransferConfig.ucCacheMode == prvCACHE_MODE_CCTL )
		nds_dcache_invalidate_range( uxBase, uxSize );
}

static portFORCE_INLINE void prvDcacheInvalidateAddrCCTL( UBaseType_t uxAddr )
{
	if( xTransferConfig.ucCacheMode == prvCACHE_MODE_CCTL )
		nds_dcache_invalidate_range( uxAddr, 1 );
}

static portFORCE_INLINE void prvDcacheFlushAllCCTL()
{
	if( xTransferConfig.ucCacheMode == prvCACHE_MODE_CCTL )
		nds_dcache_flush_all();
}

static portFORCE_INLINE void prvDcacheWritebackRangeCCTL( UBaseType_t uxBase, UBaseType_t uxSize )
{
	if( xTransferConfig.ucCacheMode == prvCACHE_MODE_CCTL )
		nds_dcache_writeback_range( uxBase, uxSize );
}

/*
 * Transfer Control Block implementation.
 */
static portFORCE_INLINE BaseType_t prvIsRingBufferFull( uintptr_t uxHead, uintptr_t uxTail, uintptr_t uxSize )
{
	if( ( uxHead + 1 ) == uxTail )
	{
		return pdTRUE;
	}
	else if( ( ( uxHead + 1 ) == uxSize ) && ( uxTail == 0 ) )
	{
		return pdTRUE;
	}
	else
	{
		return pdFALSE;
	}
}

static portFORCE_INLINE BaseType_t prvIsRingBufferEmpty( uintptr_t uxHead, uintptr_t uxTail )
{
	if( uxHead == uxTail )
	{
		return pdTRUE;
	}
	else
	{
		return pdFALSE;
	}
}

/*
 * Note: cache invalidate policy in CCTL mode
 *
 * We should do cache invalidate after reading any buffer that can be
 * written by the host, so that we can prevent cache writeback by HW's cache
 * replacement policy (e.g. LRU) from polluting host-written data in the DDR.
 *
 * Also, we don't need to do cache invalidate before reading buffer because
 * the host-written buffer should be kicked out from cacheline immediately
 * and not in the cacheline at this time.
 */
static BaseType_t prvWriteLogBuffer( LogBufferManage_t *pxLogBuf, uint8_t *pucData,
	UBaseType_t uxSize )
{
BaseType_t xReturn;
UBaseType_t uxSavedStatus;
uint64_t ullBufStart;
uintptr_t uxBufSize, uxHead, uxTail;

	ullBufStart = pxLogBuf->ullBufStart;
	uxBufSize = pxLogBuf->uxBufSize;

	uxSavedStatus = prvMIE_SAVE();
	{
		uxHead = *( pxLogBuf->puxLogHead );

		/* Host-written buffer */
		prvDcacheInvalidateAddrCCTL( prvPTR_TO_UINT( pxLogBuf->puxLogTail ) );
		uxTail = *( pxLogBuf->puxLogTail );

		if( prvIsRingBufferFull( uxHead, uxTail, uxBufSize ) == pdTRUE )
		{
			xReturn = pdFALSE;
		}
		else if( uxTail <= uxHead )
		{
			if( ( uxHead + uxSize ) < uxBufSize )
			{
				prvMEMCPY( prvUINT_TO_PTR( ullBufStart + uxHead ), pucData, uxSize );
				prvDcacheWritebackRangeCCTL( ullBufStart + uxHead, uxSize );

				*( pxLogBuf->puxLogHead ) = uxHead + uxSize;
				prvDcacheWritebackRangeCCTL( prvPTR_TO_UINT( pxLogBuf->puxLogHead ), 8 );

				xReturn = pdTRUE;
			}
			else if( ( ( uxHead + uxSize ) - uxBufSize ) < uxTail )
			{
			uintptr_t uxFirstSize = uxBufSize - uxHead;
			uintptr_t uxSecondSize = uxSize - uxFirstSize;

				prvMEMCPY( prvUINT_TO_PTR( ullBufStart + uxHead ), pucData, uxFirstSize );
				prvDcacheWritebackRangeCCTL( ullBufStart + uxHead, uxFirstSize );

				if( uxSecondSize != 0 )
				{
					prvMEMCPY( prvUINT_TO_PTR( ullBufStart ), pucData + uxFirstSize, uxSecondSize );
					prvDcacheWritebackRangeCCTL( ullBufStart, uxSecondSize );
				}

				*( pxLogBuf->puxLogHead ) = uxSecondSize;
				prvDcacheWritebackRangeCCTL( prvPTR_TO_UINT( pxLogBuf->puxLogHead ), 8 );

				xReturn = pdTRUE;
			}
			else
			{
				xReturn = pdFALSE;
			}
		}
		else if( ( uxHead + uxSize ) < uxTail )
		{
			prvMEMCPY( prvUINT_TO_PTR( ullBufStart + uxHead ), pucData, uxSize );
			prvDcacheWritebackRangeCCTL( ullBufStart + uxHead, uxSize );

			*( pxLogBuf->puxLogHead ) = uxHead + uxSize;
			prvDcacheWritebackRangeCCTL( prvPTR_TO_UINT( pxLogBuf->puxLogHead ), 8 );

			xReturn = pdTRUE;
		}
		else
		{
			xReturn = pdFALSE;
		}
	}
	prvMIE_RESTORE( uxSavedStatus );

	return xReturn;
}

BaseType_t xWriteLogBuffer( uint8_t *pucData, UBaseType_t uxSize )
{
	return prvWriteLogBuffer( &xLogBufferManage, pucData, uxSize );
}

static BaseType_t prvReadCommandBuffer( CmdBufferManage_t *pxCmdBuf, uint8_t *pucData, UBaseType_t uxSize )
{
BaseType_t xReturn;
UBaseType_t uxSavedStatus;
uint64_t ullBufStart;
uintptr_t uxBufSize, uxHead, uxTail;

	ullBufStart = pxCmdBuf->ullBufStart;
	uxBufSize = pxCmdBuf->uxBufSize;

	uxSavedStatus = prvMIE_SAVE();
	{
		/* Host-written buffer */
		prvDcacheInvalidateAddrCCTL( prvPTR_TO_UINT( pxCmdBuf->puxCmdHead ) );
		uxHead = *( pxCmdBuf->puxCmdHead );

		uxTail = *( pxCmdBuf->puxCmdTail );

		if( prvIsRingBufferEmpty( uxHead, uxTail ) == pdTRUE )
		{
			xReturn = pdFALSE;
		}
		else if( ( uxTail + uxSize ) <= uxHead )
		{
			/* Host-written buffer */
			prvDcacheInvalidateRangeCCTL( ullBufStart + uxTail, uxSize );
			prvMEMCPY( pucData, prvUINT_TO_PTR( ullBufStart + uxTail ), uxSize );

			uxTail = ( uxTail + uxSize );
			*( pxCmdBuf->puxCmdTail ) = uxTail;
			prvDcacheWritebackRangeCCTL( prvPTR_TO_UINT( pxCmdBuf->puxCmdTail ), 8 );

			xReturn = pdTRUE;
		}
		else if( uxHead < uxTail )
		{
			if( ( uxTail + uxSize ) < uxBufSize )
			{
				/* Host-written buffer */
				prvDcacheInvalidateRangeCCTL( ullBufStart + uxTail, uxSize );
				prvMEMCPY( pucData, prvUINT_TO_PTR( ullBufStart + uxTail ), uxSize );

				*( pxCmdBuf->puxCmdTail ) = ( uxTail + uxSize );
				prvDcacheWritebackRangeCCTL( prvPTR_TO_UINT( pxCmdBuf->puxCmdTail ), 8 );

				xReturn = pdTRUE;
			}
			else if( ( ( uxTail + uxSize ) - uxBufSize ) <= uxHead )
			{
			uintptr_t uxFirstSize = uxBufSize - uxTail;
			uintptr_t uxSecondSize = uxSize - uxFirstSize;

				/* Host-written buffer */
				prvDcacheInvalidateRangeCCTL( ullBufStart + uxTail, uxFirstSize );
				prvMEMCPY( pucData, prvUINT_TO_PTR( ullBufStart + uxTail ), uxFirstSize );

				if( uxSecondSize != 0 )
				{
					/* Host-written buffer */
					prvDcacheInvalidateRangeCCTL( ullBufStart, uxSecondSize );
					prvMEMCPY( pucData + uxFirstSize, prvUINT_TO_PTR( ullBufStart ), uxSecondSize );
				}

				*( pxCmdBuf->puxCmdTail ) = uxSecondSize;
				prvDcacheWritebackRangeCCTL( prvPTR_TO_UINT( pxCmdBuf->puxCmdTail ), 8 );

				xReturn = pdTRUE;
			}
			else
			{
				xReturn = pdFALSE;
			}
		}
		else
		{
			xReturn = pdFALSE;
		}
	}
	prvMIE_RESTORE( uxSavedStatus );

	return xReturn;

}

BaseType_t xReadCommandBuffer( uint8_t *pucData, UBaseType_t uxSize )
{
	return prvReadCommandBuffer( &xCmdBufferManage, pucData, uxSize );
}

static portFORCE_INLINE BaseType_t prvIsPMARegionValidNAPOT( unsigned long uxBase,
	unsigned long uxSize )
{
	/* PMA NAPOT mode's region size should be power-of-2, and region base should
	be aligned to size. Currently, HW supports minimal alignment of NAPOT mode is 4K. */
	int is_valid = ( ( uxSize >= prvPMA_NAPOT_MIN_ALIGN ) &&
		( ( uxSize & ( uxSize - 1 ) ) == 0 ) &&
		( ( uxBase & ( uxSize - 1 ) ) == 0 ) );

	if( is_valid )
	{
		return pdTRUE;
	}
	return pdFALSE;
}

static portFORCE_INLINE BaseType_t prvCheckAndSetPMA()
{
unsigned long uxMmscCfg;

	/* Check if HW supports PMA */
	__asm volatile( "csrr %0, mmsc_cfg":"=r"( uxMmscCfg ) );
	if( !( uxMmscCfg & prvMMSC_CFG_PPMA ) )
	{
		return pdFALSE;
	}

	unsigned long uxBase = prvPTR_TO_UINT( &__rtos_tracer_transfer_ctrl_block.xData );
	unsigned long uxSize = prvUNCACHE_BUFFER_PADDING;

	/* NAPOT mode */
	if( ( prvPMA_NAPOT_ALIGN_SUPPORTED ) && ( prvIsPMARegionValidNAPOT( uxBase, uxSize ) == pdTRUE ) )
	{
	unsigned long uxPmaCfg0;

		/* Before marking the region as uncache region, we should cache flush this region first. */
		nds_dcache_flush_range( uxBase, uxSize );

		/* Set PMA registers. */
		__asm volatile( "csrw pmaaddr0, %0"::"r"( prvNAPOT( uxBase, uxSize ) ) );

		__asm volatile( "csrr %0, pmacfg0" : "=r"( uxPmaCfg0 ) );

		uint8_t ucCfg0 = prvPMA_NAMO_OFF | prvPMA_MTYP_NONCACHEABLE | prvPMA_ETYP_NAPOT;
		unsigned long uxNewPmaCfg0 = ( ( uxPmaCfg0 & ~0xFF ) | ucCfg0 );

		__asm volatile( "csrw pmacfg0, %0"::"r"( uxNewPmaCfg0 ) );

		return pdTRUE;
	}

	return pdFALSE;
}

static portFORCE_INLINE BaseType_t prvCheckCCTL()
{
unsigned long uxMiscCfg;

	__asm volatile( "csrr %0, mmsc_cfg":"=r"( uxMiscCfg ) );

	if( ( uxMiscCfg & prvMMSC_CFG_CCTLCSR ) )
	{
		return pdTRUE;
	}
	else
	{
		return pdFALSE;
	}
}

int xInitTracerTCB( uint8_t ucCacheEnable )
{
unsigned long uxMdcmCfg;
	/* If cache is enabled, check supported HW features and
	choose the suitable cache handling method. */
	if( ucCacheEnable )
	{
		__asm volatile( "csrr %0, mdcm_cfg":"=r"( uxMdcmCfg ) );
		if( (uxMdcmCfg & 0x1C0) == 0 )
		{
			/* Error: User enable cache but HW doesn't have dcache */
			return 0;
		}

		BaseType_t xSupportCCTL = prvCheckCCTL();

		if( ( xSupportCCTL == pdTRUE ) && ( prvCheckAndSetPMA() == pdTRUE ) )
		{
			/* Use PMA mode */
			xTransferConfig.ucCacheEnable = 1;
			xTransferConfig.ucCacheMode = prvCACHE_MODE_PMA;
		}
		else if( xSupportCCTL == pdTRUE )
		{
			/* Use CCTL mode */
			xTransferConfig.ucCacheEnable = 1;
			xTransferConfig.ucCacheMode = prvCACHE_MODE_CCTL;
		}
		else
		{
			/* Error: HW doesn't support CCTL, Transfer Control Block
			couldn't support enabling cache. */
			return 0;
		}
	}

	/* Initialize CCTL_EN of status field */
	if( xTransferConfig.ucCacheMode == prvCACHE_MODE_CCTL )
	{
		pxTracerTCB->ulStatus |= prvSTATUS_CCTL_EN;
	}

	/* Initialize buffer start and size */
	pxTracerTCB->xLogBuffer.ullBufStart = prvPTR_TO_UINT( __rtos_tracer_transfer_ctrl_block.xData.ucLogBuffer );
	*( uintptr_t * )pxTracerTCB->xLogBuffer.ucBufSize = configTRACER_LOG_BUFFER_SIZE;

	pxTracerTCB->xCmdBuffer.ullBufStart = prvPTR_TO_UINT( __rtos_tracer_transfer_ctrl_block.xData.ucCommandBuffer );
	*( uintptr_t * )pxTracerTCB->xCmdBuffer.ucBufSize = tracerCMD_BUFFER_SIZE;

	HostBlockAlign_t *pxHostBlock = &( __rtos_tracer_transfer_ctrl_block.xData.xHostBlock );

	/* Initialize buffer head and tail */
	if( xTransferConfig.ucCacheMode == prvCACHE_MODE_CCTL )
	{
		/* Initialize head and tail pointer in Transfer Control Block */
		*( uintptr_t * )pxTracerTCB->xLogBuffer.ucHead = prvPTR_TO_UINT( xTargetBlock.xData.ucLogHead );
		*( uintptr_t * )pxTracerTCB->xLogBuffer.ucTail = prvPTR_TO_UINT( pxHostBlock->xData.ucLogTail );
		*( uintptr_t * )pxTracerTCB->xCmdBuffer.ucHead = prvPTR_TO_UINT( pxHostBlock->xData.ucCmdHead );
		*( uintptr_t * )pxTracerTCB->xCmdBuffer.ucTail = prvPTR_TO_UINT( xTargetBlock.xData.ucCmdTail );

		/* Clear head and tail */
		*( uintptr_t * )xTargetBlock.xData.ucLogHead = 0;
		*( uintptr_t * )xTargetBlock.xData.ucCmdTail = 0;
	}
	else
	{
		/* Clear head and tail */
		*( uintptr_t * )pxTracerTCB->xLogBuffer.ucHead = 0;
		*( uintptr_t * )pxTracerTCB->xLogBuffer.ucTail = 0;
		*( uintptr_t * )pxTracerTCB->xCmdBuffer.ucHead = 0;
		*( uintptr_t * )pxTracerTCB->xCmdBuffer.ucTail = 0;
	}

	/* Initialize BufferManage struct for target usage */
	xLogBufferManage.ullBufStart = pxTracerTCB->xLogBuffer.ullBufStart;
	xLogBufferManage.uxBufSize = *( uintptr_t * )pxTracerTCB->xLogBuffer.ucBufSize;
	xCmdBufferManage.ullBufStart = pxTracerTCB->xCmdBuffer.ullBufStart;
	xCmdBufferManage.uxBufSize = *( uintptr_t * )pxTracerTCB->xCmdBuffer.ucBufSize;

	if( xTransferConfig.ucCacheMode == prvCACHE_MODE_CCTL )
	{
		xLogBufferManage.puxLogHead = ( uintptr_t * )xTargetBlock.xData.ucLogHead;
		xLogBufferManage.puxLogTail = ( uintptr_t * )pxHostBlock->xData.ucLogTail;
		xCmdBufferManage.puxCmdHead = ( uintptr_t * )pxHostBlock->xData.ucCmdHead;
		xCmdBufferManage.puxCmdTail = ( uintptr_t * )xTargetBlock.xData.ucCmdTail;
	}
	else
	{
		xLogBufferManage.puxLogHead = ( uintptr_t * )pxTracerTCB->xLogBuffer.ucHead;
		xLogBufferManage.puxLogTail = ( uintptr_t * )pxTracerTCB->xLogBuffer.ucTail;
		xCmdBufferManage.puxCmdHead = ( uintptr_t * )pxTracerTCB->xCmdBuffer.ucHead;
		xCmdBufferManage.puxCmdTail = ( uintptr_t * )pxTracerTCB->xCmdBuffer.ucTail;
	}

	/* Finish the Initialization of Transfer Control Block. */

	/* In CCTL mode, flush xHostBlock & ucCommandBuffer before host could write to them
	to prevent from polluting host-written data by HW cache replacement. */
	prvDcacheFlushAllCCTL();

	/* Set ID to Transfer Control Block */
	prvMEMCPY( pxTracerTCB->ucID, "_TRANS", 6 );
	prvMEMCPY( pxTracerTCB->ucID + 6, "FER_B", 5 );
	prvMEMCPY( pxTracerTCB->ucID + 11, "LOCK\0", 5 );

	/* Writeback ucID to host in CCTL mode */
	prvDcacheWritebackRangeCCTL( ( UBaseType_t )pxTracerTCB->ucID, sizeof( pxTracerTCB->ucID ) );

	/* Tell to host that Transfer Control Block is ready. */
	uint32_t ulStatus = pxTracerTCB->ulStatus;
	ulStatus &= ~prvSTATUS_STATE_MASK;
	ulStatus |= prvSTATUS_STATE_READY;
	pxTracerTCB->ulStatus = ulStatus;

	/* Writeback ulStatus to host in CCTL mode */
	prvDcacheWritebackRangeCCTL( prvPTR_TO_UINT( &( pxTracerTCB->ulStatus ) ), sizeof( pxTracerTCB->ulStatus ) );

	return 1;
}

uint8_t ucReadStartFlag( void )
{
	return *( pxTracerTCB->u.pucStartFlagPtr );
}
#endif /* ( configUSE_ANDES_TRACER == 1 ) */
