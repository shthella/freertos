#ifndef TRACE_H
#define TRACE_H

#define tracerTRANSMODE_NO_BLOCK_SKIP		0
#define tracerTRANSMODE_BLOCK_WHEN_FULL		1

/* Note: Command buffer size MUST not larger than cache line size */
#define tracerCMD_BUFFER_SIZE				32

#if( configUSE_ANDES_TRACER == 1 )

/* Define the flag of ullConfig arg in vRtosTracerInit() */
#define tracerCFG_CACHE_ENABLE		( 0x01 )

/* Define the cache line size to do alignment or padding.
Note: The size MUST be cache line size or multiple of it. */
#define tracerCACHE_LINE_SIZE		( 64 )

#if( tracerCACHE_LINE_SIZE < 32 )
	#error "Couldn't define cache line size smaller than 32 byte."
#endif

/* API status macro for trace hook */
#define tracerAPI_STAT_SUCCESS		( 0u )
#define tracerAPI_STAT_FAIL			( 1u )
#define tracerAPI_STAT_BLOCKING		( 2u )

/* Default Idle task name. Copied from tasks.c. */
#ifndef configIDLE_TASK_NAME
	#define configIDLE_TASK_NAME "IDLE"
#endif

/* Public API of tracer.c */
#ifndef __ASSEMBLER__
	int xRtosTracerInit( uint64_t ullConfig );
	void vRtosTracerEnterISR( uint16_t usIrqId );
	void vRtosTracerExitISR( void );
	void vRtosTracerEnterTickISR( void );
	void vRtosTracerExitTickISR( void );

	void vRtosTracerTaskSwitchIn( void *pvTask );
	void vRtosTracerIdleTaskCreate( void *pvNewTask );
	void vRtosTracerTaskCreate( void *pvNewTask, uint16_t usPriority, const char *pcTaskName );
	void vRtosTracerTaskDelete( void *pvTaskToDelete );
	void vRtosTracerQueueSend( void *pvQueue, uint8_t ucApiStatus, uint8_t ucQueueType, unsigned long xTicksToWait );
	void vRtosTracerQueueRecv( void *pvQueue, uint8_t ucApiStatus, uint8_t ucQueueType, unsigned long xTicksToWait );
	void vRtosTracerQueuePeek( void *pvQueue, uint8_t ucApiStatus, uint8_t ucQueueType, unsigned long xTicksToWait );
	void vRtosTracerQueueSendFromISR( void *pvQueue, uint8_t ucApiStatus, uint8_t ucQueueType, uint8_t ucHigherPriorityTaskWoken );
	void vRtosTracerQueueRecvFromISR( void *pvQueue, uint8_t ucApiStatus, uint8_t ucQueueType, uint8_t ucHigherPriorityTaskWoken );
	void vRtosTracerQueuePeekFromISR( void *pvQueue, uint8_t ucApiStatus, uint8_t ucQueueType );
	void vRtosTracerQueueRegistryAdd( void *pvQueue, const char *pcQueueName );
#endif /* __ASSEMBLER__ */

/* Definition of FreeRTOS trace hook */
#ifdef __ASSEMBLER__
	/* Note: caller should set arguments of vRtosTracerEnterISR() before calling it */
	#define traceISR_ENTER()							jal vRtosTracerEnterISR
	#define traceISR_EXIT()								jal vRtosTracerExitISR
	#define traceTICK_ISR_ENTER()						jal vRtosTracerEnterTickISR
	#define traceTICK_ISR_EXIT()						jal vRtosTracerExitTickISR
#else
	#define traceISR_ENTER( usIrqId )					vRtosTracerEnterISR( usIrqId )
	#define traceISR_EXIT()								vRtosTracerExitISR()
	#define traceTICK_ISR_ENTER()						vRtosTracerEnterTickISR()
	#define traceTICK_ISR_EXIT()						vRtosTracerExitTickISR()
#endif /* __ASSEMBLER__ */

#define traceTASK_SWITCHED_IN()							vRtosTracerTaskSwitchIn( pxCurrentTCB )
#define traceTASK_CREATE( pxNewTCB ) \
{ \
	if( strncmp( pxNewTCB->pcTaskName, configIDLE_TASK_NAME, configMAX_TASK_NAME_LEN ) == 0 ) { \
		vRtosTracerIdleTaskCreate( pxNewTCB ); \
	} else { \
		vRtosTracerTaskCreate( pxNewTCB, pxNewTCB->uxPriority, pxNewTCB->pcTaskName ); \
	} \
} \

#define traceTASK_DELETE( pxTaskToDelete )				vRtosTracerTaskDelete( pxTaskToDelete )

/* To avoid compile error in ( configUSE_QUEUE_SETS == 1 ),
not using xTicksToWait in traceQUEUE_SEND() */
#define traceQUEUE_SEND( pxQueue ) \
	vRtosTracerQueueSend( pxQueue, tracerAPI_STAT_SUCCESS, pxQueue->ucQueueType, 0 )

#define traceQUEUE_SEND_FAILED( pxQueue ) \
	vRtosTracerQueueSend( pxQueue, tracerAPI_STAT_FAIL, pxQueue->ucQueueType, xTicksToWait )

#define traceBLOCKING_ON_QUEUE_SEND( pxQueue ) \
	vRtosTracerQueueSend( pxQueue, tracerAPI_STAT_BLOCKING, pxQueue->ucQueueType, xTicksToWait )

#define traceQUEUE_RECEIVE( pxQueue ) \
	vRtosTracerQueueRecv( pxQueue, tracerAPI_STAT_SUCCESS, pxQueue->ucQueueType, xTicksToWait )

#define traceQUEUE_RECEIVE_FAILED( pxQueue ) \
	vRtosTracerQueueRecv( pxQueue, tracerAPI_STAT_FAIL, pxQueue->ucQueueType, xTicksToWait )

#define traceBLOCKING_ON_QUEUE_RECEIVE( pxQueue ) \
	vRtosTracerQueueRecv( pxQueue, tracerAPI_STAT_BLOCKING, pxQueue->ucQueueType, xTicksToWait )

#define traceQUEUE_PEEK( pxQueue ) \
	vRtosTracerQueuePeek( pxQueue, tracerAPI_STAT_SUCCESS, pxQueue->ucQueueType, xTicksToWait )

#define traceQUEUE_PEEK_FAILED( pxQueue ) \
	vRtosTracerQueuePeek( pxQueue, tracerAPI_STAT_FAIL, pxQueue->ucQueueType, xTicksToWait )

#define traceBLOCKING_ON_QUEUE_PEEK( pxQueue ) \
	vRtosTracerQueuePeek( pxQueue, tracerAPI_STAT_BLOCKING, pxQueue->ucQueueType, xTicksToWait )

#define traceQUEUE_SEND_FROM_ISR( pxQueue ) \
	vRtosTracerQueueSendFromISR( pxQueue, tracerAPI_STAT_SUCCESS, pxQueue->ucQueueType, ( *pxHigherPriorityTaskWoken == pdTRUE ) )

#define traceQUEUE_SEND_FROM_ISR_FAILED( pxQueue ) \
	vRtosTracerQueueSendFromISR( pxQueue, tracerAPI_STAT_FAIL, pxQueue->ucQueueType, ( *pxHigherPriorityTaskWoken == pdTRUE ) )

#define traceQUEUE_RECEIVE_FROM_ISR( pxQueue ) \
	vRtosTracerQueueRecvFromISR( pxQueue, tracerAPI_STAT_SUCCESS, pxQueue->ucQueueType, ( *pxHigherPriorityTaskWoken == pdTRUE ) )

#define traceQUEUE_RECEIVE_FROM_ISR_FAILED( pxQueue ) \
	vRtosTracerQueueRecvFromISR( pxQueue, tracerAPI_STAT_FAIL, pxQueue->ucQueueType, ( *pxHigherPriorityTaskWoken == pdTRUE ) )

#define traceQUEUE_PEEK_FROM_ISR( pxQueue ) \
	vRtosTracerQueuePeekFromISR( pxQueue, tracerAPI_STAT_SUCCESS, pxQueue->ucQueueType )

#define traceQUEUE_PEEK_FROM_ISR_FAILED( pxQueue ) \
	vRtosTracerQueuePeekFromISR( pxQueue, tracerAPI_STAT_FAIL, pxQueue->ucQueueType )

#define traceQUEUE_REGISTRY_ADD( xQueue, pcQueueName ) \
	vRtosTracerQueueRegistryAdd( xQueue, pcQueueName )

#endif /* ( configUSE_ANDES_TRACER == 1 ) */
#endif /* TRACE_H */
