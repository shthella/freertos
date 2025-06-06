/*
 * Copyright (c) 2012-2021 Andes Technology Corporation
 * All rights reserved.
 *
 */

#include "uart.h"

#define DEFAULT_BAUDRATE	38400

void uart_set_baudrate(UART_RegDef *UARTx, unsigned int baudrate)
{
	unsigned int div;

	/*
	 * Oversampling is fixed to 16, and round off the divider value:
	 *   divider = (UCLKFREQ / (16.0 * baudrate)) + 0.5;
	 *
	 * And below is a variant which doesn't use FP operation.
	 */
	div = ((UCLKFREQ + 8 * baudrate) / (16 * baudrate));

	UARTx->LCR |= UARTC_LCR_DLAB;
	UARTx->DLL  = (div >> 0) & 0xff;
	UARTx->DLM  = (div >> 8) & 0xff;
	UARTx->LCR &= (~UARTC_LCR_DLAB);	/* Reset DLAB bit */
}

int uart_receive_byte(UART_RegDef *UARTx)
{
	while(!(UARTx->LSR & UARTC_LSR_RDR));
	return UARTx->RBR;
}

void uart_send_byte(UART_RegDef *UARTx, int ch)
{

	while(!(UARTx->LSR & UARTC_LSR_THRE));
	UARTx->THR = ch;
}

void uart_init(UART_RegDef *UARTx)
{
	/* Clear everything */
	UARTx->IER = 0UL;
	UARTx->LCR = 0UL;

	/* Setup baud rate */
	uart_set_baudrate(UARTx, DEFAULT_BAUDRATE);

	/* Setup parity, data bits, and stop bits */
	UARTx->LCR = UARTC_LCR_PARITY_NONE | UARTC_LCR_BITS8 | UARTC_LCR_STOP1;
}

