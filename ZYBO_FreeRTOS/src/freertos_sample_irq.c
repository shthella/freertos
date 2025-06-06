#include "include/FreeRTOS.h"
#include "include/task.h"
#include "include/semphr.h"

#include "include/xparameters.h"
#include "include/xil_exception.h"
#include "include/xil_printf.h"
#include "include/xuartps.h"

// Definitions
#define UART_DEVICE_ID    XPAR_XUARTPS_0_DEVICE_ID
#define UART_INT_IRQ_ID   82

// Handles
static XUartPs Uart_PS;

// UART interrupt handler
void Uart_ISR(void *CallbackRef) {
    xil_printf("uart custom isr handler is called \r\n");
    XUartPs *Uart = (XUartPs *)CallbackRef;

    u32 isrStatus = XUartPs_ReadReg(Uart->Config.BaseAddress, XUARTPS_ISR_OFFSET);

    if (isrStatus & (XUARTPS_IXR_RXFULL | XUARTPS_IXR_RXOVR | XUARTPS_IXR_TOUT)) {
        if (XUartPs_IsReceiveData(Uart->Config.BaseAddress)) {
            u8 recvChar = XUartPs_RecvByte(Uart->Config.BaseAddress);
            xil_printf("ISR Received: %c\r\n", recvChar);
        }
    }

    XUartPs_WriteReg(Uart->Config.BaseAddress, XUARTPS_ISR_OFFSET, isrStatus);
    xil_printf("interrupt is cleared\r\n");
}

// Initialize UART and RX interrupt
void SetupUART() {
    XUartPs_Config *Config = XUartPs_LookupConfig(UART_DEVICE_ID);
    XUartPs_CfgInitialize(&Uart_PS, Config, Config->BaseAddress);
    XUartPs_SetBaudRate(&Uart_PS, 115200);

    // Register default driver callback (not mandatory, since we're using custom ISR)
    //XUartPs_SetHandler(&Uart_PS, (void *)Uart_ISR, &Uart_PS);

    // Enable RX interrupts
    XUartPs_SetInterruptMask(&Uart_PS,
        XUARTPS_IXR_RXOVR | XUARTPS_IXR_RXFULL | XUARTPS_IXR_TOUT);
    XUartPs_SetOperMode(&Uart_PS, XUARTPS_OPER_MODE_NORMAL);
    XUartPs_SetRecvTimeout(&Uart_PS, 1);

    // Register and enable interrupt using FreeRTOS wrappers
    xPortInstallInterruptHandler(UART_INT_IRQ_ID, (Xil_ExceptionHandler)Uart_ISR, &Uart_PS);
    vPortEnableInterrupt(UART_INT_IRQ_ID);
}

// Task waiting for UART interrupt
void vTaskUart(void *pvParameters) {
    for (;;) {
        xil_printf("Task running.and changing     ..\r\n");
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// Main function
int main(void) {
    xil_printf("FreeRTOS UART RX Interrupt + FreeRTOS API Demo Start\r\n");

    // Install FreeRTOS vector table
    vPortInstallFreeRTOSVectorTable();

    // Setup UART and interrupts
    SetupUART();

    // Start task
    xTaskCreate(vTaskUart, "UartTask", configMINIMAL_STACK_SIZE + 100, NULL, tskIDLE_PRIORITY + 2, NULL);
    vTaskStartScheduler();

    for (;;);
}
