/*
 * Copyright (c) 2012-2021 Andes Technology Corporation
 * All rights reserved.
 *
 */

#include <stdio.h>
#include "platform.h"
#include "nds_intrinsic.h"
#include "ae350.h"

#include "FreeRTOS.h"

#define PLIC_BASE_ADDRESS               0xE4000000

typedef void (*isr_func)(void);

void default_irq_handler(void)
{
	printf("Default interrupt handler\n");
}


void wdt_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void rtc_period_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void rtc_alarm_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void pit_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void spi1_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void spi2_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void i2c_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void gpio_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void uart1_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void uart2_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void dma_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void swint_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void ac97_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void sdc_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void mac_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void lcd_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void touch_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void standby_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void wakeup_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));

const isr_func irq_handler[] = {
	wdt_irq_handler,
	rtc_period_irq_handler,
	rtc_alarm_irq_handler,
	pit_irq_handler,
	spi1_irq_handler,
	spi2_irq_handler,
	i2c_irq_handler,
	gpio_irq_handler,
	uart1_irq_handler,
	uart2_irq_handler,
	dma_irq_handler,
	default_irq_handler,
	swint_irq_handler,
	default_irq_handler,
	default_irq_handler,
	default_irq_handler,
	default_irq_handler,
	ac97_irq_handler,
	sdc_irq_handler,
	mac_irq_handler,
	lcd_irq_handler,
	default_irq_handler,
	default_irq_handler,
	default_irq_handler,
	default_irq_handler,
	touch_irq_handler,
	standby_irq_handler,
	wakeup_irq_handler,
	default_irq_handler,
	default_irq_handler,
	default_irq_handler,
	default_irq_handler
};

/* At the time of writing, interrupt nesting is not supported, so do not use
the default mext_interrupt() implementation as that enables interrupts.  A
version that does not enable interrupts is provided below.  THIS INTERRUPT
HANDLER IS SPECIFIC TO FREERTOS WHICH USES PLIC! */
void mext_interrupt(void)
{

	printf("[Debug] Entering mext_interrupt\n");
	printf("[Debug] Pending Status: 0x%x\n", *(volatile unsigned int *)(PLIC_BASE_ADDRESS + PLIC_PENDING_OFFSET));

	unsigned int irq_source = __nds__plic_claim_interrupt();
	printf("plic_claim_interrupt %d\n",irq_source);
	#if( configUSE_ANDES_TRACER == 1 )
		traceISR_ENTER(irq_source);
	#endif

	/* Do interrupt handler */

	irq_handler[irq_source]();

	#if( configUSE_ANDES_TRACER == 1 )
		traceISR_EXIT();
	#endif
	__nds__plic_complete_interrupt(irq_source);
}
