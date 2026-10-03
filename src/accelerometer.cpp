#include "accelerometer.h"

#include <Arduino.h>
#include <math.h>
#include <Wire.h>

#include "config.h"

struct AccelerometerSample {
  unsigned long timestampMillis;
  float horizontalG;
};

static AccelerometerSample sampleHistory[ACCELEROMETER_HISTORY_SIZE];
static int sampleHistoryCount = 0;
static int sampleHistoryIndex = 0;

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

static AccelerometerSample sampleHistoryAt(int index) {
  int oldestIndex = (sampleHistoryCount < ACCELEROMETER_HISTORY_SIZE) ? 0 : sampleHistoryIndex;

  return sampleHistory[(oldestIndex + index) % ACCELEROMETER_HISTORY_SIZE];
}

static void pushSampleHistory(unsigned long timestampMillis, float horizontalG) {
  sampleHistory[sampleHistoryIndex] = {timestampMillis, horizontalG};
  sampleHistoryIndex = (sampleHistoryIndex + 1) % ACCELEROMETER_HISTORY_SIZE;

  if (sampleHistoryCount < ACCELEROMETER_HISTORY_SIZE) {
    sampleHistoryCount++;
  }
}

static void clearSampleHistory() {
  sampleHistoryCount = 0;
  sampleHistoryIndex = 0;
}

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

  bool biasChangedMaterially = fabsf(biasXG - averageXG) > HISTORY_RESET_BIAS_CHANGE_G ||
                               fabsf(biasYG - averageYG) > HISTORY_RESET_BIAS_CHANGE_G ||
                               fabsf(biasZG - averageZG) > HISTORY_RESET_BIAS_CHANGE_G;

  biasXG = averageXG;
  biasYG = averageYG;
  biasZG = averageZG;

  calibrationActive = false;

  if (biasChangedMaterially) {
    clearSampleHistory();
    filteredHorizontalAccelerationG = 0.0f;
    motionActive = false;
  }
}

static bool calibrationMovedTooMuch(float gX, float gY, float gZ) {
  float averageXG = (calibrationSumRawX / (float)calibrationSamplesCollected) * ACCELERATION_SCALE_G_PER_BIT;
  float averageYG = (calibrationSumRawY / (float)calibrationSamplesCollected) * ACCELERATION_SCALE_G_PER_BIT;
  float averageZG = (calibrationSumRawZ / (float)calibrationSamplesCollected) * ACCELERATION_SCALE_G_PER_BIT;

  return fabsf(gX - averageXG) > CALIBRATION_ABORT_DEVIATION_G ||
         fabsf(gY - averageYG) > CALIBRATION_ABORT_DEVIATION_G ||
         fabsf(gZ - averageZG) > CALIBRATION_ABORT_DEVIATION_G;
}

static void collectCalibrationSample() {
  int16_t rawX = 0;
  int16_t rawY = 0;
  int16_t rawZ = 0;

  if (!readRawAcceleration(rawX, rawY, rawZ)) {
    return;
  }

  float gX = rawX * ACCELERATION_SCALE_G_PER_BIT;
  float gY = rawY * ACCELERATION_SCALE_G_PER_BIT;
  float gZ = rawZ * ACCELERATION_SCALE_G_PER_BIT;

  if (calibrationSamplesCollected > 0 && calibrationMovedTooMuch(gX, gY, gZ)) {
    abortCalibration();
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
  pushSampleHistory(millis(), horizontalG);

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

void abortCalibration() {
  calibrationActive = false;
}

bool isMotionActive() {
  return motionActive;
}

void notifyLaunchDetected() {
  motionActive = true;
}

float getFilteredAccelerationG() {
  return filteredHorizontalAccelerationG;
}

bool getAccelerationAtMillis(unsigned long timestampMillis, float &accelerationG) {
  for (int i = sampleHistoryCount - 1; i >= 0; i--) {
    AccelerometerSample sample = sampleHistoryAt(i);

    if (sample.timestampMillis <= timestampMillis) {
      accelerationG = sample.horizontalG;
      return true;
    }
  }

  return false;
}

bool detectLaunchStart(unsigned long &launchStartMillis) {
  if (sampleHistoryCount < LAUNCH_CONFIRM_SAMPLES) {
    return false;
  }

  for (int i = 1; i <= LAUNCH_CONFIRM_SAMPLES; i++) {
    if (sampleHistoryAt(sampleHistoryCount - i).horizontalG <= MOTION_START_THRESHOLD_G) {
      return false;
    }
  }

  int risingStartIndex = sampleHistoryCount - LAUNCH_CONFIRM_SAMPLES;
  int launchIndex = risingStartIndex;

  while (launchIndex > 0 && sampleHistoryAt(launchIndex - 1).horizontalG >= MOTION_STOP_THRESHOLD_G) {
    launchIndex--;
  }

  launchStartMillis = sampleHistoryAt(launchIndex).timestampMillis;
  return true;
}

void updateAccelerometer() {
  unsigned long now = millis();

  if (calibrationActive && now - lastCalibrationSampleMillis >= CALIBRATION_SAMPLE_INTERVAL_MS) {
    lastCalibrationSampleMillis = now;
    collectCalibrationSample();
  }

  if (now - lastSampleMillis < ACCELEROMETER_INTERVAL_MS) {
    return;
  }
  lastSampleMillis = now;
  sampleAndFilterAcceleration();
}
