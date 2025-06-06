/*
 * Copyright (c) 2012-2021 Andes Technology Corporation
 * All rights reserved.
 *
 */

#include "platform.h"
#include "uart.h"

int outbyte(int c)
{
	uart_send_byte(AE350_UART1, c);
	if (c =='\n')
		uart_send_byte(AE350_UART1, '\r');
	return c;
}

