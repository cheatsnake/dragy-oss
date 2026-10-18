#ifndef GPS_H
#define GPS_H

#include <stddef.h>

void initGps();
void feedGps();
void setGpsStandby();
bool isFixValid();
float getSpeedKmh();
int getSatelliteCount();
bool getSpeedCrossingMillis(float targetKmh, unsigned long &crossingMillis);
bool getSpeedAtMillis(unsigned long timestampMillis, float &speedKmh);
bool getOldestSpeedKmh(float &speedKmh);
bool getGpsDateTime(char *buffer, size_t bufferSize);

#endif
