#ifndef __MS_TIMER_H
#define __MS_TIMER_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

#define MS_TIMER_MHZ 84
#define MS_TIMER_PRESCALER 2

extern int ms_msec, ms_phase;

extern int ms_msecPerDay; // msec +- correction per daynight 
#define MS_PER_DAY 86400000 // msec per daynight

//#define MS_PER_DAY_CORRECTION -6081 // ms per daynight correction
//#define MS_PER_DAY_CORRECTION -5760 // +520 ms per daynight correction
//#define MS_PER_DAY_CORRECTION -6280 // -190 ms per daynight correction
//#define MS_PER_DAY_CORRECTION -6090 // -634 ms per daynight correction
//#define MS_PER_DAY_CORRECTION -6000 // +900 ms per daynight correction
//#define MS_PER_DAY_CORRECTION -6045 // -185 ms per daynight correction
//#define MS_PER_DAY_CORRECTION -6022 // -93 ms per daynight correction
//#define MS_PER_DAY_CORRECTION -6010 // +201 ms per daynight correction
//#define MS_PER_DAY_CORRECTION -6016 // -900 ms per daynight correction
//#define MS_PER_DAY_CORRECTION -6013 // -900 ms per daynight correction
//#define MS_PER_DAY_CORRECTION -6010 // +277 ms per daynight correction
#define MS_PER_DAY_CORRECTION -6011 // -300 ms per daynight correction

extern void msInit(int hiKHz, int loMS); // msInit hiKHz - ms/phases timer, loMS - lo N ms timer;
extern void hiMSTick(void);
extern void loMSTick(void);
extern void msSetMSecPerDay(int msec);

#endif
