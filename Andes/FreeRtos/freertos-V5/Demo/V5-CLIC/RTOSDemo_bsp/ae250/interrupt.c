/*
 * Copyright (c) 2012-2021 Andes Technology Corporation
 * All rights reserved.
 *
 */

#include <stdio.h>
#include "platform.h"

void default_irq_handler(void)
{
	printf("Default interrupt handler\n");
}

void mswi_handler(void) __attribute__((weak, alias("default_irq_handler")));
void mtime_handler(void) __attribute__((weak, alias("default_irq_handler")));
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
void bmc_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void swint_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void sdc_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void mac_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void standby_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));
void wakeup_irq_handler(void) __attribute__((weak, alias("default_irq_handler")));


/*
 * To suppose CLIC would use non-SHV interrupt as default and
 * calling C-ABI functions as Interrupt Handlers.
 */

void entry_irq3(void)
{
	mswi_handler();                         // Call ISR
}

void entry_irq7(void)
{
	mtime_handler();                        // Call ISR
}

void entry_irq19(void)
{
	rtc_period_irq_handler();               // Call ISR
}

void entry_irq20(void)
{
	rtc_alarm_irq_handler();                // Call ISR
}

void entry_irq21(void)
{
	pit_irq_handler();                      // Call ISR
}

void entry_irq22(void)
{
	spi1_irq_handler();                     // Call ISR
}

void entry_irq23(void)
{
	spi2_irq_handler();                     // Call ISR
}

void entry_irq24(void)
{
	i2c_irq_handler();                      // Call ISR
}

void entry_irq25(void)
{
	gpio_irq_handler();                     // Call ISR
}

void entry_irq26(void)
{
	uart1_irq_handler();                    // Call ISR
}

void entry_irq27(void)
{
	uart2_irq_handler();                    // Call ISR
}

void entry_irq28(void)
{
	dma_irq_handler();                      // Call ISR
}

void entry_irq29(void)
{
	bmc_irq_handler();                      // Call ISR
}

void entry_irq30(void)
{
	swint_irq_handler();                    // Call ISR
}

void entry_irq36(void)
{
	sdc_irq_handler();                      // Call ISR
}

void entry_irq37(void)
{
	mac_irq_handler();                      // Call ISR
}

void entry_irq44(void)
{
	standby_irq_handler();                  // Call ISR
}

void entry_irq45(void)
{
	wakeup_irq_handler();                   // Call ISR
}

/* At the time of writing, interrupt nesting is not supported, so do not use
the default CLIC "irq_entry" implementation as that enables interrupts.  A
version that does not enable interrupts is provided below.  THIS INTERRUPT
HANDLER IS SPECIFIC TO FREERTOS WHICH USES CLIC! */
void clic_interrupt( unsigned long mcause )
{
	int irq_source = mcause & 0xFFFUL;
	typedef void (*isr_func)(void);
//	extern isr_func __vectors[];

	/* Do interrupt handler */
	(*(isr_func)__vectors[irq_source])();
}
