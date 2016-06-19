#ifndef __GPS_H
#define __GPS_H

#include <stdbool.h>
#include "time2.h"

extern char gps_time[20], gps_date[20];
extern time2ms_t gpsTime;
extern struct tm gpsTime_tm;

extern int gpsTimeZone;	//Time Zone: Latvia +2
typedef enum {
	GPS_DST_AUTO = -1,
	GPS_DST_OFF  = 0,
	GPS_DST_ON = 1
}	gpsDST_t;
extern gpsDST_t gpsDST; //Day Saving Time: Latvia -1

#define GPS_MSEC_PER_DAY_ERROR_UNKNOWN 100000
extern int gps_msec_per_day;
extern int gps_msec_per_day_error;
#define GPS_RECEIVED_UPDATE_HOURS 6

void gpsInit(void);
void gpsMsTask(void);
void gpsTimeReceived(time2_t now, struct tm * now_tm);

	#endif

