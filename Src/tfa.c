#include "pins.h"
#include "tfa.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "stm32f4xx_hal.h"

static pin_t tfaPin = {GPIOC, GPIO_PIN_3};
#define TFA_PIN_CLOCK_ENABLE() __GPIOC_CLK_ENABLE()

void tfaInit(void) {
	TFA_PIN_CLOCK_ENABLE();
	
	GPIO_InitTypeDef gpioInit;

	gpioInit.Pin = tfaPin.pin;
	gpioInit.Mode = GPIO_MODE_INPUT;
	gpioInit.Pull = GPIO_PULLUP;
	gpioInit.Speed = GPIO_SPEED_HIGH;
	HAL_GPIO_Init(tfaPin.port, &gpioInit);
}


static char level0 = false;
#define MAX_LEN_US 3000

static enum {
	ST_NOSIGNAL = 0,
	ST_SYNC,
	ST_HEADER,
	ST_DATA
} state = ST_NOSIGNAL;

static int pulse = MAX_LEN_US, pause = MAX_LEN_US, pause0 = MAX_LEN_US, period = MAX_LEN_US, duty = 0;
#define PACKETS_NUM 12
#define PACKETS_NUM_MIN 3
#define PACKET_LEN 40
#define PACKET_LEN_B (PACKET_LEN >> 3)
static char data[PACKETS_NUM][PACKET_LEN_B] = { 0 };
#define SET_DATA(j, i, d) if ((j) < PACKETS_NUM) {((d)?(data[(j)][(i) >> 3] |= (1 << ((7 - (i) & 7)))):(data[(j)][(i) >> 3] &= ~(1 << ((7 - (i) & 7)))));}
#define GET_DATA(j, i) (((j) < PACKETS_NUM)?((data[(j)][(i) >> 3] & (1 << ((7 - (i) & 7)))) != 0):0)
static int packets_i = 0, data_i = 0;
static char dataOk[PACKET_LEN_B] = { 0 };
int tfaLevel; // 0 - Lost, 1 - Weak, 5 - Exellent
													
#define MEASURE_LEN 8
static int measure[MEASURE_LEN] = { 0 };
static int bitTreshold = 41;

static char interestMask[] = {
	0xff,
	0x8f,
	0xff,
	0xff,
	0x00,
};

#define PACKAGES_EQUAL(i, j) (((data[(i)][0] & interestMask[0]) == (data[(j)][0] & interestMask[0])) && ((data[(i)][1] & interestMask[1]) == (data[(j)][1] & interestMask[1])) && ((data[(i)][2] & interestMask[2]) == (data[(j)][2] & interestMask[2])) && ((data[(i)][3] & interestMask[3]) == (data[(j)][3] & interestMask[3])) && ((data[(i)][4] & interestMask[4]) == (data[(j)][4] & interestMask[4])))

#define TFA_LEVEL_LOST_US (15 * 60 * 1000000) // 15 min for level 0
#define TFA_LOST_US (30 * 60 * 1000000) // 30 min for temperature lost
static int tfaLostUs = TFA_LOST_US;

char tfaSearchSensor = true; // If True search for sensor
static char sensorId = 0;

int tfaGoodPackages = 0; // Number of Good Packages 0..12
int tfaNumPackages = 0; // Overal number of Packages 0..12
static char badData[PACKETS_NUM] = { 0 };

static void comparePackets(void) {
	int i, j, v, level;
	int max_i = 0, max_v = 1;
	for (i = 0; i < packets_i; i++) {
		if (badData[i]) continue;
		v = 1;
		for (j = i + 1; j < packets_i; j++) {
			if (badData[j]) continue;
			if (PACKAGES_EQUAL(i, j)) v++;
		}
		if (v > max_v) {
			max_i = i;
			max_v = v;
		}
	}
	//Mark bad packages
	int g = 0;
	for (i = 0; i < packets_i; i++) {
		if (PACKAGES_EQUAL(i, max_i)) g++;
		else badData[i] = true;
	}
	tfaGoodPackages = g;
	tfaNumPackages = packets_i;

	if (max_v >= 3) {
		memcpy(dataOk, data[max_i], sizeof(data[max_i]));
	
		level = (max_v - 2);
		if (level > 5) level = 5;
		if (tfaSearchSensor) {
			//Store sensorId
			sensorId = dataOk[0];
			//tfaSearchSensor = false;
		}
	} else {
		level = 0;
	}
	if (level) { 
		tfaLostUs = 0;
		tfaLevel = level;
	}	
}

int tfaTemperature = 0; // t deg C * 10
int tfaHumidity = 0; // rel Humidity %
char tfaBatteryGood = false; // 1 - Battery Norm, 0 - Battery Low

static void readData(void) {
	//All bits counts from 0 to 39, -> left to right
	if (tfaLevel) {
		// Temperature: begin from 12 bit (from 0 - left) 3 digits 12 bits ABC->dec / 10 - 50 deg C
		tfaTemperature = (((dataOk[1] << 8) | dataOk[2]) & 0xfff) - 500; 
		//Rel Humidity: begin from 24 bit 2 digits 8 bits AB->dec %
		tfaHumidity = dataOk[3] & 0xff;
		//Battery status: 8 bit 0 - normal / 1 - low
		tfaBatteryGood = ((dataOk[1] & 0x80) == 0);
	}	
}

static int calcTreshold(int tr0) {
	int i, d, t0 = 0, n0 = 0, t1 = 0, n1 = 0, res = 0;
	for (i = 0; i < MEASURE_LEN; i++) {
		d = measure[i];
		if (d < tr0) {
			t0 += d;
			n0++;
		} else {
			t1 += d;
			n1++;
		}
	}
	if ((n0 > 0) && (n1 > 0))	{
		t0 = t0 / n0;
		t1 = t1 / n1;
		res = (t0 + t1) / 2;
	} else {
		res = (t0 + t1) / MEASURE_LEN;
	}
	return res;
}

static int packetLens[PACKETS_NUM];

static void firstPacket(void) {
			memset(data, 0, sizeof(data));
			memset(measure, 0, sizeof(measure));
			memset(badData, 0, sizeof(badData));
			memset(packetLens, 0, sizeof(packetLens));
			packets_i = 0;
}	

#define PREV_SYNC 50000
#define SYNC_NUM 2
static int prevSync = 0;

void tfaTask(void) {
	int i;
	static int sync_i = 0;
	char level = pinRead(tfaPin);
	if ((level && !level0) || ((pause >= MAX_LEN_US) && (pause0 < MAX_LEN_US))) {	//Front
		period = pulse + pause;
		if (period == 0) duty = 0;
    else duty = 1000 * pulse / period;

		if (((period > 1050) && (period <= 2000)) || ((pulse >= 550) && (pulse <= 1100) && (pause >= MAX_LEN_US))) {	//Sync
			if (sync_i < SYNC_NUM)	{
				sync_i++;
			}	
			if ((state != ST_SYNC) && ((sync_i >= SYNC_NUM) || ((sync_i >=2) && (pause >= MAX_LEN_US)))) {
				state = ST_SYNC;
				if (prevSync >= PREV_SYNC) {
					//First Packet
					firstPacket();
				} else {
						//Next packet
						if (data_i > 30) {	
							if (packets_i < PACKETS_NUM) {
								packetLens[packets_i] = data_i;
								if (data_i < 35) badData[packets_i] = true;
								packets_i++;
							}
						}	
						if ((packets_i >= PACKETS_NUM) || ((sync_i >=2) && (pause >= MAX_LEN_US))) {
							//End data
							if (packets_i >= PACKETS_NUM_MIN) {
								comparePackets();
								readData();
							}
							state = ST_NOSIGNAL;
						}
				}
				prevSync = 0;
			}
		} else if ((period >= 150) && (period <= 1050)) {	//Data
			if (state == ST_SYNC) {
				state = ST_HEADER;
				data_i = 0;
			}
			if (state == ST_HEADER) {
				if (data_i < MEASURE_LEN) {
					measure[data_i] = duty;
					data_i++;
				} else {
					bitTreshold = calcTreshold(calcTreshold(calcTreshold(calcTreshold(0))));
					char dat = 0;
					int mask = 0x80;
					for (i = 0; i < 8; i++) {
						if (measure[i] >= bitTreshold) dat |= mask;
						mask >>= 1;
					}
					if (tfaSearchSensor || (dat == sensorId)) {
						data[packets_i][0] = dat;
						mask = 0x80;
						for (i = 0; i < 8; i++) {
							SET_DATA(packets_i, i, (dat & mask) != 0);
							mask >>= 1;
						}
						data_i = MEASURE_LEN;
						state = ST_DATA;
					} else {
						state = ST_NOSIGNAL;
					}
				}
			}
			if (state == ST_DATA) {
					if (data_i < PACKET_LEN) {
						SET_DATA(packets_i, data_i, duty >= bitTreshold);
						data_i++;
					}
 			}
			sync_i = 0;
		}	else {
			if ((prevSync >= PREV_SYNC) && (state != ST_NOSIGNAL)) {
				state = ST_NOSIGNAL;
			}
			sync_i = 0;
		}
		pulse = 0;
		pause0 = pause;
	}
	if (!level && level0) { //Fall
		pause = 0;
	}
	if (level) {
		if (pulse < MAX_LEN_US) pulse += TFA_TASK_US;
	} else {
		if (pause < MAX_LEN_US) pause += TFA_TASK_US;
	}
	if (prevSync < PREV_SYNC) prevSync += TFA_TASK_US;
	
	if (tfaLostUs < TFA_LOST_US) tfaLostUs += TFA_TASK_US;
	if (tfaLostUs >= TFA_LEVEL_LOST_US) {
		tfaLevel = 0;
		if (tfaLostUs >= TFA_LOST_US) {
			tfaSearchSensor = true;
			tfaTemperature = 0;
			tfaHumidity = 0;
			tfaBatteryGood = true;
		}
	}	
	level0 = level;
}
