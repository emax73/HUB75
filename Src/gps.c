#include "gps.h"
#include "uart.h"
#include "time2.h"
#include <string.h>

int gpsTimeZone = 2;	//Time Zone: Latvia +2
gpsDST_t gpsDST = GPS_DST_AUTO; //Day Saving Time: Latvia -1

time2ms_t gpsTime = { 0 }; //Current Time with msec
struct tm gpsTime_tm = { 0 }; //Current time struct

void gpsInit(void) {
	uartInit();
}

//10:46:28  $GPGLL,,,,,104628.00,V,N*43
//10:46:29  $GPRMC,104629.00,V,,,,,,,041015,,,N*74
//10:46:29  $GPVTG,,,,,,,,,N*30

char gps_time[20] = "", gps_date[20] = "";

static char * strtok1(char * str, char * dels) {
	static char * str0 = NULL;
	char * res = NULL, * d;
	if (str) str0 = str;
	if (!str0) return NULL;
	d = strpbrk(str0, dels);
	if (d) {
		*d = 0;
		res = str0;
		str0 = ++d;
	} else if (*str0) {
		res = str0;
		str0 = NULL;
	}
	return res;
}

#define CHR2DEC(c) ((c)>='0'&&(c)<='9'?(c)-'0':0) 
#define CHR2HEX(c) ((c)>='0'&&(c)<='9'?(c)-'0':((c)>='A'&&(c)<='F'?(c)-'A'+10:((c)>='a'&&(c)<='f'?(c)-'a'+10:(0)))) 

static char getDateTime(char * time, char * date, struct tm * res, int * msec) {
	if (!res || !msec) return false;
	if (!time || !date) return false;
	int t_len = strlen(time);
	int msec0 = 0;
	if (t_len < 6 || strlen(date) < 6) return false;
	res->tm_hour = CHR2DEC(time[0]) * 10 + CHR2DEC(time[1]);
	res->tm_min = CHR2DEC(time[2]) * 10 + CHR2DEC(time[3]);
	res->tm_sec = CHR2DEC(time[4]) * 10 + CHR2DEC(time[5]);
	if (t_len > 6 && time[6] == '.') {
		if (t_len >= 8) msec0 = 100 * CHR2DEC(time[7]);
		if (t_len >= 9) msec0 += 10 * CHR2DEC(time[8]);
		if (t_len >= 10) msec0 += CHR2DEC(time[9]);
		*msec = msec0;
	} else {
		*msec = 0;
	}
	res->tm_mday = CHR2DEC(date[0]) * 10 + CHR2DEC(date[1]);
	res->tm_mon = CHR2DEC(date[2]) * 10 + CHR2DEC(date[3]) - 1;
	res->tm_year = CHR2DEC(date[4]) * 10 + CHR2DEC(date[5]) + 100;
	return true;
}

static char crc(char *line) {
	char res = false;
	char * c0 = strchr(line, '$');
	char * cn = strchr(line, '*');
	int sum1 = 0, sum2;
	char * c;
	sum2 = (CHR2HEX(cn[1])<<4)|CHR2HEX(cn[2]);
	if (c0 && cn) {
		c0++;
		for (c = c0; c < cn; c++) sum1 ^= *c; 
		res = (sum1 == sum2);
	}
	return res;
}

static int dstDays[] = {
	26,25,31,30,28,27,26,25,30,29, // 2000-2009
	28,27,25,31,30,29,27,26,25,31, // 2010-2019
	29,28,27,26,31,30,29,28        // 2020-2027
};

static inline void getDSTDays(int year, int * day1, int * day2) {
	int y1 = (year - 100 + 280) % 28;
	int y2 = (year - 100 - 8 + 280) % 28;
	*day1 = dstDays[y1];
	*day2 = dstDays[y2];
}

static inline char isDST(struct tm * time) {
	char res = false;
	int d1, d2;
	getDSTDays(time->tm_year, &d1, &d2);
	if (time->tm_mon > 2 && time->tm_mon < 9) res = true;
	else if (time->tm_mon < 2 || time->tm_mon > 9) res = false;
	else if (time->tm_mon == 2) {	//Marth
		if (time->tm_mday < d1) res = false;
		else if (time->tm_mday > d1) res = true;
		else {	//Last Sunday of Marth
			if (time->tm_hour >= 1) res = true; // 01:00 GMT
			else res = false;
		}
	} else if (time->tm_mon == 9) { //October
		if (time->tm_mday < d2) res = true;
		else if (time->tm_mday > d2) res = false;
		else {	//Last Sunday of October
			if (time->tm_hour >= 1) res = false; // 01:00 GMT
			else res = true;
		}
	}
	return res;
}

static void testDST(void) {
	struct tm time;
	char dst;
	time.tm_hour = 3;
	time.tm_min = 0;
	time.tm_sec = 0;
	time.tm_mday = 25;
	time.tm_mon = 9; // 25 Oct 2015 false
	time.tm_year = 115;
	dst = isDST(&time);

	time.tm_hour = 3;
	time.tm_min = 0;
	time.tm_sec = 0;
	time.tm_mday = 24;
	time.tm_mon = 9; // 24 Oct 2015 true
	time.tm_year = 115;
	dst = isDST(&time);

	time.tm_hour = 3;
	time.tm_min = 0;
	time.tm_sec = 0;
	time.tm_mday = 21;
	time.tm_mon = 1; // 21 Feb 2015 false
	time.tm_year = 115;
	dst = isDST(&time);

	time.tm_hour = 3;
	time.tm_min = 0;
	time.tm_sec = 0;
	time.tm_mday = 22;
	time.tm_mon = 1; // 21 Feb 2015 true
	time.tm_year = 115;
	dst = isDST(&time);
	
}

time2ms_t timeS = { 0, 0 }; // System time from program start
static time2ms_t firstGPS = { 0, 0 }, firstS = { 0, 0 }; // Yesterday time DCF77 and sys timer
#define SEC_PER_DAY 86400 // sec per daynight
#define IS_MSEC_CORRECTION_HOUR(x) ((x) >= 0 && (x) < 5)
int gps_msec_per_day = 0;
int gps_msec_per_day_error = 0;
#define SIGN(x) ((x > 0)?1:((x < 0)?-1:0))

inline static void calculateMSecPerDay(time2_t secGPS, int msecGPS) {
	time2ms_t nowGPS, nowS, diffGPS, diffS, diffMs;
	int msec, measure_time_s;
	nowGPS.sec = secGPS; nowGPS.msec = msecGPS;
	TM2MS_COPY(nowS, timeS);
	if (!firstGPS.sec) {
		TM2MS_COPY(firstGPS, nowGPS);
		TM2MS_COPY(firstS, nowS);
	} else {
		time2msSub(&nowGPS, &firstGPS, &diffGPS);
		time2msSub(&nowS, &firstS, &diffS);
		time2msSub(&diffS, &diffGPS, &diffMs);
		msec = diffMs.sSec * 1000 + diffMs.msec;
		measure_time_s = diffS.sSec + ((diffS.msec >= 500)?1:0);
		if (!measure_time_s) {
			gps_msec_per_day = 0;
			gps_msec_per_day_error = GPS_MSEC_PER_DAY_ERROR_UNKNOWN;
		} else {
			gps_msec_per_day = ((((msec * SEC_PER_DAY) << 1) + SIGN(msec)) / measure_time_s) >> 1; 	
			gps_msec_per_day_error = SEC_PER_DAY / measure_time_s; 	
		}
	}	
}

__weak void gpsTimeReceived(time2_t now, struct tm * now_tm) {
}

static int updateHours0 = -1;

static void parse(char * line) {
	char * word, * time = NULL, * date = NULL;
	int i = 0;
	struct tm time_tm, * loc_time_tm;
	time2_t time_sec, loc_time_sec;
	int msec;
	char dst = 0;
	
	if (crc(line)) {
		//strcpy(lin, line);
		word	= strtok1(line, ",");
		if (word != NULL) {
			if (strstr(word, "RMC") != NULL) {
				while (word != NULL) {
					word = strtok1(NULL, ",");
					i++;
					if (i == 1) { //Time
						time = word;
					} else if (i == 9) { //Date
						date = word;
					}
				}
			}
		}
	}
	if (time && date) {
		if (getDateTime(time, date, &time_tm, &msec)) {
			time_sec = mktime2(&time_tm);
			loc_time_sec = time_sec + gpsTimeZone * 60 * 60;
			if (gpsDST == GPS_DST_AUTO) dst = isDST(&time_tm);
			else if (gpsDST == GPS_DST_ON) dst = true;
			else dst = false;
			if (dst) loc_time_sec += 60 * 60;
			calculateMSecPerDay(loc_time_sec, msec);
			gpsTime.sec = loc_time_sec;
			gpsTime.msec = msec;
			loc_time_tm = localtime2(&loc_time_sec);
			memcpy(&gpsTime_tm, loc_time_tm, sizeof(gpsTime_tm));
			if (!msec) { //Round sec
				int updateHours = gpsTime_tm.tm_hour / GPS_RECEIVED_UPDATE_HOURS;
				if (updateHours != updateHours0) {
					gpsTimeReceived(gpsTime.sec, &gpsTime_tm);
					updateHours0 = updateHours;
				}
			}
		} else {
			time_sec = 0;
			msec = 0;
		}
	}
}

static inline void incMs(void) {
	TM2MSS_INC(timeS);
	if (gpsTime.sec) {
		TM2MSS_INC(gpsTime);
		if (!gpsTime.msec) {
			struct tm * time_tm = localtime2(&gpsTime.sec);
			memcpy(&gpsTime_tm, time_tm, sizeof(gpsTime_tm));
		}
	}
}

void gpsMsTask(void) {
	uart_buffer_t * b = uart_line.result;
	if (uart_line.ready) {
		uart_line.ready = false;
		if (b->len) parse(b->buffer);
	}
	incMs();
}

