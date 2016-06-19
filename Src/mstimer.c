#include "mstimer.h"
#include "pins.h"
#include "delay.h"

TIM_HandleTypeDef htim2, htim4;

int hiTicksKHz;
int loTicksMS;
int ms_phase = 0, ms_msec = 0;

int ms_msecPerDay; // msec +- correction per daynight 

pin_t hi_timer_pin = {
	GPIOA,
	GPIO_PIN_9
}, 
lo_timer_pin = {
	GPIOA,
	GPIO_PIN_10
};

static int period; // MS Timer period
#define MS_FIXED_POINT 24

#define MS_MAX_MSEC 10000
static int deltaT, msecSign, deltaTI, deltaTmod;

void msSetMSecPerDay(int msec) {
	int sign = 1;
	int delta;
	if (msec > MS_MAX_MSEC) msec = MS_MAX_MSEC;
	else if (msec < -MS_MAX_MSEC) msec = - MS_MAX_MSEC;
	ms_msecPerDay = msec;
	if (msec == 0) deltaT = 0;
	else if (msec < 0) {
		msec = -msec;
		sign = -1;
	}
	delta = (((uint64_t)(msec * period)) << MS_FIXED_POINT) / MS_PER_DAY;
	deltaT = delta;
	msecSign = sign;
	deltaTmod = 0;
}

void msInit(int hiKHz, int loMS) {   // msInit hiKHz - ms/phases timer, loMS - lo N ms timer 
	
	hiTicksKHz = hiKHz;
	loTicksMS = loMS;
	
  __TIM2_CLK_ENABLE();
	
  TIM_ClockConfigTypeDef sClockSourceConfig;

  //hi - mstimer
	htim2.Instance = TIM2;
  htim2.Init.Prescaler = MS_TIMER_PRESCALER - 1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  period = MS_TIMER_MHZ * 1000 / (hiTicksKHz * MS_TIMER_PRESCALER);
	htim2.Init.Period = period - 1;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	HAL_TIM_Base_Init(&htim2);
	
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig);

  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
  HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);

  //lo - 100ms timer
  __TIM4_CLK_ENABLE();

  htim4.Instance = TIM4;
  htim4.Init.Prescaler = MS_TIMER_MHZ * 1000 - 1; // 1 KHz
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = loTicksMS - 1;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  HAL_TIM_Base_Init(&htim4);

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig);

  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
  HAL_NVIC_SetPriority(TIM4_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(TIM4_IRQn);
	
	ms_msec = 0;
	ms_phase = 0;
	
	msSetMSecPerDay(MS_PER_DAY_CORRECTION);

	HAL_TIM_Base_Start_IT(&htim2);
	HAL_TIM_Base_Start_IT(&htim4);
}

__weak void hiMSTick(void) {
}
__weak void loMSTick(void) {
}

void TIM2_IRQHandler(void)
{
	int p;
	HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
	if (__HAL_TIM_GET_ITSTATUS(&htim2, TIM_IT_UPDATE) != RESET) {
		__HAL_TIM_CLEAR_IT(&htim2, TIM_IT_UPDATE);
		//pinReset(hi_timer_pin);
		p = period - 1;
		//Correction
		if (deltaT != 0) {
			deltaTI = (deltaT + deltaTmod + (1 << (MS_FIXED_POINT - 1))) >> MS_FIXED_POINT;
			deltaTmod += (deltaT - (deltaTI << MS_FIXED_POINT));
			if (msecSign > 0) {
				p -= deltaTI;
			} else {
				p += deltaTI;
			}
		}
		//-Correction
		__HAL_TIM_SetAutoreload(&htim2, p);
		
		hiMSTick();
		ms_phase++;
		if (ms_phase >= hiTicksKHz) {
			ms_msec++;
			ms_phase=0;
		}
	}
}

void TIM4_IRQHandler(void)
{
	HAL_NVIC_ClearPendingIRQ(TIM4_IRQn);
	if (__HAL_TIM_GET_ITSTATUS(&htim4, TIM_IT_UPDATE) != RESET) {
		__HAL_TIM_CLEAR_IT(&htim4, TIM_IT_UPDATE);
		//100 ms int
		//pinReset(lo_timer_pin);
		loMSTick();
		//pinSet(lo_timer_pin);
	}
}
