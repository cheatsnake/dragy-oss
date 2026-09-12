#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

void initAccelerometer();
void startCalibration();
bool isCalibrationInProgress();
void abortCalibration();
void updateAccelerometer();
bool isMotionActive();
bool detectLaunchStart(unsigned long &launchStartMillis);

#endif
