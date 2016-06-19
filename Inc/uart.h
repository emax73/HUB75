#ifndef __UART_H
#define __UART_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>

void uartIRQHandler(UART_HandleTypeDef *huart);

extern UART_HandleTypeDef huart5;

extern bool uartTransferComplete;
extern bool uartReceiveComplete;

#define UART_BUFFER_LEN 1024
typedef struct {
	int len;
	char buffer[UART_BUFFER_LEN];
} uart_buffer_t;

typedef struct {
	uart_buffer_t line1;
	uart_buffer_t line2;
	uart_buffer_t * result, * receiving;
	char ready;
	char receiving1;
} uart_line_t;

extern uart_line_t uart_line;

void uartInit(void); // initialization

#endif

