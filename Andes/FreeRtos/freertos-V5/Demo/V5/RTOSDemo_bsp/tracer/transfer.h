#ifndef TRANSFER_H
#define TRANSFER_H

#if( configUSE_ANDES_TRACER == 1 )

/* Public API of transfer.c */
int xInitTracerTCB( uint8_t ucCacheEnable );
uint8_t ucReadStartFlag( void );
BaseType_t xWriteLogBuffer( uint8_t *pucData, UBaseType_t uxSize );
BaseType_t xReadCommandBuffer( uint8_t *pucData, UBaseType_t uxSize );
#endif /* ( configUSE_ANDES_TRACER == 1 ) */
#endif /* TRANSFER_H */
