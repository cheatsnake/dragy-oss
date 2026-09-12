#include <Arduino.h>
#include <Wire.h>

#include "accelerometer.h"
#include "config.h"
#include "display.h"
#include "gps.h"
#include "speaker.h"

enum Stage {
  STAGE_SEARCHING_GPS,
  STAGE_CALIBRATING_FIRST,
  STAGE_READY,
  STAGE_BENCH_RUNNING,
  STAGE_SHOWING_RESULT
};

static Stage currentStage = STAGE_SEARCHING_GPS;

static unsigned long benchStartMillis = 0;
static unsigned long benchFinishMillis = 0;
static unsigned long resultStartMillis = 0;

static unsigned long lastDisplayUpdateMillis = 0;
static unsigned long lastCalibrationMillis = 0;
static bool calibrationDotOn = false;

static void startBenchRun() {
  benchStartMillis = millis();
  currentStage = STAGE_BENCH_RUNNING;
}

static void finishBenchRun() {
  benchFinishMillis = millis();
  resultStartMillis = benchFinishMillis;
  playSixtyReachedSignal();
  currentStage = STAGE_SHOWING_RESULT;
}

static void resetBenchRun() {
  currentStage = STAGE_READY;
}

static void runSearchingStage() {
  if (!isFixValid()) {
    return;
  }
  playSatellitesFoundSignal();
  startCalibration();
  currentStage = STAGE_CALIBRATING_FIRST;
}

static void runFirstCalibrationStage() {
  if (isCalibrationInProgress()) {
    return;
  }
  lastCalibrationMillis = millis();
  currentStage = STAGE_READY;
}

static void runReadyStage() {
  int speedKmh = getSpeedKmh();

  if (speedKmh == 0 && !isCalibrationInProgress() &&
      millis() - lastCalibrationMillis >= RECALIBRATION_INTERVAL_MS) {
    startCalibration();
    lastCalibrationMillis = millis();
  }

  if (isMotionActive() && speedKmh <= RESET_SPEED_KMH) {
    startBenchRun();
  }
}

static void runBenchRunningStage() {
  int speedKmh = getSpeedKmh();

  if (speedKmh >= TARGET_SPEED_KMH) {
    finishBenchRun();
    return;
  }

  if (speedKmh <= RESET_SPEED_KMH && !isMotionActive()) {
    resetBenchRun();
  }
}

static void runShowingResultStage() {
  if (millis() - resultStartMillis < RESULT_DISPLAY_DURATION_MS) {
    return;
  }
  resetBenchRun();
}

static void refreshDisplay() {
  unsigned long now = millis();
  if (now - lastDisplayUpdateMillis < DISPLAY_INTERVAL_MS) {
    return;
  }
  lastDisplayUpdateMillis = now;

  calibrationDotOn = !calibrationDotOn;

  int speedKmh = getSpeedKmh();
  int satelliteCount = getSatelliteCount();
  bool calibrationDotVisible = isCalibrationInProgress() && calibrationDotOn;

  switch (currentStage) {
    case STAGE_SEARCHING_GPS:
      showMessage("Searching signal...");
      break;
    case STAGE_CALIBRATING_FIRST:
      showMessage("Calibration...");
      break;
    case STAGE_READY:
      showSpeedView(speedKmh, satelliteCount, calibrationDotVisible);
      break;
    case STAGE_BENCH_RUNNING: {
      float elapsedSeconds = (now - benchStartMillis) / 1000.0f;
      showBenchView(speedKmh, satelliteCount, elapsedSeconds);
      break;
    }
    case STAGE_SHOWING_RESULT: {
      float finalSeconds = (benchFinishMillis - benchStartMillis) / 1000.0f;
      showResultView(finalSeconds, speedKmh, satelliteCount);
      break;
    }
  }
}

void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

  initSpeaker();
  initAccelerometer();

  if (!initDisplay()) {
    Serial.println(F("SSD1306 not found!"));
    while (true) {
      delay(1000);
    }
  }

  initGps();
}

void loop() {
  feedGps();
  updateAccelerometer();

  switch (currentStage) {
    case STAGE_SEARCHING_GPS:
      runSearchingStage();
      break;
    case STAGE_CALIBRATING_FIRST:
      runFirstCalibrationStage();
      break;
    case STAGE_READY:
      runReadyStage();
      break;
    case STAGE_BENCH_RUNNING:
      runBenchRunningStage();
      break;
    case STAGE_SHOWING_RESULT:
      runShowingResultStage();
      break;
  }

  refreshDisplay();
}
