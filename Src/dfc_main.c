#include "dcf_main.h"
#include "dcf77.h"
#include <stdio.h>

#include "GUI.h"

#include "gps.h"
#include "uart.h"
#include "tfa.h"

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

void hiMSTick(void) {
	pinReset(time_pin);
	startCountUS();
	//dcf77msTask();
	tfaTask();
	if (!phase) {
		gpsMsTask();
		dcf_usec = endCountUS();
	}	
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
	//uartTest();
	/*if (cnt > 2) {*/
		//gpsTask();
	/*	cnt = 0;
	} else cnt++;
		*/
	//gpsTask();
	
	sprintf(str, "Time: %02i:%02i:%02i          ", gpsTime_tm.tm_hour, gpsTime_tm.tm_min, gpsTime_tm.tm_sec);
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "Date: %02i.%s.%04i, %s           ", gpsTime_tm.tm_mday, tm2_months3[gpsTime_tm.tm_mon], gpsTime_tm.tm_year + 1900, tm2_weekdays3[gpsTime_tm.tm_wday]);
	GUI_DispStringAt(str, x, y);
	y += dy;
	
	sprintf(str, "t: %0.1f *C          ", tfaTemperature / 10.0);
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "H: %i%%          ", tfaHumidity);
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "Level: %i          ", tfaLevel);
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "Battery: %i          ", tfaBatteryGood);
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "Overal packages: %i          ", tfaNumPackages);
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "Good packages: %i          ", tfaGoodPackages);
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "Process: %i / 50 uSec         ", dcf_usec);
	GUI_DispStringAt(str, x, y);
	y += dy;
	

	/*GUI_DispStringAt("DCF77", x, y);
	y += dy;
	
	sprintf(str, "Time: %02i:%02i:%02i      ", dcf77timeOk_tm.tm_hour, dcf77timeOk_tm.tm_min, dcf77timeOk_tm.tm_sec);
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "Date: %02i.%s.%04i, %s       ", dcf77timeOk_tm.tm_mday, tm2_months3[dcf77timeOk_tm.tm_mon], dcf77timeOk_tm.tm_year + 1900, tm2_weekdays3[dcf77timeOk_tm.tm_wday]);
	GUI_DispStringAt(str, x, y);
	y += dy;

	struct tm * rec;
	rec = localtime2(&dcf77timeOkReceived);
	if (dcf77timeOkReceived) {
		sprintf(str, "Last received: %02i:%02i %02i     ", rec->tm_hour, rec->tm_min, rec->tm_mday);
	} else {
		sprintf(str, "Last received: no     ");
	}
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "Time received last day: %s       ", dcf77received?"yes":"no");
	GUI_DispStringAt(str, x, y);
	y += dy;
		
	sprintf(str, "Level: %i %%      ", dcf77levelPercent);
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "Sync: %i %%      ", dcf77syncPercent);
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "Min sync: %i %%      ", dcf77minSyncPercent);
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "Sync: %i ms      ", dcf77sync_ms);
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "59s: %i s     ", dcf77min59sync);
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "Start time: %i m %i s     ", dcf77startSec / 60, dcf77startSec % 60);
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "mSec per day: %i ms            ", dcf77msec_per_day);
	GUI_DispStringAt(str, x, y);
	y += dy;

	/*sprintf(str, "Last good night: %i      ", last_good_night);
	GUI_DispStringAt(str, x, y);
	y += dy;
	*//*
	
	sprintf(str, "All results: %i      ", dcf77testCnt);
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "Good parity: %i      ", dcf77testCnt1);
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "Good values: %i      ", dcf77testCnt2);
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "Good compared: %i      ", dcf77testCnt3);
	GUI_DispStringAt(str, x, y);
	y += dy;

	sprintf(str, "Good callback: %i      ", dcf77testCnt4);
	GUI_DispStringAt(str, x, y);
	y += dy;

	//sprintf(str, "Processed: %i us     ", dcf_usec);
	//GUI_DispStringAt(str, x, y);
	//y += dy;

	//sprintf(str, "Sec processed: %i us     ", dcf77secUs);
	//GUI_DispStringAt(str, x, y);
	//y += dy;

	sprintf(str, "Min processed: %i us     ", dcf77minUs);
	GUI_DispStringAt(str, x, y);
	y += dy;*/

}

void dcf77work(void) {
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
	//dcf77init();
	tfaInit();
	gpsInit();
	

	ms_msec = 0;
	msec = 0;
	
	lcdInit();
	
	msInit(20, 50); //hi - 20 kHz; lo - 50 ms

	//Test LCD
	//LCD_GLASS_DisplayString((uint8_t *)"Hello, world!");

	while(true) {
		delayMS(100);
	};
}
