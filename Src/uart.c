#include "uart.h"
#include "gps.h"
#include <string.h>

UART_HandleTypeDef huart5;
uart_line_t uart_line = { 0 };

void uartInit(void) { // initialization
	__UART5_CLK_ENABLE();
	__GPIOC_CLK_ENABLE();
  __GPIOD_CLK_ENABLE();

  huart5.Instance = UART5;
  huart5.Init.BaudRate = 115200;
  //huart5.Init.BaudRate = 9600;
  huart5.Init.WordLength = UART_WORDLENGTH_8B;
  huart5.Init.StopBits = UART_STOPBITS_1;
  huart5.Init.Parity = UART_PARITY_NONE;
  huart5.Init.Mode = UART_MODE_TX_RX;
  huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart5.Init.OverSampling = UART_OVERSAMPLING_16;
  HAL_UART_Init(&huart5);
	
	GPIO_InitTypeDef GPIO_InitStruct;

  /**UART5 GPIO Configuration    
  PC12     ------> UART5_TX
  PD2     ------> UART5_RX 
  */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF8_UART5;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF8_UART5;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
	
	uart_line.receiving = &uart_line.line1;
	uart_line.result = &uart_line.line2;
	uart_line.receiving1 = true;
	uart_line.ready = false;
	
  /* Peripheral interrupt init*/
  HAL_NVIC_SetPriority(UART5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(UART5_IRQn);
	
	__HAL_UART_ENABLE_IT(&huart5, UART_IT_RXNE);
}

static inline void uartToggleLine(void) {
	uart_buffer_t * buf = uart_line.receiving;
	buf->buffer[buf->len] = 0;
	if (uart_line.receiving1) {
		uart_line.receiving = &uart_line.line2;
		uart_line.result = &uart_line.line1;
		uart_line.receiving1 = false;
	} else {
		uart_line.receiving = &uart_line.line1;
		uart_line.result = &uart_line.line2;
		uart_line.receiving1 = true;
	}
	buf = uart_line.receiving;
	buf->len = 0;
	buf->buffer[buf->len] = 0;
	uart_line.ready = true;
}

static inline void uartPutBuf(char ch) {
	uart_buffer_t * buf = uart_line.receiving;
	if (buf->len < UART_BUFFER_LEN - 1) {
		buf->buffer[buf->len++] = ch;
	}
}

static inline void uartPutChar(char ch) {
	if (!ch || ch == 10) { // lf \n
		uartToggleLine();
	} else if (ch == 13) {	//cr \r
	} else {
		uartPutBuf(ch);
	}
}

void uartIRQHandler(UART_HandleTypeDef *huart) {
	char it_rx, have_rx, ch;
	it_rx = __HAL_UART_GET_IT_SOURCE(huart, UART_IT_RXNE);
	have_rx = __HAL_UART_GET_FLAG(huart, UART_FLAG_RXNE);
	ch = __HAL_UART_FLUSH_DRREGISTER(huart) & 0xff;
	if (it_rx && have_rx) uartPutChar(ch);
}

