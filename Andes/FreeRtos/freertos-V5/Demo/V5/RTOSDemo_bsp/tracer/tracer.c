#include "FreeRTOS.h"

#if( configUSE_ANDES_TRACER == 1 )

#include <string.h>

#include "transfer.h"
#include "queue.h"
#include "semphr.h"

/*
 * Macros of Event ID
 */

/* System Events */
#define prvEVT_ID_TRACE_START				( 0u )
#define prvEVT_ID_TRACE_STOP				( 1u )
#define prvEVT_ID_TRACE_START_INFO_FINISHED	( 2u )
#define prvEVT_ID_BUFFER_OVERFLOW			( 3u )
#define prvEVT_ID_SYSTEM_DESC				( 4u )
#define prvEVT_ID_TASK_INFO					( 5u )
#define prvEVT_ID_IDLE_TASK_INFO			( 6u )
#define prvEVT_ID_OBJECT_NAME				( 7u )
#define prvEVT_ID_CURRENT_CONTEXT			( 8u )
#define prvEVT_ID_EXCEPTION_ENTER			( 9u )
#define prvEVT_ID_EXCEPTION_EXIT			( 10u )
#define prvEVT_ID_ISR_ENTER					( 11u )
#define prvEVT_ID_ISR_EXIT					( 12u )
#define prvEVT_ID_TICK_ISR_ENTER			( 13u )
#define prvEVT_ID_TICK_ISR_EXIT				( 14u )

/* RTOS Events */
#define prvEVT_ID_TASK_SWITCH_IN			( 32u )
#define prvEVT_ID_TASK_CREATE				( 33u )
#define prvEVT_ID_TASK_DELETE				( 34u )
#define prvEVT_ID_QUEUE_SEND				( 35u )
#define prvEVT_ID_QUEUE_SEND_FROM_ISR		( 36u )
#define prvEVT_ID_QUEUE_RECV				( 37u )
#define prvEVT_ID_QUEUE_RECV_FROM_ISR		( 38u )
#define prvEVT_ID_QUEUE_PEEK				( 39u )
#define prvEVT_ID_QUEUE_PEEK_FROM_ISR		( 40u )
#define prvEVT_ID_SEMAPHORE_TAKE			( 41u )
#define prvEVT_ID_SEMAPHORE_TAKE_FROM_ISR	( 42u )
#define prvEVT_ID_SEMAPHORE_GIVE			( 43u )
#define prvEVT_ID_SEMAPHORE_GIVE_FROM_ISR	( 44u )

/* Max length of Task name */
#define prvMAX_TASK_NAME_SIZE				( 256 )

/* Max length of Object name */
#define prvMAX_OBJECT_NAME_SIZE				( 256 )

/*
 * Max size of event data: ID + Data + TimestampDelta
 *
 * Max Data: TASK_INFO event.
 * Max TimestampDelta: uint64_t are at most 10 bytes in LEB128 encoding
 */
#define prvMAX_EVENT_DATA_SIZE				( 1 + ( 1 + 8 + 2 + prvMAX_TASK_NAME_SIZE ) + 10 )
#define prvMAX_OVF_EVENT_SIZE				( 1 + 4 + 10 )

/* Macro of Queue OPs (also includes Semaphore/Mutex) */
#define prvOP_API_MASK			( 0xf )
#define prvOP_QUEUE_SEND		( 0x0 )
#define prvOP_QUEUE_RECV		( 0x1 )
#define prvOP_QUEUE_PEEK		( 0x2 )
#define prvOP_SEMAPHORE_TAKE	( prvOP_QUEUE_RECV )
#define prvOP_SEMAPHORE_GIVE	( prvOP_QUEUE_SEND )

#define prvOP_CTX_SHIFT			( 4 )
#define prvOP_CTX_MASK			( 0x3 << prvOP_CTX_SHIFT )
#define prvOP_FROM_TASK			( 0x0 << prvOP_CTX_SHIFT )
#define prvOP_FROM_ISR			( 0x1 << prvOP_CTX_SHIFT )

/* Macro of Context Type */
#define prvCONTEXT_NONE			( 0 )
#define prvCONTEXT_TASK			( 1 )
#define prvCONTEXT_ISR			( 2 )
#define prvCONTEXT_TICK_ISR		( 3 )
#define prvCONTEXT_EXCEPTION	( 4 )
#define prvCONTEXT_IDLE			( 5 )
#define prvCONTEXT_KERNEL		( 6 )

/* Macro of Start Flag */
#define prvFLAG_TRACE_START		( 1 )
#define prvFLAG_TRACE_STOP		( 2 )

/* Macro of Host Command */
#define prvCMD_TRACE_START		( 1 )
#define prvCMD_TRACE_STOP		( 2 )
#define prvCMD_QUEUE_REGISTER   ( 3 )

/* CPU Arch */
#if __riscv_xlen == 64
	#define prvCPU_ARCH		( 1U )
#elif __riscv_xlen == 32
	#define prvCPU_ARCH		( 0U )
#else
	#error "Not RV32 and RV64 Arch, unsupported RISC-V architecture"
#endif

/* Macros for save/restore global interrupt (mstatus.mie) */
#define prvMIE_SAVE()					prvMieSave()
#define prvMIE_RESTORE( x )				prvMieRestore( x )

#define prvMEMCPY( des, src, n )		__builtin_memcpy ( ( des ), ( src ), ( n ) )
#define prvMEMSET( s, c, n )			__builtin_memset ( ( s ), ( c ), ( n ) )

/* Macros to access high and low part of 64-bit mtime registers in RV32 */
#if __riscv_xlen == 32
	#define prvREG64_HI( pxRegAddr )	( ( (volatile uint32_t *)( pxRegAddr ) )[1] )
	#define prvREG64_LO( pxRegAddr )	( ( (volatile uint32_t *)( pxRegAddr ) )[0] )
#endif

/*
 * LEB128 encoding
 */

/* prvLEB128_ENCODE_U64( uint8_t *pucDest , uint64_t *pullSrcValue ) */
#define prvLEB128_ENCODE_U64( pucDest, pullSrcValue )	\
{														\
uint8_t *pucPtr = ( pucDest );							\
uint64_t ullTemp = *( pullSrcValue );					\
														\
	while( ullTemp > 0x7f ) 							\
	{													\
		*pucPtr++ = ( ( ullTemp & 0x7f ) | 0x80 );		\
		ullTemp >>= 7;									\
	}													\
	*pucPtr++ = ( uint8_t ) ullTemp;					\
	pucDest = pucPtr;									\
}

#define prvEventAddField( pxEvent, Type, uxValue )		\
{														\
	*( Type * )( ( pxEvent )->pucNext ) = ( uxValue );	\
	( pxEvent )->pucNext += sizeof( Type );				\
}

/*-----------------------------------------------------------*/

typedef struct
{
	uint8_t ucType;
	uint16_t usId;
} TrapId_t;

typedef struct
{
	uint8_t *pucStart;
	uint8_t *pucNext;
} EventData_t;

typedef struct
{
	uintptr_t *puxTaskId;
	uint16_t usPriority;
	const char *pucName;
} TaskInfoEvent_t;

typedef struct
{
	uint8_t ucStructSize;
	uint8_t ucHandleOffset;
	uint8_t ucNameOffset;
} QueueRegistryCmdInfo_t;

extern TaskHandle_t volatile pxCurrentTCB;
extern PRIVILEGED_DATA uint8_t xQueueRegistry[];

static BaseType_t xTraceEnable = pdFALSE;
static uint64_t ullLastEventTimestamp = 0;
static uint8_t pucEventBuffer[ prvMAX_EVENT_DATA_SIZE ];
static uint8_t pucEventBufferOverflow[ prvMAX_OVF_EVENT_SIZE ];
static void *pvIdleTaskId = NULL;
static int xIrqLevel = 0;
static TrapId_t xCurrentTrapId = { prvCONTEXT_NONE };
static uint32_t ulNumOfTasks = 0;
static TaskInfoEvent_t xTaskInfoEventPool[ configTRACER_MAX_NUM_TASK_INFO ];
static uint32_t ulBufferOverflow = 0;
static QueueRegistryCmdInfo_t xQueueRegistryCmdInfo = { 0 };

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

/* prvReadMtime(): Read machine timer register.
Note: Always use this API to access mtime */
static portFORCE_INLINE uint64_t prvReadMtime( void )
{
volatile uint64_t * const pullMtimeRegister = ( volatile uint64_t * const ) ( configMTIME_BASE_ADDRESS );

	#if __riscv_xlen == 32
		uint32_t ulCurrentTimeHigh, ulCurrentTimeLow;
		do
		{
			ulCurrentTimeHigh = prvREG64_HI( pullMtimeRegister );
			ulCurrentTimeLow = prvREG64_LO( pullMtimeRegister );
		} while ( ulCurrentTimeHigh != prvREG64_HI( pullMtimeRegister ) );

		return ( ( ( uint64_t ) ulCurrentTimeHigh ) << 32 ) | ulCurrentTimeLow;
	#else
		return *pullMtimeRegister;
	#endif
}

/*
 * APIs of EventData_t
 */
static portFORCE_INLINE void prvEventInit( EventData_t *pxEvent, uint8_t ucEventId, uint8_t *pucEventBuf )
{
	pxEvent->pucNext = pxEvent->pucStart = pucEventBuf;

	prvEventAddField( pxEvent, uint8_t, ucEventId );
}

static portFORCE_INLINE void prvEventAddFieldU8( EventData_t *pxEvent, uint8_t ucValue )
{
	prvEventAddField( pxEvent, uint8_t, ucValue );
}

static portFORCE_INLINE void prvEventAddFieldU16( EventData_t *pxEvent, uint16_t usValue )
{
	prvEventAddField( pxEvent, uint16_t, usValue );
}

static portFORCE_INLINE void prvEventAddFieldU32( EventData_t *pxEvent, uint32_t ulValue )
{
	prvEventAddField( pxEvent, uint32_t, ulValue );
}

/* Comments unused helper function to fix warning -Wunused-function in LLVM. */
/*
static portFORCE_INLINE void prvEventAddFieldU64( EventData_t *pxEvent, uint64_t ullValue )
{
	prvEventAddField( pxEvent, uint64_t, ullValue );
}
*/

static portFORCE_INLINE void prvEventAddFieldPtr( EventData_t *pxEvent, uintptr_t uxValue )
{
	prvEventAddField( pxEvent, uintptr_t, uxValue );
}

static portFORCE_INLINE void prvEventAddFieldULong( EventData_t *pxEvent, unsigned long uxValue )
{
	prvEventAddField( pxEvent, unsigned long, uxValue );
}

static portFORCE_INLINE void prvEventAddFieldStr( EventData_t *pxEvent, const char *pcStr, uint32_t ulMaxLen )
{
	if( ( strlen( pcStr ) + 1 ) > ulMaxLen )
	{
		strncpy( ( char * )pxEvent->pucNext, pcStr, ulMaxLen );
		pxEvent->pucNext[ ulMaxLen - 1 ] = '\0';
		pxEvent->pucNext += ulMaxLen;
	}
	else
	{
		strcpy( ( char * )pxEvent->pucNext, pcStr );
		pxEvent->pucNext += ( strlen( pcStr ) + 1 );
	}
}

static portFORCE_INLINE void prvEventAddTimestamp( EventData_t *pxEvent, uint64_t *pullTimestamp )
{
uint64_t ullTimestampDelta;

	*pullTimestamp = prvReadMtime();
	ullTimestampDelta = *pullTimestamp - ullLastEventTimestamp;
	prvLEB128_ENCODE_U64( pxEvent->pucNext, &ullTimestampDelta );
}

/* prvEventSend(pxEvent): Send event data to host.
Return: 0 if success, -1 if fail. */
static int prvEventSend( EventData_t *pxEvent )
{
	if( xWriteLogBuffer( pxEvent->pucStart, ( UBaseType_t )( pxEvent->pucNext - pxEvent->pucStart ) ) == pdTRUE )
	{
		return 0;
	}
	else
	{
		return -1;
	}

}

/* prvBufferOverflowEventSend(): Send BUFFER_OVERFLOW event
Return: 0 if success, -1 if fail.
Note: this API can only be called when IRQ is disabled */
static portFORCE_INLINE int prvBufferOverflowEventSend()
{
EventData_t xEvent;
uint64_t ullEventTimestamp;
int xReturn = 0;

	if( ulBufferOverflow )
	{
		prvEventInit( &xEvent, prvEVT_ID_BUFFER_OVERFLOW, pucEventBufferOverflow );
		prvEventAddFieldU32( &xEvent, ulBufferOverflow );
		prvEventAddTimestamp( &xEvent, &ullEventTimestamp );
		xReturn = prvEventSend( &xEvent );
		if( xReturn == 0 )
		{
			ullLastEventTimestamp = ullEventTimestamp;
			ulBufferOverflow = 0;
		}
		else
		{
			ulBufferOverflow++;
		}
	}

	return xReturn;
}

/* prvTaskInfoEvent(): Send TASK_INFO event
Note: this API can only be called when IRQ is disabled */
static void prvTaskInfoEvent( void )
{
EventData_t xEvent;
uint64_t ullEventTimestamp;
uint8_t ucLength;
int xReturn;

	for( UBaseType_t i = 0; i < ulNumOfTasks; i++ )
	{
		if( xTaskInfoEventPool[ i ].puxTaskId )
		{
			if( prvBufferOverflowEventSend() )
			{
				continue;
			}

			/* Length + Task ID + Priority + Name  */
			ucLength = 1 + sizeof( uintptr_t ) + 2 + strlen( xTaskInfoEventPool[ i ].pucName ) + 1;

			prvEventInit( &xEvent, prvEVT_ID_TASK_INFO, pucEventBuffer );
			prvEventAddFieldU8( &xEvent, ucLength );
			prvEventAddFieldPtr( &xEvent, ( uintptr_t ) xTaskInfoEventPool[ i ].puxTaskId );
			prvEventAddFieldU16( &xEvent, xTaskInfoEventPool[ i ].usPriority );
			prvEventAddFieldStr( &xEvent, xTaskInfoEventPool[ i ].pucName, prvMAX_TASK_NAME_SIZE );
			prvEventAddTimestamp( &xEvent, &ullEventTimestamp );
			xReturn = prvEventSend( &xEvent );
			if( xReturn == 0 )
			{
				ullLastEventTimestamp = ullEventTimestamp;
			}
			else
			{
				ulBufferOverflow++;
			}
		}
	}
}

/* prvIdleTaskInfoEvent(): Send IDLE_TASK_INFO event
Note: this API can only be called when IRQ is disabled */
static void prvIdleTaskInfoEvent( void )
{
EventData_t xEvent;
uint64_t ullEventTimestamp;
int xReturn;

	if( pvIdleTaskId == NULL )
	{
		/* Don't send info if idle task isn't created (Scheduler doesn't start). */
		return;
	}

	if( prvBufferOverflowEventSend() )
	{
		return;
	}

	prvEventInit( &xEvent, prvEVT_ID_IDLE_TASK_INFO, pucEventBuffer );
	prvEventAddFieldPtr( &xEvent, ( uintptr_t ) pvIdleTaskId );
	prvEventAddTimestamp( &xEvent, &ullEventTimestamp );
	xReturn = prvEventSend( &xEvent );
	if( xReturn == 0 )
	{
		ullLastEventTimestamp = ullEventTimestamp;
	}
	else
	{
		ulBufferOverflow++;
	}
}

/* prvObjectNameEvent(): Send OBJECT_NAME event
Note: this API can only be called when IRQ is disabled */
static void prvObjectNameEvent( void )
{
EventData_t xEvent;
uint64_t ullEventTimestamp;
int xReturn;
uintptr_t uxHandle;
char *pcName;

	if( !xQueueRegistryCmdInfo.ucStructSize )
	{
		return;
	}

	for( UBaseType_t i = 0; i < configQUEUE_REGISTRY_SIZE; i++ )
	{
		uint8_t *pucQueueRegistryItem = xQueueRegistry + i * xQueueRegistryCmdInfo.ucStructSize;
		uxHandle = *( uintptr_t * )( pucQueueRegistryItem + xQueueRegistryCmdInfo.ucHandleOffset );
		pcName = *( char ** )( pucQueueRegistryItem + xQueueRegistryCmdInfo.ucNameOffset );

		if( uxHandle )
		{
			if( prvBufferOverflowEventSend() )
			{
				continue;
			}

			prvEventInit( &xEvent, prvEVT_ID_OBJECT_NAME, pucEventBuffer );
			prvEventAddFieldPtr( &xEvent, uxHandle );
			prvEventAddFieldStr( &xEvent, pcName, prvMAX_OBJECT_NAME_SIZE );
			prvEventAddTimestamp( &xEvent, &ullEventTimestamp );
			xReturn = prvEventSend( &xEvent );
			if( xReturn == 0 )
			{
				ullLastEventTimestamp = ullEventTimestamp;
			}
			else
			{
				ulBufferOverflow++;
			}
		}
	}
}

/* prvCurrentContextEvent(): Send CURRENT_CONTEXT event
Note: this API can only be called when IRQ is disabled */
static void prvCurrentContextEvent( void )
{
EventData_t xEvent;
uint64_t ullEventTimestamp;
uint8_t ucLength;
uint8_t ucCtxType;
uintptr_t uxCtxId;
int xReturn;

	if( prvBufferOverflowEventSend() )
	{
		return;
	}

	/* length + IRQ level + Context Type + Context ID */
	ucLength = 1 + 1 + 1 + sizeof( uintptr_t );

	if( xIrqLevel < 0 )
	{
		/* Error: IRQ level couldn't be lower than 0. */
		xIrqLevel = 0;
	}

	if( pxCurrentTCB )
	{
		ucCtxType = prvCONTEXT_TASK;
		uxCtxId = ( uintptr_t )pxCurrentTCB;
	}
	else
	{
		ucCtxType = prvCONTEXT_KERNEL;
		uxCtxId = 0;
	}

	prvEventInit( &xEvent, prvEVT_ID_CURRENT_CONTEXT, pucEventBuffer );
	prvEventAddFieldU8( &xEvent, ucLength );
	prvEventAddFieldU8( &xEvent, ( uint8_t )xIrqLevel );
	prvEventAddFieldU8( &xEvent, ucCtxType );
	prvEventAddFieldPtr( &xEvent, uxCtxId );
	prvEventAddTimestamp( &xEvent, &ullEventTimestamp );
	xReturn = prvEventSend( &xEvent );
	if( xReturn == 0 )
	{
		ullLastEventTimestamp = ullEventTimestamp;
	}
	else
	{
		ulBufferOverflow++;
	}
}

/* Note: this API can only be called when IRQ is disabled */
static void prvRtosTracerStartEvents( void )
{
uint64_t ullEventTimestamp;
EventData_t xEvent;
int xReturn;

	xReturn = prvBufferOverflowEventSend();
	if( xReturn == 0 )
	{
		/* Generate SYSTEM_DESC event */
		uint8_t ucLength = 1 + 4 + 1; /* length + CPU Freq + CPU Arch */
		prvEventInit( &xEvent, prvEVT_ID_SYSTEM_DESC, pucEventBuffer );
		prvEventAddFieldU8( &xEvent, ucLength );
		prvEventAddFieldU32( &xEvent, ( uint32_t )configCPU_CLOCK_HZ );
		prvEventAddFieldU8( &xEvent, prvCPU_ARCH );
		prvEventAddTimestamp( &xEvent, &ullEventTimestamp );
		xReturn = prvEventSend( &xEvent );
		if( xReturn == 0 )
		{
			ullLastEventTimestamp = ullEventTimestamp;
		}
		else
		{
			ulBufferOverflow++;
		}
	}

	/* Generate IDLE_TASK_INFO event */
	prvIdleTaskInfoEvent();

	/* Generate TASK_INFO event */
	prvTaskInfoEvent();

	/* Generate OBJECT_NAME event */
	prvObjectNameEvent();

	/* Generate CURRENT_CONTEXT event */
	prvCurrentContextEvent();

	if( prvBufferOverflowEventSend() )
	{
		return;
	}

	/* Generate START_INFO_FINISHED event */
	prvEventInit( &xEvent, prvEVT_ID_TRACE_START_INFO_FINISHED, pucEventBuffer );
	prvEventAddTimestamp( &xEvent, &ullEventTimestamp );
	xReturn = prvEventSend( &xEvent );
	if( xReturn == 0 )
	{
		ullLastEventTimestamp = ullEventTimestamp;
	}
	else
	{
		ulBufferOverflow++;
	}
}

/* prvRtosTracerStart(): Start RTOS Tracer
Note: this API can only be called when IRQ is disabled */
static void prvRtosTracerStart( void )
{
EventData_t xEvent;

	/* Enable RTOS Tracer */
	xTraceEnable = pdTRUE;

	/* Generate TRACE_START event */
	prvEventInit( &xEvent, prvEVT_ID_TRACE_START, pucEventBuffer );
	/* TRACE_START event always has 0 timestamp delta */
	prvEventAddFieldU8( &xEvent, 0 );
	ullLastEventTimestamp = prvReadMtime();
	prvEventSend( &xEvent );

	/* Generate 'Start Info' events */
	prvRtosTracerStartEvents();
}

/* prvRtosTracerStop(): Stop RTOS Tracer.
Note: this API can only be called when IRQ is disabled */
static void prvRtosTracerStop( void )
{
EventData_t xEvent;
uint64_t ullEventTimestamp;
int xReturn;

	/* Disable RTOS Tracer */
	xTraceEnable = pdFALSE;

	/* Generate TRACE_STOP event */
	prvEventInit( &xEvent, prvEVT_ID_TRACE_STOP, pucEventBuffer );
	prvEventAddTimestamp( &xEvent, &ullEventTimestamp );
	xReturn = prvEventSend( &xEvent );
	if( xReturn == 0 )
	{
		ullLastEventTimestamp = ullEventTimestamp;
	}
	else
	{
		ulBufferOverflow++;
	}
}

static void prvRtosTracerHandleStartFlag( void )
{
uint8_t ucStartFlag;

	ucStartFlag = ucReadStartFlag();

	/* Handling start flag */
	if( ucStartFlag == prvFLAG_TRACE_START && xTraceEnable == pdFALSE )
	{
		prvRtosTracerStart();
	}
	else if( ucStartFlag == prvFLAG_TRACE_STOP && xTraceEnable == pdTRUE )
	{
		prvRtosTracerStop();
	}
}

static void prvRtosTracerHandleHostCmd( void )
{
	while( 1 )
	{
	BaseType_t xReturn;
	uint8_t ucHostCmd;

		xReturn = xReadCommandBuffer( &ucHostCmd, 1 );
		if( xReturn == pdFALSE )
		{
			/* Command buffer is empty */
			break;
		}

		/* Handling host commands */
		if( ucHostCmd == prvCMD_TRACE_START && xTraceEnable == pdFALSE )
		{
			prvRtosTracerStart();
		}
		else if( ucHostCmd == prvCMD_TRACE_STOP && xTraceEnable == pdTRUE )
		{
			prvRtosTracerStop();
		}
		else if( ucHostCmd == prvCMD_QUEUE_REGISTER )
		{
			xReturn = xReadCommandBuffer( ( uint8_t * )&xQueueRegistryCmdInfo, 3 );
			if( xReturn == pdFALSE )
			{
				/* Command buffer is empty */
				break;
			}
		}
	}
}

static void prvRtosTracerHandleHostControl( void )
{
static BaseType_t xFirstRecvFromHost = pdTRUE;
UBaseType_t uxSavedStatus;

	uxSavedStatus = prvMIE_SAVE();
	{
		if( xFirstRecvFromHost == pdTRUE )
		{
			prvRtosTracerHandleStartFlag();
			xFirstRecvFromHost = pdFALSE;
		}

		prvRtosTracerHandleHostCmd();
	}
	prvMIE_RESTORE( uxSavedStatus );
}

int xRtosTracerInit( uint64_t ullConfig )
{
	uint8_t ucCacheEnable = 0;
	if( ullConfig & tracerCFG_CACHE_ENABLE )
	{
		ucCacheEnable = 1;
	}

	return xInitTracerTCB( ucCacheEnable );
}

static void vRtosTracerEnterTrap( uint8_t ucType, uint16_t usTrapId )
{
UBaseType_t uxSavedStatus;

	prvRtosTracerHandleHostControl();

	uxSavedStatus = prvMIE_SAVE();
	{
		xIrqLevel++;

		xCurrentTrapId.ucType = ucType;
		xCurrentTrapId.usId = usTrapId;
	}
	prvMIE_RESTORE( uxSavedStatus );

	uxSavedStatus = prvMIE_SAVE();
	do {
		if( xTraceEnable )
		{
		EventData_t xEvent;
		uint64_t ullEventTimestamp;
		uint8_t ucEventId;
		int xReturn;

			if( prvBufferOverflowEventSend() )
			{
				break;
			}

			/* Event ID */
			if( xCurrentTrapId.ucType == prvCONTEXT_EXCEPTION )
			{
				ucEventId = prvEVT_ID_EXCEPTION_ENTER;
			}
			else if( xCurrentTrapId.ucType == prvCONTEXT_ISR )
			{
				ucEventId = prvEVT_ID_ISR_ENTER;
			}
			else if( xCurrentTrapId.ucType == prvCONTEXT_TICK_ISR )
			{
				ucEventId = prvEVT_ID_TICK_ISR_ENTER;
			}
			else
			{
				/* Error: no trap found */
				break;
			}

			prvEventInit( &xEvent, ucEventId, pucEventBuffer );
			prvEventAddFieldU16( &xEvent, xCurrentTrapId.usId );
			prvEventAddTimestamp( &xEvent, &ullEventTimestamp );
			xReturn = prvEventSend( &xEvent );
			if( xReturn == 0 )
			{
				ullLastEventTimestamp = ullEventTimestamp;
			}
			else
			{
				ulBufferOverflow++;
			}
		}
	} while( 0 );
	prvMIE_RESTORE( uxSavedStatus );
}

static void vRtosTracerExitTrap( void )
{
UBaseType_t uxSavedStatus;

	prvRtosTracerHandleHostControl();

	uxSavedStatus = prvMIE_SAVE();
	{
		xIrqLevel--;
	}
	prvMIE_RESTORE( uxSavedStatus );

	uxSavedStatus = prvMIE_SAVE();
	do {
		if( xTraceEnable )
		{
		EventData_t xEvent;
		uint64_t ullEventTimestamp;
		uint8_t ucEventId;
		int xReturn;

			if( prvBufferOverflowEventSend() )
			{
				break;
			}

			/* Event ID */
			if( xCurrentTrapId.ucType == prvCONTEXT_EXCEPTION )
			{
				ucEventId = prvEVT_ID_EXCEPTION_EXIT;
			}
			else if( xCurrentTrapId.ucType == prvCONTEXT_ISR )
			{
				ucEventId = prvEVT_ID_ISR_EXIT;
			}
			else if( xCurrentTrapId.ucType == prvCONTEXT_TICK_ISR )
			{
				ucEventId = prvEVT_ID_TICK_ISR_EXIT;
			}
			else
			{
				/* Error: no trap found */
				break;
			}

			prvEventInit( &xEvent, ucEventId, pucEventBuffer );
			prvEventAddFieldU16( &xEvent, xCurrentTrapId.usId );
			prvEventAddTimestamp( &xEvent, &ullEventTimestamp );
			xReturn = prvEventSend( &xEvent );
			if( xReturn == 0 )
			{
				ullLastEventTimestamp = ullEventTimestamp;
			}
			else
			{
				ulBufferOverflow++;
			}
		}
	} while( 0 );
	prvMIE_RESTORE( uxSavedStatus );
}

void vRtosTracerEnterISR( uint16_t usIrqId )
{
	vRtosTracerEnterTrap( prvCONTEXT_ISR, usIrqId );
}

void vRtosTracerExitISR( void )
{
	vRtosTracerExitTrap();
}

void vRtosTracerEnterTickISR( void )
{
	vRtosTracerEnterTrap( prvCONTEXT_TICK_ISR, 0 );
}

void vRtosTracerExitTickISR( void )
{
	vRtosTracerExitTrap();
}

void vRtosTracerTaskSwitchIn( void *pvTask )
{
UBaseType_t uxSavedStatus;

	prvRtosTracerHandleHostControl();

	uxSavedStatus = prvMIE_SAVE();
	{
		if( xTraceEnable )
		{
		EventData_t xEvent;
		uint64_t ullEventTimestamp;
		int xReturn;

			xReturn = prvBufferOverflowEventSend();

			if( xReturn == 0 )
			{
				prvEventInit( &xEvent, prvEVT_ID_TASK_SWITCH_IN, pucEventBuffer );
				prvEventAddFieldPtr( &xEvent, ( uintptr_t )pvTask );
				prvEventAddTimestamp( &xEvent, &ullEventTimestamp );
				xReturn = prvEventSend( &xEvent );
				if( xReturn == 0 )
				{
					ullLastEventTimestamp = ullEventTimestamp;
				}
				else
				{
					ulBufferOverflow++;
				}
			}
		}
	}
	prvMIE_RESTORE( uxSavedStatus );
}

void vRtosTracerIdleTaskCreate( void *pvNewTask )
{
UBaseType_t uxSavedStatus;

	prvRtosTracerHandleHostControl();

	/* Save idle task ID */
	uxSavedStatus = prvMIE_SAVE();
	{
		pvIdleTaskId = pvNewTask;
	}
	prvMIE_RESTORE( uxSavedStatus );

	uxSavedStatus = prvMIE_SAVE();
	{
		if( xTraceEnable )
		{
			prvIdleTaskInfoEvent();
		}
	}
	prvMIE_RESTORE( uxSavedStatus );
}

void vRtosTracerTaskCreate( void *pvNewTask, uint16_t usPriority, const char *pcTaskName )
{
UBaseType_t uxSavedStatus;

	prvRtosTracerHandleHostControl();

	/* Save task IDs and the max number is configTRACER_MAX_NUM_TASK_INFO */
	uxSavedStatus = prvMIE_SAVE();
	{
		if( ( ulNumOfTasks + 1 ) < configTRACER_MAX_NUM_TASK_INFO )
		{
			xTaskInfoEventPool[ ulNumOfTasks ].puxTaskId = pvNewTask;
			xTaskInfoEventPool[ ulNumOfTasks ].usPriority = usPriority;
			xTaskInfoEventPool[ ulNumOfTasks ].pucName = pcTaskName;
			ulNumOfTasks++;
		}
	}
	prvMIE_RESTORE( uxSavedStatus );

	uxSavedStatus = prvMIE_SAVE();
	{
		if( xTraceEnable )
		{
		EventData_t xEvent;
		uint64_t ullEventTimestamp;
		uint8_t ucLength;
		int xReturn;

			xReturn = prvBufferOverflowEventSend();

			if( xReturn == 0 )
			{
				/* Length + Task ID + Priority + Name  */
				ucLength = 1 + sizeof( uintptr_t ) + 2 + strlen( pcTaskName ) + 1;
				prvEventInit( &xEvent, prvEVT_ID_TASK_INFO, pucEventBuffer );
				prvEventAddFieldU8( &xEvent, ucLength );
				prvEventAddFieldPtr( &xEvent, ( uintptr_t ) pvNewTask );
				prvEventAddFieldU16( &xEvent, usPriority );
				prvEventAddFieldStr( &xEvent, pcTaskName, prvMAX_TASK_NAME_SIZE );
				prvEventAddTimestamp( &xEvent, &ullEventTimestamp );
				xReturn = prvEventSend( &xEvent );
				if( xReturn == 0 )
				{
					ullLastEventTimestamp = ullEventTimestamp;
				}
				else
				{
					ulBufferOverflow++;
				}
			}

			xReturn = prvBufferOverflowEventSend();

			if( xReturn == 0 )
			{
				prvEventInit( &xEvent, prvEVT_ID_TASK_CREATE, pucEventBuffer );
				prvEventAddFieldPtr( &xEvent, ( uintptr_t ) pvNewTask );
				prvEventAddTimestamp( &xEvent, &ullEventTimestamp );
				xReturn = prvEventSend( &xEvent );
				if( xReturn == 0 )
				{
					ullLastEventTimestamp = ullEventTimestamp;
				}
				else
				{
					ulBufferOverflow++;
				}
			}
		}
	}
	prvMIE_RESTORE( uxSavedStatus );
}

void vRtosTracerTaskDelete( void *pvTaskToDelete )
{
UBaseType_t uxSavedStatus;

	prvRtosTracerHandleHostControl();

	/* Delete task IDs and the max number is configTRACER_MAX_NUM_TASK_INFO */
	uxSavedStatus = prvMIE_SAVE();
	{
		for( UBaseType_t i = 0; i < configTRACER_MAX_NUM_TASK_INFO; i++ )
		{
			if( xTaskInfoEventPool[ i ].puxTaskId == pvTaskToDelete )
			{
				xTaskInfoEventPool[ i ] = xTaskInfoEventPool[ --ulNumOfTasks ];
				break;
			}
		}
	}
	prvMIE_RESTORE( uxSavedStatus );

	uxSavedStatus = prvMIE_SAVE();
	{
		if( xTraceEnable )
		{
		EventData_t xEvent;
		uint64_t ullEventTimestamp;
		int xReturn;

			xReturn = prvBufferOverflowEventSend();

			if( xReturn == 0 )
			{
				prvEventInit( &xEvent, prvEVT_ID_TASK_DELETE, pucEventBuffer );
				prvEventAddFieldPtr( &xEvent, ( uintptr_t ) pvTaskToDelete );
				prvEventAddTimestamp( &xEvent, &ullEventTimestamp );
				xReturn = prvEventSend( &xEvent );
				if( xReturn == 0 )
				{
					ullLastEventTimestamp = ullEventTimestamp;
				}
				else
				{
					ulBufferOverflow++;
				}
			}
		}
	}
	prvMIE_RESTORE( uxSavedStatus );
}

static void prvCreateEventQueueOp( uint8_t ucEventId, uint32_t ulQueueOp,
	void *pvQueue, uint8_t ucApiStatus, uint8_t ucQueueType,
	unsigned long xTicksToWait, uint8_t ucHigherPriorityTaskWoken )
{
EventData_t xEvent;
uint64_t ullEventTimestamp;
int xReturn;

	/* ID and Data field */
	if( ucQueueType == queueQUEUE_TYPE_BASE )
	{
		if( prvBufferOverflowEventSend() )
		{
			return;
		}

		prvEventInit( &xEvent, ucEventId, pucEventBuffer );
		prvEventAddFieldPtr( &xEvent, ( uintptr_t )pvQueue );
		prvEventAddFieldU8( &xEvent, ucApiStatus );

		if( ( ulQueueOp & prvOP_CTX_MASK ) == prvOP_FROM_TASK )
		{
			prvEventAddFieldULong( &xEvent, xTicksToWait );
		}
		if( ( ( ulQueueOp & prvOP_CTX_MASK ) == prvOP_FROM_ISR ) &&
			( ( ulQueueOp & prvOP_API_MASK ) != prvOP_QUEUE_PEEK ) )
		{
			prvEventAddFieldU8( &xEvent, ucHigherPriorityTaskWoken );
		}
	}
	else if( ( ucQueueType == queueQUEUE_TYPE_BINARY_SEMAPHORE ) ||
			 ( ucQueueType == queueQUEUE_TYPE_COUNTING_SEMAPHORE ) ||
			 ( ucQueueType == queueQUEUE_TYPE_MUTEX ) )
	{
		if( prvBufferOverflowEventSend() )
		{
			return;
		}

		prvEventInit( &xEvent, ucEventId, pucEventBuffer );
		prvEventAddFieldPtr( &xEvent, ( uintptr_t )pvQueue );
		prvEventAddFieldU8( &xEvent, ucApiStatus );
		prvEventAddFieldU8( &xEvent, ucQueueType );

		if( ( ( ulQueueOp & prvOP_CTX_MASK ) == prvOP_FROM_TASK ) &&
			( ( ulQueueOp & prvOP_API_MASK ) != prvOP_SEMAPHORE_GIVE ) )
		{
			prvEventAddFieldULong( &xEvent, xTicksToWait );
		}
		if( ( ulQueueOp & prvOP_CTX_MASK ) == prvOP_FROM_ISR )
		{
			prvEventAddFieldU8( &xEvent, ucHigherPriorityTaskWoken );
		}
	}
	else
	{
		/* Unsupported RTOS event */
		return;
	}

	/* Timestamp Delta */
	prvEventAddTimestamp( &xEvent, &ullEventTimestamp );

	xReturn = prvEventSend( &xEvent );
	if( xReturn == 0 )
	{
		ullLastEventTimestamp = ullEventTimestamp;
	}
	else
	{
		ulBufferOverflow++;
	}
}

void vRtosTracerQueueSend( void *pvQueue, uint8_t ucApiStatus,
	uint8_t ucQueueType, unsigned long xTicksToWait )
{
UBaseType_t uxSavedStatus;

	prvRtosTracerHandleHostControl();

	uxSavedStatus = prvMIE_SAVE();
	do {
		if( xTraceEnable )
		{
		uint8_t ucEventId;
		uint32_t ulOp;

			/* Event ID */
			if( ucQueueType == queueQUEUE_TYPE_BASE )
			{
				ucEventId = prvEVT_ID_QUEUE_SEND;
				ulOp = prvOP_QUEUE_SEND | prvOP_FROM_TASK;
			}
			else if( ( ucQueueType == queueQUEUE_TYPE_BINARY_SEMAPHORE ) ||
					 ( ucQueueType == queueQUEUE_TYPE_COUNTING_SEMAPHORE ) ||
					 ( ucQueueType == queueQUEUE_TYPE_MUTEX ) )
			{
				ucEventId = prvEVT_ID_SEMAPHORE_GIVE;
				ulOp = prvOP_SEMAPHORE_GIVE | prvOP_FROM_TASK;
			}
			else
			{
				/* Unsupported RTOS event */
				break;
			}

			prvCreateEventQueueOp( ucEventId, ulOp, pvQueue, ucApiStatus,
				ucQueueType, xTicksToWait, 0 );
		}
	} while( 0 );
	prvMIE_RESTORE( uxSavedStatus );
}

void vRtosTracerQueueRecv( void *pvQueue, uint8_t ucApiStatus,
	uint8_t ucQueueType, unsigned long xTicksToWait )
{
UBaseType_t uxSavedStatus;

	prvRtosTracerHandleHostControl();

	uxSavedStatus = prvMIE_SAVE();
	do {
		if( xTraceEnable )
		{
		uint8_t ucEventId;
		uint32_t ulOp;

			/* Event ID */
			if( ucQueueType == queueQUEUE_TYPE_BASE )
			{
				ucEventId = prvEVT_ID_QUEUE_RECV;
				ulOp = prvOP_QUEUE_RECV | prvOP_FROM_TASK;
			}
			else if( ( ucQueueType == queueQUEUE_TYPE_BINARY_SEMAPHORE ) ||
					 ( ucQueueType == queueQUEUE_TYPE_COUNTING_SEMAPHORE ) ||
					 ( ucQueueType == queueQUEUE_TYPE_MUTEX ) )
			{
				ucEventId = prvEVT_ID_SEMAPHORE_TAKE;
				ulOp = prvOP_SEMAPHORE_TAKE | prvOP_FROM_TASK;
			}
			else
			{
				/* Unsupported RTOS event */
				break;
			}

			prvCreateEventQueueOp( ucEventId, ulOp, pvQueue, ucApiStatus,
				ucQueueType, xTicksToWait, 0 );
		}
	} while( 0 );
	prvMIE_RESTORE( uxSavedStatus );
}

void vRtosTracerQueuePeek( void *pvQueue, uint8_t ucApiStatus,
	uint8_t ucQueueType, unsigned long xTicksToWait )
{
UBaseType_t uxSavedStatus;

	prvRtosTracerHandleHostControl();

	uxSavedStatus = prvMIE_SAVE();
	do {
		if( xTraceEnable )
		{
		uint8_t ucEventId;
		uint32_t ulOp;

			/* Event ID */
			if( ucQueueType == queueQUEUE_TYPE_BASE )
			{
				ucEventId = prvEVT_ID_QUEUE_PEEK;
				ulOp = prvOP_QUEUE_PEEK | prvOP_FROM_TASK;
			}
			else
			{
				/* Unsupported RTOS event */
				break;
			}

			prvCreateEventQueueOp( ucEventId, ulOp, pvQueue, ucApiStatus,
				ucQueueType, xTicksToWait, 0 );
		}
	} while( 0 );
	prvMIE_RESTORE( uxSavedStatus );
}

void vRtosTracerQueueSendFromISR( void *pvQueue, uint8_t ucApiStatus,
	uint8_t ucQueueType, uint8_t ucHigherPriorityTaskWoken )
{
UBaseType_t uxSavedStatus;

	prvRtosTracerHandleHostControl();

	uxSavedStatus = prvMIE_SAVE();
	do {
		if( xTraceEnable )
		{
		uint8_t ucEventId;
		uint32_t ulOp;

			/* Event ID */
			if( ucQueueType == queueQUEUE_TYPE_BASE )
			{
				ucEventId = prvEVT_ID_QUEUE_SEND_FROM_ISR;
				ulOp = prvOP_QUEUE_SEND | prvOP_FROM_ISR;
			}
			else if( ( ucQueueType == queueQUEUE_TYPE_BINARY_SEMAPHORE ) ||
					 ( ucQueueType == queueQUEUE_TYPE_COUNTING_SEMAPHORE ) ||
					 ( ucQueueType == queueQUEUE_TYPE_MUTEX ) )
			{
				ucEventId = prvEVT_ID_SEMAPHORE_GIVE_FROM_ISR;
				ulOp = prvOP_SEMAPHORE_GIVE | prvOP_FROM_ISR;
			}
			else
			{
				/* Unsupported RTOS event */
				break;
			}

			prvCreateEventQueueOp( ucEventId, ulOp, pvQueue, ucApiStatus,
				ucQueueType, 0, ucHigherPriorityTaskWoken );
		}
	} while( 0 );
	prvMIE_RESTORE( uxSavedStatus );
}

void vRtosTracerQueueRecvFromISR( void *pvQueue, uint8_t ucApiStatus,
	uint8_t ucQueueType, uint8_t ucHigherPriorityTaskWoken )
{
UBaseType_t uxSavedStatus;

	prvRtosTracerHandleHostControl();

	uxSavedStatus = prvMIE_SAVE();
	do {
		if( xTraceEnable )
		{
		uint8_t ucEventId;
		uint32_t ulOp;

			/* Event ID */
			if( ucQueueType == queueQUEUE_TYPE_BASE )
			{
				ucEventId = prvEVT_ID_QUEUE_RECV_FROM_ISR;
				ulOp = prvOP_QUEUE_RECV | prvOP_FROM_ISR;
			}
			else if( ( ucQueueType == queueQUEUE_TYPE_BINARY_SEMAPHORE ) ||
					 ( ucQueueType == queueQUEUE_TYPE_COUNTING_SEMAPHORE ) ||
					 ( ucQueueType == queueQUEUE_TYPE_MUTEX ) )
			{
				ucEventId = prvEVT_ID_SEMAPHORE_TAKE_FROM_ISR;
				ulOp = prvOP_SEMAPHORE_TAKE | prvOP_FROM_ISR;
			}
			else
			{
				/* Unsupported RTOS event */
				break;
			}

			prvCreateEventQueueOp( ucEventId, ulOp, pvQueue, ucApiStatus,
				ucQueueType, 0, ucHigherPriorityTaskWoken );
		}
	} while( 0 );
	prvMIE_RESTORE( uxSavedStatus );
}

void vRtosTracerQueuePeekFromISR( void *pvQueue, uint8_t ucApiStatus,
	uint8_t ucQueueType)
{
UBaseType_t uxSavedStatus;

	prvRtosTracerHandleHostControl();

	uxSavedStatus = prvMIE_SAVE();
	do {
		if( xTraceEnable )
		{
		uint8_t ucEventId;
		uint32_t ulOp;

			/* Event ID */
			if( ucQueueType == queueQUEUE_TYPE_BASE )
			{
				ucEventId = prvEVT_ID_QUEUE_PEEK_FROM_ISR;
				ulOp = prvOP_QUEUE_PEEK | prvOP_FROM_ISR;
			}
			else
			{
				/* Unsupported RTOS event */
				break;
			}

			prvCreateEventQueueOp( ucEventId, ulOp, pvQueue, ucApiStatus,
				ucQueueType, 0, 0);
		}
	} while( 0 );
	prvMIE_RESTORE( uxSavedStatus );
}

void vRtosTracerQueueRegistryAdd( void *pvQueue, const char *pcQueueName )
{
UBaseType_t uxSavedStatus;

	prvRtosTracerHandleHostControl();

	uxSavedStatus = prvMIE_SAVE();
	{
		if( xTraceEnable )
		{
		EventData_t xEvent;
		uint64_t ullEventTimestamp;
		int xReturn;

			xReturn = prvBufferOverflowEventSend();

			if( xReturn == 0 )
			{
				prvEventInit( &xEvent, prvEVT_ID_OBJECT_NAME, pucEventBuffer );
				prvEventAddFieldPtr( &xEvent, ( uintptr_t ) pvQueue );
				prvEventAddFieldStr( &xEvent, pcQueueName, prvMAX_OBJECT_NAME_SIZE );
				prvEventAddTimestamp( &xEvent, &ullEventTimestamp );
				xReturn = prvEventSend( &xEvent );
				if( xReturn == 0 )
				{
					ullLastEventTimestamp = ullEventTimestamp;
				}
				else
				{
					ulBufferOverflow++;
				}
			}
		}
	}
	prvMIE_RESTORE( uxSavedStatus );
}
#endif /* ( configUSE_ANDES_TRACER == 1 ) */
