#ifndef __DELAY_H
#define __DELAY_H
#include "stm32f4xx_hal.h"

#define DL_CLOCK_MHZ 84

extern TIM_HandleTypeDef htim2;

extern unsigned int delayTicks; 

void delayInit(void);
void delayNS(int ns);
void delayUS(int us);
void delayMS(int ms);

void startCountUS(int * counter);
int endCountUS(int * counter);

// __nop() = 6 ns @ 168 MHz
#define delay30ns() __NOP(); __NOP(); __NOP(); __NOP(); __NOP() 
#define delay50ns() __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP()
#define delay100ns() delay50ns(); delay50ns() 

#define delay500ns() delay100ns(); delay100ns(); delay100ns(); delay100ns(); delay100ns()
#define delay1us() delay500ns(); delay500ns()

#endif
