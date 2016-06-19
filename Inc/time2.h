//Library to addition to <time.h> with 2000 - 2099 years

#ifndef __TIME2_H
#define __TIME2_H

#include <time.h>

#define TM2_MAX_DATE 3155673600ul

//3 letters names of months and weekdays
extern const char tm2_months3[12][4];
extern const char tm2_weekdays3[8][4];

typedef	unsigned int time2_t;	/* seconds from 1 Jan 2000 to 31 Dec 2135 */

#pragma anon_unions

typedef struct {
	union {
		unsigned int sec;
		signed int sSec;
	};
		int msec;
} time2ms_t;

#define TM2MSS_INC(x) if (++(x).msec >= 1000) { (x).sec++; (x).msec = 0; } 
#define TM2MS_INC(x) ((x).msec = ((x).msec + 1) % 1000) 
#define TM2MS_COPY(dst, src) (dst).sec = (src).sec; (dst).msec = (src).msec  
#define TM2MS_CLEAR(x) (x).sec = 0; (x).msec = 0  
extern void time2msSub(time2ms_t * a, time2ms_t * b, time2ms_t * res);

#define tm2_is_leap(y) (((y) & 3) == 0)
extern int tm2_days_in_month[2][12];

#define	difftime2(t1, t0)	((long)((time2_t)(t1)-(time2_t)(t0)))

struct tm *	localtime2(const time2_t *);	/* local time */
time2_t	mktime2(struct tm *);
#define ctime2(t) (asctime(localtime2(t)))

#endif
