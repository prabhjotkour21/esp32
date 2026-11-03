//===============[Include Headers]==============
#ifndef MAIN_UART_H
#define MAIN_UART_H


#include <stdio.h>
#include "driver/uart.h"
#include "driver/uhci.h"
#include "driver/gpio.h"

//==============[UART GDMA PINS]==============
#define UART_PORT        UART_NUM_1
#define RX_PIN           5
#define TX_PIN           4
#define BUF_SIZE         (128)


//==============[UART GDMA Variables]==============
extern uhci_controller_handle_t uhci_ctrl;
extern SemaphoreHandle_t xBufferSem;
extern uint8_t rx_buffer_a[BUF_SIZE];
extern uint8_t rx_buffer_b[BUF_SIZE];
extern volatile int active_buffer_idx;
extern uint32_t last_received_size;

void init_uart_gdma(void);

#endif // MAIN_UART_H