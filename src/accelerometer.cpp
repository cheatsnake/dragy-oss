#include "accelerometer.h"

#include <Arduino.h>
#include <math.h>
#include <Wire.h>

#include "config.h"

static float biasXG = 0.0f;
static float biasYG = 0.0f;
static float biasZG = 0.0f;

static int verticalAxisIndex = 2;

static float filteredHorizontalAccelerationG = 0.0f;
static bool motionActive = false;

static bool calibrationActive = false;
static int calibrationSamplesCollected = 0;
static long calibrationSumRawX = 0;
static long calibrationSumRawY = 0;
static long calibrationSumRawZ = 0;

static unsigned long lastCalibrationSampleMillis = 0;
static unsigned long lastSampleMillis = 0;

static int16_t readTwoBytesAsSigned() {
  uint8_t lowByte = Wire.read();
  uint8_t highByte = Wire.read();
  return (int16_t)(lowByte | (highByte << 8));
}

static bool readRawAcceleration(int16_t &rawX, int16_t &rawY, int16_t &rawZ) {
  Wire.beginTransmission(ACCELEROMETER_ADDRESS);
  Wire.write(ACCELEROMETER_REGISTER_DATA_START);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  Wire.requestFrom(ACCELEROMETER_ADDRESS, ACCELEROMETER_DATA_BYTES);
  if (Wire.available() < ACCELEROMETER_DATA_BYTES) {
    return false;
  }

  rawX = readTwoBytesAsSigned();
  rawY = readTwoBytesAsSigned();
  rawZ = readTwoBytesAsSigned();
  return true;
}

static void finishCalibration() {
  float averageXG = (calibrationSumRawX / (float)CALIBRATION_SAMPLE_COUNT) * ACCELERATION_SCALE_G_PER_BIT;
  float averageYG = (calibrationSumRawY / (float)CALIBRATION_SAMPLE_COUNT) * ACCELERATION_SCALE_G_PER_BIT;
  float averageZG = (calibrationSumRawZ / (float)CALIBRATION_SAMPLE_COUNT) * ACCELERATION_SCALE_G_PER_BIT;

  float absXG = fabsf(averageXG);
  float absYG = fabsf(averageYG);
  float absZG = fabsf(averageZG);

  if (absXG > absYG && absXG > absZG) {
    verticalAxisIndex = 0;
  } else if (absYG > absXG && absYG > absZG) {
    verticalAxisIndex = 1;
  } else {
    verticalAxisIndex = 2;
  }

  biasXG = averageXG;
  biasYG = averageYG;
  biasZG = averageZG;

  calibrationActive = false;
}

static void collectCalibrationSample() {
  int16_t rawX = 0;
  int16_t rawY = 0;
  int16_t rawZ = 0;
  if (!readRawAcceleration(rawX, rawY, rawZ)) {
    return;
  }

  calibrationSumRawX += rawX;
  calibrationSumRawY += rawY;
  calibrationSumRawZ += rawZ;
  calibrationSamplesCollected++;

  if (calibrationSamplesCollected >= CALIBRATION_SAMPLE_COUNT) {
    finishCalibration();
  }
}

static float computeHorizontalAccelerationG(float gX, float gY, float gZ) {
  if (verticalAxisIndex == 0) {
    return sqrtf(gY * gY + gZ * gZ);
  }
  if (verticalAxisIndex == 1) {
    return sqrtf(gX * gX + gZ * gZ);
  }
  return sqrtf(gX * gX + gY * gY);
}

static void updateMotionState() {
  if (!motionActive) {
    if (filteredHorizontalAccelerationG > MOTION_START_THRESHOLD_G) {
      motionActive = true;
    }
    return;
  }
  if (filteredHorizontalAccelerationG < MOTION_STOP_THRESHOLD_G) {
    motionActive = false;
  }
}

static void sampleAndFilterAcceleration() {
  int16_t rawX = 0;
  int16_t rawY = 0;
  int16_t rawZ = 0;
  if (!readRawAcceleration(rawX, rawY, rawZ)) {
    return;
  }

  float gX = rawX * ACCELERATION_SCALE_G_PER_BIT - biasXG;
  float gY = rawY * ACCELERATION_SCALE_G_PER_BIT - biasYG;
  float gZ = rawZ * ACCELERATION_SCALE_G_PER_BIT - biasZG;

  float horizontalG = computeHorizontalAccelerationG(gX, gY, gZ);
  filteredHorizontalAccelerationG =
      horizontalG * BUMP_FILTER_ALPHA +
      filteredHorizontalAccelerationG * (1.0f - BUMP_FILTER_ALPHA);

  updateMotionState();
}

void initAccelerometer() {
  Wire.beginTransmission(ACCELEROMETER_ADDRESS);
  Wire.write(ACCELEROMETER_REGISTER_POWER_CTL);
  Wire.write(ACCELEROMETER_MEASURE_MODE_VALUE);
  Wire.endTransmission();
  delay(ACCELEROMETER_POWER_UP_DELAY_MS);
}

void startCalibration() {
  calibrationActive = true;
  calibrationSamplesCollected = 0;
  calibrationSumRawX = 0;
  calibrationSumRawY = 0;
  calibrationSumRawZ = 0;
  lastCalibrationSampleMillis = millis();
}

bool isCalibrationInProgress() {
  return calibrationActive;
}

bool isMotionActive() {
  return motionActive;
}

void updateAccelerometer() {
  unsigned long now = millis();

  if (calibrationActive) {
    if (now - lastCalibrationSampleMillis < CALIBRATION_SAMPLE_INTERVAL_MS) {
      return;
    }
    lastCalibrationSampleMillis = now;
    collectCalibrationSample();
    return;
  }

  if (now - lastSampleMillis < ACCELEROMETER_INTERVAL_MS) {
    return;
  }
  lastSampleMillis = now;
  sampleAndFilterAcceleration();
}
