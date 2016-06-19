/**
  ******************************************************************************
  * @file    dcf77.c
  * @author  Maxim Enbaev
  * @version V2.0
  * @date    28-September-2015
  * @brief   This file provides DCF77 signal reception library for STM32F4 microcontrollers
  ******************************************************************************
  * @attention
  * 
  * Copyright (c) 2015 Maxim Enbaev emax@cosmostat.com
  * 
  * This work is licensed under a Creative Commons
  * Attribution-NonCommercial 4.0 International License
  * 
  * CC BY-NC 4.0
  * 
  * http://creativecommons.org/licenses/by-nc/4.0/
  * 
  * You are free to:
  * Share — copy and redistribute the material in any medium or format
  * Adapt — remix, transform, and build upon the material
  * 
  * Under the following terms:
  * Attribution — You must give appropriate credit,
  * provide a link to the license, and indicate if changes were made.
  * You may do so in any reasonable manner, but not in any way that suggests
  * the licensor endorses you or your use.
  * NonCommercial — You may not use the material for commercial purposes
  * without permission from the author.
  * 
  ******************************************************************************
  */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "dcf77.h"

#include "delay.h" //endCountUS

static pin_t dcf77pin = {GPIOC, GPIO_PIN_1};
#define DCF77_PIN_CLOCK_ENABLE() __GPIOC_CLK_ENABLE()

#define DCF77_0MS 100
#define DCF77_1MS 200
#define ERROR_MS 10

#define DCF77_TIME_ZONE 1
int dcf77timeZone = 2;	//Time Zone: Latvia +2
bool dcf77dst = true;			//Day Saving Time: Latvia 1

#define AVERAGE_SEC 300 // 10 min for sync
//#define AVERAGE_SEC 60 // 2 min for sync if msec is float
#define AVERAGE_MIN 10 // 20 min for minute sync

int dcf77level = 0; // 0 - Weak, 3 - Excellent
int dcf77levelPercent = 0; // 0% - Weak, 100% - Excellent

int dcf77sync_ms = 0; // ms of begin sec sync
int dcf77min59sync = 0; // sec of 59 sec sync

#define SYNC_OK_PERCENT 10 // sync Max above Average
#define MIN_SYNC_OK_PERCENT 5 // Min sync Max above Average
int dcf77syncPercent = 0; // Sync Max above Average
int dcf77minSyncPercent = 0; // Min sync Max above Average

#define BIT_OK_PERCENT 66 // difference between 0 and 1 for dirty bits

time2_t dcf77timeOk = 0; // Received DCF77 time or zero
time2_t dcf77timeOk_msec = 0; // Received DCF77 time msec
time2_t dcf77timeOkReceived = 0; // Time of ok receiving DCF77 signal

struct tm dcf77timeOk_tm = { 0 };

int dcf77msec_per_day = DCF77_MSEC_PER_DAY_UNKNOWN; // Correction msec per daynight
int dcf77startSec = 0; // Seconds from start to first result

#define DCF77_BUF_LEN 2
#define DCF77_BUF_LEN_BYTES 8
typedef uint32_t dcf77buffer_t[DCF77_BUF_LEN];
#define DCF77_GET_BIT(buffer, i) (((buffer)[(i) >> 5] & (1 << (31 - ((i) & 31)))) != 0)
#define DCF77_SET_BIT(buffer, i) ((buffer)[(i) >> 5] |= (1 << (31 - ((i) & 31))))
#define DCF77_RESET_BIT(buffer, i) ((buffer)[(i) >> 5] &= (~(1 << (31 - ((i) & 31)))))
static dcf77buffer_t dcf77buf = { 0 }; // buffer with DCF77 data

int dcf77testCnt = 0; // Test Counters
int dcf77testCnt1 = 0;
int dcf77testCnt2 = 0;
int dcf77testCnt3 = 0;
int dcf77testCnt4 = 0;

int dcf77secUs = 0; // uSec counter
int dcf77minUs = 0; // uSec counter

#define GOOD_SYNC_CNT 10 // Quantity near sync
#define SYNC_DELTA_MS 15 // Near sync range ms
//#define AVERAGE_SEC_CNT 20 // Cnt for average sync ms
#define AVERAGE_SEC_CNT 60 // Cnt for average sync ms
static int good_sync_cnt = 0;
static int sync_ms00 = 0;

#define MS_ADD(x, y) (((x) + (y) + 4000) % 1000)
#define MS_DIFF(a, b) (((b) - (a)) > 500?((b) - (a)) - 1000:((((b) - (a)) <= -500)?((b) - (a)) + 1000:(b) - (a)))
#define MS_INC(x) ((x) = ((x) + 1) % 1000)

#define ABS(x) (((x)>=0)?(x):-(x))
#define SIGN(x) ((x > 0)?1:((x < 0)?-1:0))
static int _a;
#define AVERAGE(x, a, n)  _a = (((x) + ((n) - 1)*(*(a)))/(n)); if (_a != *(a)) *(a) = _a; else if ((x) != *(a)) { if ((x) > *(a)) (*(a))++; else (*(a))--;}
static int _x;
#define AVERAGE_CYCL(x, a, n, m) _x = (x); if ((m)) { if (*(a) - (x) > ((m)>>1)) _x += (m); else if (*(a) - (x) <= -((m)>>1)) _x -= (m);} _a = ((_x + ((n) - 1)*(*(a)))/(n)); \
	if (_a == *(a)) { if (_x > _a) _a++; else if (_x < _a) _a--; } if (!(m)) *(a) = _a; else *(a) = (_a + ((m)<<2)) % (m);    

static inline void initPin() {
	DCF77_PIN_CLOCK_ENABLE();
	
	GPIO_InitTypeDef gpioInit;

	gpioInit.Pin = dcf77pin.pin;
	gpioInit.Mode = GPIO_MODE_INPUT;
	gpioInit.Pull = GPIO_PULLUP;
	gpioInit.Speed = GPIO_SPEED_HIGH;
	HAL_GPIO_Init(dcf77pin.port, &gpioInit);
}

void inline dcf77init(void) {
	initPin();
}

#define SIGNED_ONE 30000 // signed short 1 value
#define ZERO_NOISE 6000 // +- statistic noise
#define BOOL2SIGNED(x) ((x)?SIGNED_ONE:-SIGNED_ONE)

#define STAT_LEN 1000
#define LAST_LEN 32
static int16_t syncStat[STAT_LEN] = { 0 }; // Average buffer within sec: -30000 - all 0, 30000 - all 1, near 0 - statistical noise
typedef uint32_t dcf77last_t[LAST_LEN];
static dcf77last_t lastSec = { 0xffffffff };	//Bit array for last 1 sec signal

inline static int convolutionSync(int x) {
	int y = 0, i, j, sum;
	for (i = 0; i < DCF77_0MS; i++) {
		j = MS_ADD(i, x);
		if (syncStat[j] <= -ZERO_NOISE) y++;
	}
	for (i = DCF77_1MS; i < 1000; i++) {
		j = MS_ADD(i, x);
		if (syncStat[j] >= ZERO_NOISE) y++;
	}
	sum = 1000 - (DCF77_1MS - DCF77_0MS);
	return y * SIGNED_ONE / sum; 
}

inline static int convolutionMin(int x) {
	int y = 0, sum, i, j;
	for (i = ERROR_MS; i < DCF77_0MS - ERROR_MS; i++) {
		j = MS_ADD(i, x);
		if (DCF77_GET_BIT(lastSec, j)) y++;
	}
	sum = DCF77_0MS - (ERROR_MS << 1);
	return y * SIGNED_ONE / sum;
}
 
inline static int convolution0(int x) {
	int y = 0, sum, i, j;
	for (i = DCF77_0MS + ERROR_MS; i < DCF77_1MS - ERROR_MS; i++) {
		j = MS_ADD(i, x);
		if (DCF77_GET_BIT(lastSec, j)) y++;
	}
	sum = DCF77_1MS - DCF77_0MS - (ERROR_MS << 1);
	return y * SIGNED_ONE / sum;
}

static time2ms_t timeS = { 0, 0 }; // System time from program start
static int syncMax = 0, syncSum = 0, sync_ms0 = 0, syncAverage = 0;

static bool syncOk = false, minSyncOk = false; // flags successful synchronization
#define GET_DATA_MS 500
static int getData_ms = GET_DATA_MS; // msec for getBit

#define GOOD_SYNC_SEC (15 * 60) // sec interval for good sec sync
#define GOOD_SYNC_MIN (30 * 60) // min interval for good min sync
static int good_sync_sec; // sec from last good sync
static int good_sync_min; // sec from last good min sync

inline static void sync(void) {
	int x = timeS.msec;
	int convo = convolutionSync(x);
	bool error = true;
	if (!x) {	//Begin of second
		syncAverage = syncSum / STAT_LEN;
		if (!syncAverage) dcf77syncPercent = 0;
		else {
			dcf77syncPercent = syncMax * 100 / syncAverage - 100; 
		}
		if (dcf77syncPercent >= SYNC_OK_PERCENT) {
			//New sync value near old
			if (ABS(MS_DIFF(sync_ms0, sync_ms00)) <= SYNC_DELTA_MS) {
				if (good_sync_cnt < GOOD_SYNC_CNT) good_sync_cnt++;
			} else good_sync_cnt = 1;
			sync_ms00 = sync_ms0;
			if (good_sync_cnt >= GOOD_SYNC_CNT) {
				AVERAGE_CYCL(sync_ms0, &dcf77sync_ms, AVERAGE_SEC_CNT, 1000);
				syncOk = true;
				getData_ms = MS_ADD(dcf77sync_ms, GET_DATA_MS);
				good_sync_sec = 0;
				error = false;
			} else if (good_sync_sec < GOOD_SYNC_SEC) {
				error = false;
			} else error = true;
		}
		if (error) {
			syncOk = false;
			minSyncOk = false;
		}
		if (good_sync_sec < GOOD_SYNC_SEC) good_sync_sec++; // Increment secs for break sync
		syncMax = 0;
		syncSum = 0;
	}	
	if (convo > syncMax) {
		syncMax = convo;
		sync_ms0 = x;
	}
	syncSum += convo;
}

static int minCnt = 0; // Min counter

#define MIN_LEN 60
static int16_t min59[MIN_LEN] = { 0 };
static int16_t min0[MIN_LEN] = { 0 };
static int16_t minStat[MIN_LEN] = { 0 };

#define MIN_SYN_59(x) (x)
#define MIN_SYN_0(x) (((x) + 1) % MIN_LEN)
#define MIN_SYN_20(x) (((x) + 1 + 20) % MIN_LEN)

#define MIN_BUF_I(cnt, cnt59) (((cnt) - (cnt59) + 59 + MIN_LEN) % MIN_LEN)

static int min59sync00 = 0; // old min sync for counter

#define GOOD_MIN_SYNC 60 // sec for good sync
static int good_min_sync_cnt; // counter for good min sync

inline static void syncMin(int16_t conMin, int16_t con0) {
	int i = minCnt;
	bool error = true;
	AVERAGE(conMin, &min59[i], AVERAGE_MIN);
	AVERAGE(con0, &min0[i], AVERAGE_MIN);
	minStat[i] = (3 * min59[i] + min0[MIN_SYN_0(i)] + (SIGNED_ONE - min0[MIN_SYN_20(i)])) / 5;
	//Max - Average
	int sum = 0, max = 0, avr = 0, min59sync0 = 0, v;
	for (i = 0; i < MIN_LEN; i++) {
		v = minStat[i];
		if (v > max) {
			max = v;
			min59sync0 = i;
		}
		sum += v;
	}
	avr = sum / MIN_LEN;
	if (avr) dcf77minSyncPercent = max * 100 / avr - 100;
	else dcf77minSyncPercent = 0;
	if (dcf77minSyncPercent >= MIN_SYNC_OK_PERCENT) {
		if (min59sync0 == min59sync00) {
				if (good_min_sync_cnt < GOOD_MIN_SYNC) good_min_sync_cnt++;
		}	else good_min_sync_cnt = 1;
		min59sync00 = min59sync0;
		if (good_min_sync_cnt >= GOOD_MIN_SYNC) {
				dcf77min59sync = min59sync0;
			  minSyncOk = true;
				good_sync_min = 0;
				error = false;
		} else if (good_sync_min < GOOD_SYNC_MIN) {
			error = false;
		} else error = true;
	} else error = true;
	if (error) {
		minSyncOk = false;
	}
	if (good_sync_min < GOOD_SYNC_MIN) good_sync_min++; // Increment min secs for break sync	
}

static bool getParity(int i0, int in) {
	int i;
	char res = 0;
	for (i = i0; i <= in; i++) res ^= DCF77_GET_BIT(dcf77buf, i);
	return res;
}

static int getDigits(dcf77buffer_t buf, int i0, int in) {
	int res = 0, i, j = 1;
	for (i = i0; i <= in; i++) {
		if (DCF77_GET_BIT(buf, i)) res += j;
		j <<= 1;
		if (j == 16) j = 10;
	}
	return res;
}

#define CANDIDATES_LEN 60
#define CANDIDATES_GOOD 3
static time2_t candidate[CANDIDATES_LEN] = { 0 };	// Quene of candidates to good time
static int candidateI = 0;

static bool compareCandidates(time2_t time_sec) {
	int good = 1;
	int i;
	if (time_sec) {
		if (time_sec == dcf77timeOk) good = CANDIDATES_GOOD;
		else {
			for (i = 0; i < CANDIDATES_LEN; i++) {
				if (candidate[i] && candidate[i] == time_sec) {
					if (++good >= CANDIDATES_GOOD) break;
				}
			}
		}
		//Increment candidates quene
		candidate[candidateI] = time_sec;
		candidateI = (candidateI + 1) % CANDIDATES_LEN;
	}	
	return good >= CANDIDATES_GOOD;
}

static int lastOkSec = 0; // System time of last received time
#define IS_NIGHT(x) ((x) >= 0 && (x) < 5)
#define NIGHT_UPDATE_PERIOD (1 * 3600) // Sec for night update callback
#define DAY_UPDATE_PERIOD (24 * 3600) // Sec for night update callback

static bool timeReceived = false, timeReceivedLong = false; // Flags off received time for user tasks

inline static void storeGoodTime(time2_t time_sec) {
		//Store Good candidate
		dcf77timeOk = time_sec;
		dcf77timeOkReceived = time_sec + 1; // 59 -> 00 sec
		dcf77timeOk_msec = GET_DATA_MS;
		struct tm * timeOk_tm = localtime2(&dcf77timeOk);
		memcpy(&dcf77timeOk_tm, timeOk_tm, sizeof(dcf77timeOk_tm));
		
		//Flag for callbacks
		int nowSec = timeS.sec;
		int period = nowSec - lastOkSec;
		if (!dcf77startSec)	dcf77startSec = nowSec;
		if (!lastOkSec || 
			((IS_NIGHT(dcf77timeOk_tm.tm_hour)) && period >= NIGHT_UPDATE_PERIOD) ||
			((!IS_NIGHT(dcf77timeOk_tm.tm_hour)) && period >= DAY_UPDATE_PERIOD)) {
			timeReceived = true;
		}
		lastOkSec = nowSec;	
}

static time2ms_t yesterday77 = { 0, 0 }, yesterdayS = { 0, 0 }; // Yesterday time DCF77 and sys timer
#define SEC_PER_DAY 86400 // sec per daynight
#define IS_MSEC_CORRECTION_HOUR(x) ((x) >= 0 && (x) < 5)

inline static void calculateMSecPerDay(time2_t dcf77sec, int dcf77msec) {
	time2ms_t now77, nowS, diff77, diffS, diffMs;
	int msec, measure_time_s;
	TM2MS_COPY(nowS, timeS);
	now77.sec = dcf77sec; now77.msec = dcf77msec;
	if (yesterday77.sec) {
		time2msSub(&now77, &yesterday77, &diff77);
		time2msSub(&nowS, &yesterdayS, &diffS);
		time2msSub(&diffS, &diff77, &diffMs);
		if (diffMs.sSec > DCF77_MSEC_PER_DAY_MAXIMUM) {
			dcf77msec_per_day = DCF77_MSEC_PER_DAY_MAXIMUM; 
		} else if (diffMs.sSec < -DCF77_MSEC_PER_DAY_MAXIMUM) {
			dcf77msec_per_day = -DCF77_MSEC_PER_DAY_MAXIMUM; 
		} else {
			msec = diffMs.sSec * 1000 + diffMs.msec;
			measure_time_s = diffS.sSec + ((diffS.msec >= 500)?1:0);
			if (measure_time_s < (SEC_PER_DAY >> 1)) {
				dcf77msec_per_day = DCF77_MSEC_PER_DAY_SHORT_PERIOD;
			} else {
				int msec_per_day = ((((msec * SEC_PER_DAY) << 1) + SIGN(msec)) / measure_time_s) >> 1; 	
				if (msec_per_day > DCF77_MSEC_PER_DAY_MAXIMUM) {
					dcf77msec_per_day = DCF77_MSEC_PER_DAY_MAXIMUM; 
				} else if (msec_per_day < -DCF77_MSEC_PER_DAY_MAXIMUM) {
					dcf77msec_per_day = -DCF77_MSEC_PER_DAY_MAXIMUM; 
				} else {
					dcf77msec_per_day = msec_per_day;
				}
			}
		}
	}
	TM2MS_COPY(yesterday77, now77);
	TM2MS_COPY(yesterdayS, nowS);
}

#define YEAR_BEGIN 2015 // Year of begin for validate
//static int last_good_night = 0; // day of last night ok result
int last_good_night = 0; // day of last night ok result

static int counter;

static void decodeBuf() {
	bool d1, d2, p1, p2, p3, error = false;
	struct tm time;
	time2_t time_sec = 0;
	int local_sec;
	if (dcf77buf[0] || dcf77buf[1]) {
			dcf77testCnt++; // Test Counters
		
			p1 = getParity(21, 28);
			p2 = getParity(29, 35);
			p3 = getParity(36, 58);
			error = p1 || p2 || p3;
			if (!error) {
					dcf77testCnt1++; // Test Counters

					d1 = DCF77_GET_BIT(dcf77buf, 17); // DST Bits
				  d2 = DCF77_GET_BIT(dcf77buf, 18);
					time.tm_sec = 0;
					time.tm_min = getDigits(dcf77buf, 21, 27);
					time.tm_hour = getDigits(dcf77buf, 29, 34);
					time.tm_mday = getDigits(dcf77buf, 36, 41);
					time.tm_wday = 0;
					time.tm_mon = getDigits(dcf77buf, 45, 49) - 1; // Month: 0 - 11
					time.tm_year = getDigits(dcf77buf, 50, 57) + 100; // Years: 1900 - ...
					time.tm_isdst = 0;
					//Validate Time
					if (time.tm_min > 59 || time.tm_hour > 23 || time.tm_mday > 31 || time.tm_mon > 11 || time.tm_year < (YEAR_BEGIN - 1900) || time.tm_year >= 200) {
						error = true;
					} else {
						time_sec = mktime2(&time);
						if (d1 && !d2 && !dcf77dst) {
							local_sec = (dcf77timeZone - DCF77_TIME_ZONE - 1) * 60 * 60;
						} else {
							local_sec = (dcf77timeZone - DCF77_TIME_ZONE) * 60 * 60;
						}
						time_sec += local_sec;
					}
			}
	} else error = true;
	if (!error && time_sec) {
		dcf77testCnt2++; // Test Counters

		time_sec--; // 59.5 sec
		if (compareCandidates(time_sec)) {
			dcf77testCnt3++; // Test Counters

			storeGoodTime(time_sec);
			
			if (dcf77timeOk_tm.tm_mday != last_good_night && 
				IS_MSEC_CORRECTION_HOUR(dcf77timeOk_tm.tm_hour)) {
					//Good result between 00:00 - 04:59 at Night
				calculateMSecPerDay(dcf77timeOk, GET_DATA_MS);
				last_good_night = dcf77timeOk_tm.tm_mday;
			}
			dcf77minUs = endCountUS(&counter);
		}
	}
}

#define DIRTY_NOISE_LEVEL_PERCENT 66 // 2/3 - random noise for calculate signal level
static int dirtyBits0 = 0; // Dirty bits for last minute

inline static void getLevel(void) {
	int dirtyBits = dirtyBits0;
	int percent = 100 - 100 * dirtyBits / 59;
	if (percent < 0) percent = 0;
	else if (percent > 100) percent = 100;
	int level = percent / 25;
	if (level > 3) level = 3;
	dcf77levelPercent = percent;
	dcf77level = level;
}

inline static void getBit(void) {
	int con0 = convolution0(dcf77sync_ms);
	int con1 = SIGNED_ONE - con0;
	int conMin = convolutionMin(dcf77sync_ms);
	
	syncMin(conMin, con0);
	
	if (syncOk && minSyncOk) {
		//Sync Ok
		int i = MIN_BUF_I(minCnt, dcf77min59sync);
		if (i == 59) {
			//59 bit
			decodeBuf();
			getLevel();
			dirtyBits0 = 0;
		} else {
			//0..1
			//Count Dirty bits
			int dx = 100 * ABS(con0 - (SIGNED_ONE >> 1)) / (SIGNED_ONE >> 1);
			if (dx <= DIRTY_NOISE_LEVEL_PERCENT) dirtyBits0++;
			
			//Set 0..1 to dcf77buf
			if (con1 > con0) DCF77_SET_BIT(dcf77buf, i);
			else DCF77_RESET_BIT(dcf77buf, i);

			//Clearing timeReceived Flag
			if (timeReceivedLong) {
				if (i > 5 && i < 10) {
					timeReceived = false;
					timeReceivedLong = false;
				}
			}
			dcf77secUs = endCountUS(&counter);
		}	
	} else {
		dcf77levelPercent = 0;
		dcf77level = 0;
	}
}

__weak void dcf77timeReceived(time2_t now, struct tm * now_tm) {
}
__weak void dcf77timeReceivedLong(time2_t now, int msec, struct tm * now_tm) {
}

//Test signal
#define TEST_LEN 60
//Mo, 24.08.15 11:23:00
static char testSignal[TEST_LEN][59] = {
"01100100101001100100111000101100010000100110000010101010001",    
"00100100111101000100100100100100010000100110000010101010001",    
"00111111000000000100110100101100010000100110000010101010001",    
"01111100111101100100101100101100010000100110000010101010001",    
"01110001100001000100111100100100010000100110000010101010001",    
"00001001010001100100100010100100010000100110000010101010001",    
"01010101000100100100110010101100010000100110000010101010001",    
"00000000111110000100100001100100010000100110000010101010001",    
"00011100001110100100110001101100010000100110000010101010001",    
"01010111100100000100101001101100010000100110000010101010001",    
"00110011001101100100111001100100010000100110000010101010001",    
"00101101011100100100100101101100010000100110000010101010001",    
"00001101010010000100110101100100010000100110000010101010001",    
"00001000100100100100101101100100010000100110000010101010001",    
"00110011000011000100111101101100010000100110000010101010001",    
"00011111001011100100100011101100010000100110000010101010001",    
"00111111110101100100110011100100010000100110000010101010001",    
"00100011011001100100100000011100010000100110000010101010001",    
"01100011000101000100110000010100010000100110000010101010001",    
"01011001100101100100101000010100010000100110000010101010001",    
"00011011001011100100111000011100010000100110000010101010001",    
"00010101011010000100100100010100010000100110000010101010001",    
"00000010000010000100110100011100010000100110000010101010001",    
"00010001000100100100101100011100010000100110000010101010001",    
"01010000110010100100111100010100010000100110000010101010001",    
"01100111010011100100100010010100010000100110000010101010001",    
"00010011000111100100110010011100010000100110000010101010001",    
"01101010001010000100100001010100010000100110000010101010001",    
"01001100111011100100110001011100010000100110000010101010001",    
"00110110000100000100101001011100010000100110000010101010001",    
"01111010110100000100111001010100010000100110000010101010001",    
"00110100110000100100100101011100010000100110000010101010001",    
"00001001001001000100110101010100010000100110000010101010001",    
"00000110000101000100101101010100010000100110000010101010001",    
"00111011011011000100111101011100010000100110000010101010001",    
"00100011011010000100100011011100010000100110000010101010001",    
"00101010010000100100110011010100010000100110000010101010001",    
"01111101000110000100100000000010010000100110000010101010001",    
"00100011001110100100110000001010010000100110000010101010001",    
"00110111000011100100101000001010010000100110000010101010001",    
"01010001111001100100111000000010010000100110000010101010001",    
"00001001000100100100100100001010010000100110000010101010001",    
"00001111001001000100110100000010010000100110000010101010001",    
"00011011001000100100101100000010010000100110000010101010001",    
"00001010010000100100111100001010010000100110000010101010001",    
"01000100101001000100100010001010010000100110000010101010001",    
"01010001101001000100110010000010010000100110000010101010001",    
"00100011011101100100100001001010010000100110000010101010001",    
"00001111010101100100110001000010010000100110000010101010001",    
"00101011011110000100101001000010010000100110000010101010001",    
"00000011011111000100111001001010010000100110000010101010001",    
"01000101111011100100100101000010010000100110000010101010001",    
"01110110011011100100110101001010010000100110000010101010001",    
"00011000000010100100101101001010010000100110000010101010001",    
"00001010110011100100111101000010010000100110000010101010001",    
"00100000100011100100100011000010010000100110000010101010001",    
"00010001010001100100110011001010010000100110000010101010001",    
"00000011011011100100100000101010010000100110000010101010001",    
"00001110100110000100110000100010010000100110000010101010001"    
};


static int testMinute = 0;
static char sigChar = 2;
#define RANDOM(x) (((rand() & 4095) * (x)) >> 12)

inline static bool dcf77test(void) {
	int time = timeS.sec * 1000 + timeS.msec - 11800;
	if (time < 0) time = 0;
	int msec = time % 1000;
	int sec60 = (time / 1000) % 60;
	
	if (sec60 == 0 && msec == 0) {
		testMinute++;
		if (testMinute >= TEST_LEN)
			testMinute = 0;
	}
	char res = true;
	if (msec == 0) {
		if (sec60 < 59) sigChar = testSignal[testMinute][sec60] - 0x30;
		else sigChar = 2;
	}
	if (time <= 0) res = 0;
	else {
		//Good signal
		if ((sigChar == 0) && (msec < 100)) res = 0;
		else if ((sigChar == 1) && (msec < 200)) res = 0;
		else res = 1; //59 bit and between secs
		//Noise
		if (RANDOM(100) < DCF77_TEST_NOISE) res = RANDOM(2);
	}	
	return res;
}

void dcf77msecPerDayTest(void) {
	timeS.sec = 10000; timeS.msec = 200;
	calculateMSecPerDay(10000, 200);
	timeS.sec = 13600; timeS.msec = 200;
	calculateMSecPerDay(13600, 204);
	//int msec = dcf77msec_per_day;
}

inline static void readPort(void) {
	bool pin;
	int16_t sPin;
	int time = timeS.msec;
	if (!DCF77_TEST) {
		pin = pinRead(dcf77pin);
		if (DCF77_PIN_POLARITY) pin = !pin;
	} else pin = dcf77test();
	sPin = BOOL2SIGNED(pin);
	
	if (pin) DCF77_SET_BIT(lastSec, time);
	else DCF77_RESET_BIT(lastSec, time);
		
	AVERAGE(sPin, &syncStat[time], AVERAGE_SEC);
}

inline static void candidatesInc() {
		int i;
		for (i = 0; i < CANDIDATES_LEN; i++) {
			if (candidate[i]) candidate[i]++;
			else return;
		}
}

bool dcf77received = false; //True if DCF77 had received in last daynight

inline static void incrementSec(void) {
		candidatesInc();

		if (dcf77timeOk)	{
			dcf77timeOk++;
			dcf77timeOk_msec = 0;
			struct tm * timeOk_tm = localtime2(&dcf77timeOk);
			memcpy(&dcf77timeOk_tm, timeOk_tm, sizeof(dcf77timeOk_tm));
		}
		if (lastOkSec) {
			int nowSec = timeS.sec;
			int period = nowSec - lastOkSec;
			if (period <= DCF77_RECEIVED) dcf77received = true;
			else dcf77received = false;
		}
		if (timeReceived) {
			timeReceivedLong = true;
			timeReceived = false;
			dcf77timeReceived(dcf77timeOk, &dcf77timeOk_tm);
			dcf77testCnt4++;
		}

		minCnt++;
		if (minCnt >= MIN_LEN) minCnt = 0;
}

static int newSecond0; // margin of new second
static int newData0; // margin for get Data

inline static void increment_ms(void) {
	TM2MSS_INC(timeS);
	MS_INC(dcf77timeOk_msec);
	int newSecond = MS_DIFF(dcf77sync_ms, timeS.msec);
	if (newSecond >=0 && newSecond0 < 0) {	//Margin of second
		incrementSec();
	}
	newSecond0 = newSecond;
}

void dcf77msTask(void) {
	readPort();
	sync();
	int newData = MS_DIFF(getData_ms, timeS.msec);
	if (newData >=0 && newData0 < 0) {	//Time for get Data
		getBit();
	}
	//Test msec per day
	//dcf77msecPerDayTest();
	newData0 = newData;
	increment_ms();
}	

void dcf77taskLong() {
	if (timeReceivedLong) {
				timeReceivedLong = false;
				dcf77timeReceivedLong(dcf77timeOk, dcf77timeOk_msec, &dcf77timeOk_tm);
	}
}




