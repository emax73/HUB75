#ifndef __HUB75_H
#define __HUB75_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

#define HUB_TIMER TIM9
#define HUB_TIMER_CLK_ENABLE() __TIM9_CLK_ENABLE()
#define HUB_TIMER2 TIM10
#define HUB_TIMER2_CLK_ENABLE() __TIM10_CLK_ENABLE() 

#define HUB_TIMER_MHZ 168
#define HUB_UPDATE_HZ 100
#define HUB_PERIOD0_US 35 // time in us for prepare row data
#define HUB_MIN_PERIOD 3 // Min timer tacts 
#define HUB_COLOR_DEPTH 9
#define HUB_COLOR_NUM (1 << HUB_COLOR_DEPTH)
#define HUB_COLOR_BIT0 (1 << (HUB_COLOR_DEPTH - 1))
#define HUB_COLOR_I0 (HUB_COLOR_DEPTH - 1)

#define HUB_TIMER2_MHZ 168
#define HUB_TIMER2_PRESCALER 1
#define HUB_TIMER2_NS ((1000 + (HUB_TIMER2_MHZ >> 1)) / HUB_TIMER2_MHZ) // 6ns for timer2 tick
#define HUB_PERIOD2_MAX_NS (500 * HUB_COLOR_BIT0)
#define HUB_PERIOD2_MIN_NS 100
//#define HUB_PERIOD2_MIN_NS (HUB_TIMER2_NS << 1)

//#define HUB_GAMMA 1.2 // - Good for Gamma Test
//#define HUB_GAMMA 2.2 // - Good for Photo
//#define HUB_GAMMA 2.5 // - Bars
#define HUB_GAMMA 2.8 // - Max's photo

#define HUB_ROWS 16
#define HUB_ROWS_MASK (HUB_ROWS - 1)
#define HUB_MIN_PERIOD_USEC 20

extern int hubBrightness;

#define SCREEN_PAGES 2
#define SCREEN_W 64
#define SCREEN_H 32
#define SCREEN_BYTES (SCREEN_W * SCREEN_H * sizeof(color_t))
#define SCREEN_LEN (SCREEN_W * SCREEN_H)

#pragma anon_unions

typedef union {
		uint32_t ARGB;
		struct {
			uint8_t B;
			uint8_t G;
			uint8_t R;
			uint8_t A;
		};
	} color_t;

typedef union {
	color_t L[SCREEN_LEN];
	color_t W[SCREEN_H][SCREEN_W];
	color_t H[SCREEN_W][SCREEN_H];
}	screen_t;

typedef enum {
	HUB_ROTATE_NOTSET = -1,
	HUB_ROTATE_0 = 0,
	HUB_ROTATE_90,
	HUB_ROTATE_180,
	HUB_ROTATE_270
} orient_t;

extern orient_t hubOrientation;
void hubSetOrient(orient_t orient); // Change Display Orientation

extern int screenW, screenH; // Real Screen Width & Height after Orientation 

extern char hubNeedRedraw;
#define screenRedraw() hubNeedRedraw = true // Request to Redraw Screen at Next vertical sync
#define screenUpdated() (!hubNeedRedraw)	// true, if screen already updated

extern screen_t matrix[SCREEN_PAGES];
extern screen_t * screen, * display, * screen_temp;
#define SCREEN (*screen).W // Wide Screen Array [H][W] pixels
#define SCR (*screen) // Shortcut to screen_t
#define DISPLAY (*display).L // Display Page [LEN] pixels 
	
#define swapPages() screen_temp = display; display = screen; screen = screen_temp;  hubNeedRedraw = false  
	
#define HUB_LUT_NUM 256
extern uint16_t hubLUT[HUB_LUT_NUM];
	
void hubInit(void);
void hubLUTInit(void);
void hubScreenTest(void);	
void hubScreenBmp(void);

#define COLOR(x) ((color_t){ .ARGB = (x) })
#define COLOR_ARGB(a, r, g, b) ((color_t){ .A = (a), .R = (r), .G = (g), .B = (b) })
#define COLOR_RGB(r, g, b) ((color_t){ .A = 0xff, .R = (r), .G = (g), .B = (b) })

void inline clearScreen(void);
void inline copyScreen(screen_t * dst, screen_t * src);
void fillScreen(color_t color);
	
#define atScreen(x, y) (((x) >= 0) && ((x) < screenW) && ((y) >= 0) && ((y) < screenH)) 

#define drawPixel(x, y, color) if (atScreen((x), (y))) { SCR.L[screenW * y + x] = (color); }
#define getPixel(x, y) ((atScreen((x), (y))) ? (SCR.L[screenW * y + x]) : COLOR(0))
	
#endif
