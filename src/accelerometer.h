#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

void initAccelerometer();
void startCalibration();
bool isCalibrationInProgress();
void updateAccelerometer();
bool isMotionActive();

#endif
