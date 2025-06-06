/*
 * Copyright (c) 2012-2021 Andes Technology Corporation
 * All rights reserved.
 *
 */

#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "platform.h"

#define USE_CLIC

/*
 * Define 'NDS_CLIC_BASE' before include CLIC intrinsic header file to
 * active CLIC related intrinsic functions.
 */
#define NDS_CLIC_BASE        CLIC_BASE
#include "clic.h"

/*
 * CPU Machine timer control
 *
 * Machine timer interrupt (MTIP) is connect to CLIC and CLICPLMT scheme is used.
 */
#define HAL_MTIMER_INITIAL()                            { DEV_PLMT->MTIMESTOP = 0; __nds__clic_set_priority(IRQ_M_TIMER, 1); }
#define HAL_MTIME_ENABLE()                              __nds__clic_enable_interrupt(IRQ_M_TIMER)
#define HAL_MTIME_DISABLE()                             __nds__clic_disable_interrupt(IRQ_M_TIMER)
#define HAL_MTIME_CONFIGURE(level, shv)                 \
{                                                       \
        __nds__clic_set_level(IRQ_M_TIMER, level);      \
        __nds__clic_set_shv(IRQ_M_TIMER, shv);          \
}

/*
 * CPU Machine SWI control
 *
 * Machine SWI (MSIP) is connected to CLIC and CLICPLMT scheme is used.
 */
#define HAL_MSWI_INITIAL()                              __nds__clic_set_priority(IRQ_M_SOFT, 1)
#define HAL_MSWI_ENABLE()                               __nds__clic_enable_interrupt(IRQ_M_SOFT)
#define HAL_MSWI_DISABLE()                              __nds__clic_disable_interrupt(IRQ_M_SOFT)
#define HAL_MSWI_PENDING()                              { DEV_PLMT->MSIP = 1; }
#define HAL_MSWI_CLEAR()                                { DEV_PLMT->MSIP = 0; }
#define HAL_MSWI_CONFIGURE(level, shv)                  \
{                                                       \
        __nds__clic_set_level(IRQ_M_SOFT, level);       \
        __nds__clic_set_shv(IRQ_M_SOFT, shv);           \
}

/*
 * Platform defined interrupt controller access
 *
 * This uses the CLIC scheme.
 */
#define HAL_INTERRUPT_ENABLE(vector)                    __nds__clic_enable_interrupt(vector)
#define HAL_INTERRUPT_DISABLE(vector)                   __nds__clic_disable_interrupt(vector)
#define HAL_INTERRUPT_THRESHOLD(threshold)              __nds__clic_set_mmode_threshold(threshold)
#define HAL_INTERRUPT_SET_TRIGGER(vector, trigger)      __nds__clic_set_trig(vector, trigger)
#define HAL_INTERRUPT_SET_SHV(vector, shv)              __nds__clic_set_shv(vector, shv)
#define HAL_INTERRUPT_SET_LEVEL(vector, level)          \
{                                                       \
        __nds__clic_set_level(vector, level);           \
        __nds__clic_set_priority(vector, 1);            \
}

/*
 * Vectored based inline interrupt attach and detach control
 */
extern long __vectors[];
extern void default_irq_entry(void);

#define HAL_INLINE_INTERRUPT_ATTACH(vector, isr)        { __vectors[vector] = (long)isr; }
#define HAL_INLINE_INTERRUPT_DETACH(vector, isr)        { if ( __vectors[vector] == (long)isr ) __vectors[vector] = (long)default_irq_entry; }

/*
 * Inline nested interrupt entry/exit macros
 */
/* CSR Svae/Restore macro */
#define SAVE_CSR(r)                                     long __##r = read_csr(r);
#define RESTORE_CSR(r)                                  write_csr(r, __##r);

#if SUPPORT_PFT_ARCH
#define SAVE_MXSTATUS()                                 SAVE_CSR(NDS_MXSTATUS)
#define RESTORE_MXSTATUS()                              RESTORE_CSR(NDS_MXSTATUS)
#else
#define SAVE_MXSTATUS()
#define RESTORE_MXSTATUS()
#endif

/* Nested IRQ entry macro : Save CSRs and enable global interrupt. */
#define NESTED_IRQ_ENTRY()                              \
        SAVE_CSR(NDS_MEPC)                              \
        SAVE_CSR(NDS_MCAUSE)                            \
        SAVE_MXSTATUS()                                 \
        set_csr(NDS_MSTATUS, MSTATUS_MIE);

/* Nested IRQ exit macro : Restore CSRs */
#define NESTED_IRQ_EXIT()                               \
        clear_csr(NDS_MSTATUS, MSTATUS_MIE);            \
        RESTORE_CSR(NDS_MCAUSE)                         \
        RESTORE_CSR(NDS_MEPC)                           \
        RESTORE_MXSTATUS()

#ifdef __cplusplus
}
#endif

#endif	/* __INTERRUPT_H__ */
