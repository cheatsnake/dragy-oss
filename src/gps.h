#ifndef GPS_H
#define GPS_H

void initGps();
void feedGps();
bool isFixValid();
float getSpeedKmh();
int getSatelliteCount();
bool getSpeedCrossingMillis(float targetKmh, unsigned long &crossingMillis);
bool getSpeedAtMillis(unsigned long timestampMillis, float &speedKmh);

#endif
