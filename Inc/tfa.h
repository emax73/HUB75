// TFA 433 MHz Temperature/Humidity Sensor 30.3206.02
#ifndef __TFA_H
#define __TFA_H

#define TFA_TASK_US 50 // 20 kHz

extern int tfaLevel; // 0 - Lost, 1 - Weak, 5 - Exellent
extern int tfaTemperature; // t deg C * 10
extern int tfaHumidity; // rel Humidity %
extern char tfaBatteryGood; // 1 - Battery Norm, 0 - Battery Low
extern char tfaSearchSensor; // If True search for sensor
extern int tfaGoodPackages; // Number of Good Packages 0..12
extern int tfaNumPackages; // Overal number of Packages 0..12

void tfaInit(void);
void tfaTask(void);

#endif
