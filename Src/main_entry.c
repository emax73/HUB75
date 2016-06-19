#include "main_entry.h"
#include <stdio.h>

#include "GUI.h"

#include "hub75.h"

const pin_t green_led = {
	GPIOG,
	GPIO_PIN_13
},
red_led = {
	GPIOG,
	GPIO_PIN_14
},
button_pin = {
	GPIOA,
	GPIO_PIN_0
},
time_pin = {
	GPIOB,
	GPIO_PIN_7
};

int msec = 0;
char button = 0, button0 = 0;
int mode = 0;
int dcf_usec;
static int phase = 0;
static int counter;

void hiMSTick(void) {
	pinReset(time_pin);
	startCountUS(&counter);
	//dcf77msTask();
	if (msec < 500) {
		pinSet(green_led);
	} else {
		pinReset(green_led);
	}
	button = pinRead(button_pin);
	if (button && !button0) {
		mode++;
		if (mode > 12) {
			mode = 0;
		}
	}
	button0 = button;
	if (!phase) {
		msec++;
		if (msec >=1000) msec = 0;
	}
	phase++;
	if (phase >= 20) phase = 0;
	pinSet(time_pin);
}

void lcdInit(void) {
	GUI_SetBkColor(GUI_BLUE);
	GUI_Clear();
	GUI_SetColor(GUI_WHITE);
  GUI_SetFont(&GUI_Font20_1);
}

void loMSTick(void) {
  int x = 20, y = 20;
	const int dy = 20;
	char str[255];
	/*sprintf(str, "Time: %02i:%02i:%02i          ", gpsTime_tm.tm_hour, gpsTime_tm.tm_min, gpsTime_tm.tm_sec);
	GUI_DispStringAt(str, x, y);
	y += dy;*/
}

void mainEntry(void) {
	//Green LED
	__GPIOG_CLK_ENABLE();
	GPIO_InitTypeDef gpioInit;
	gpioInit.Pin = green_led.pin;
	gpioInit.Mode = GPIO_MODE_OUTPUT_PP;
	gpioInit.Pull = GPIO_PULLUP;
	gpioInit.Speed = GPIO_SPEED_HIGH;
	HAL_GPIO_Init(green_led.port, &gpioInit);

	//Red LED
	__GPIOG_CLK_ENABLE();
	gpioInit.Pin = red_led.pin;
	gpioInit.Mode = GPIO_MODE_OUTPUT_PP;
	gpioInit.Pull = GPIO_PULLUP;
	gpioInit.Speed = GPIO_SPEED_HIGH;
	HAL_GPIO_Init(red_led.port, &gpioInit);

	//Time Pin
	__GPIOB_CLK_ENABLE();
	gpioInit.Pin = time_pin.pin;
	gpioInit.Mode = GPIO_MODE_OUTPUT_PP;
	gpioInit.Pull = GPIO_PULLUP;
	gpioInit.Speed = GPIO_SPEED_HIGH;
	HAL_GPIO_Init(time_pin.port, &gpioInit);

	delayInit();

	
	//LCD_GLASS_Configure_GPIO();
	//LCD_GLASS_Init();
	hubInit();

	ms_msec = 0;
	msec = 0;
	
	//lcdInit();
	
	msInit(20, 50); //hi - 20 kHz; lo - 50 ms

	//Test LCD
	//LCD_GLASS_DisplayString((uint8_t *)"Hello, world!");

	while(true) {
		delayMS(100);
	};
}
