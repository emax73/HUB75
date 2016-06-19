#include "time2.h"

#define TM2_SEC_IN_DAYNIGHT 86400
#define DAYS_IN_8_YEARS 2922

const char tm2_months3[12][4] = {
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"
};

const char tm2_weekdays3[8][4] = {
	"Sun",
	"Mon",
	"Tue",
	"Wed",
	"Thu",
	"Fri",
	"Sat",
	"Sun"
};

int tm2_days_in_year[2] = {
	365,
	366
};
int tm2_days_in_month[2][12] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

time2_t	mktime2(struct tm * t) {
	int time = ((t->tm_hour * 60) + t->tm_min) * 60 + t->tm_sec;
	int year = t->tm_year - 100; // 1900 -> 2000
	if (year < 0) return 0;
	else if (year > 99) return TM2_MAX_DATE;
	
	int date = 0;
	int i;
	for (i = 0; i + 7 < year; i += 8) {
		date += DAYS_IN_8_YEARS;
	}
	for (; i < year; i++) {
		date += tm2_days_in_year[tm2_is_leap(i)];
	}
	int yday = 0;
	char l = tm2_is_leap(year);
	for (i = 0; i < t->tm_mon; i++) {
		yday += tm2_days_in_month[l][i];
	}
	yday += t->tm_mday - 1;
	date += yday;
	//Extend struct tm
	t->tm_wday = (date + 6) % 7; 
	t->tm_yday = yday;

	return (date * TM2_SEC_IN_DAYNIGHT) + time;
}

struct tm *	localtime2(const time2_t * t) {
	time2_t t1 = *t;
	struct tm tm;
	
	if (t1 > TM2_MAX_DATE) {
		t1 = TM2_MAX_DATE;
	}
	int date = (t1 / TM2_SEC_IN_DAYNIGHT);
	int time = t1 % TM2_SEC_IN_DAYNIGHT;
	tm.tm_wday = (date + 6) % 7; 
	int i;
	int year = 0;
	for (i = 0; date >= DAYS_IN_8_YEARS; i += 8) {
		date -= DAYS_IN_8_YEARS;
	}
	for (; date >= tm2_days_in_year[tm2_is_leap(i)]; i ++ ) {
		date -= tm2_days_in_year[tm2_is_leap(i)];
	}
	year = i;
	tm.tm_yday = date;
	char l = tm2_is_leap(year);
	tm.tm_year = 100 + year;
	for (i = 0; date >= tm2_days_in_month[l][i]; i++) {
		date -= tm2_days_in_month[l][i];
	}
	tm.tm_mon = i;
	tm.tm_mday = date + 1;
	tm.tm_sec = time % 60;
	time /= 60;
	tm.tm_min = time % 60;
	time /= 60;
	tm.tm_hour = time;
	tm.tm_isdst = -1;
 
	return &tm;
}

void time2msSub(time2ms_t * a, time2ms_t * b, time2ms_t * res) {
	int s = a->sec - b->sec;
	int m = a->msec - b->msec;
	if (m < 0) {
		s--;
		m += 1000;
	}
	res->sSec = s;
	res->msec = m;
}

