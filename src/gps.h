#ifndef GPS_H
#define GPS_H

void initGps();
void feedGps();
bool isFixValid();
int getSpeedKmh();
int getSatelliteCount();

#endif
