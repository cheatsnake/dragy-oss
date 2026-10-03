#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

void initAccelerometer();
void startCalibration();
bool isCalibrationInProgress();
void abortCalibration();
void updateAccelerometer();
bool isMotionActive();
void notifyLaunchDetected();
bool detectLaunchStart(unsigned long &launchStartMillis);
float getFilteredAccelerationG();
bool getAccelerationAtMillis(unsigned long timestampMillis, float &accelerationG);

#endif
