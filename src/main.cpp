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
  STAGE_BENCH_ARMING,
  STAGE_BENCH_RUNNING,
  STAGE_SHOWING_RESULT
};

static Stage currentStage = STAGE_SEARCHING_GPS;

static unsigned long benchStartMillis = 0;
static unsigned long benchFinishMillis = 0;
static unsigned long resultStartMillis = 0;

static float speedAtArmKmh = 0.0f;
static float peakSpeedKmh = 0.0f;
static float stallCheckStartSpeedKmh = 0.0f;
static unsigned long armStartMillis = 0;
static unsigned long stallCheckStartMillis = 0;

static unsigned long lastDisplayUpdateMillis = 0;
static unsigned long lastCalibrationMillis = 0;
static bool calibrationDotOn = false;

static void startBenchRun(unsigned long launchStartMillis) {
  benchStartMillis = launchStartMillis;
  speedAtArmKmh = getSpeedKmh();
  peakSpeedKmh = speedAtArmKmh;
  stallCheckStartSpeedKmh = speedAtArmKmh;
  armStartMillis = millis();
  stallCheckStartMillis = armStartMillis;
  currentStage = STAGE_BENCH_ARMING;
}

static void finishBenchRun(unsigned long now) {
  unsigned long crossingMillis = 0;
  if (getSpeedCrossingMillis(TARGET_SPEED_KMH, crossingMillis) && crossingMillis > benchStartMillis) {
    benchFinishMillis = crossingMillis;
  } else {
    benchFinishMillis = now;
  }
  resultStartMillis = now;
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
  float speedKmh = getSpeedKmh();

  if (speedKmh <= RECALIBRATION_MAX_SPEED_KMH && !isCalibrationInProgress() &&
      millis() - lastCalibrationMillis >= RECALIBRATION_INTERVAL_MS) {
    startCalibration();
    lastCalibrationMillis = millis();
  }

  unsigned long launchStartMillis = 0;
  if (!detectLaunchStart(launchStartMillis)) {
    return;
  }

  float speedAtLaunchKmh = 0.0f;
  bool speedAtLaunchKnown = getSpeedAtMillis(launchStartMillis, speedAtLaunchKmh);
  if (speedAtLaunchKnown && speedAtLaunchKmh > LAUNCH_MAX_START_SPEED_KMH) {
    return;
  }

  startBenchRun(launchStartMillis);
}

static void runBenchArmingStage() {
  unsigned long now = millis();
  float speedKmh = getSpeedKmh();

  if (speedKmh > peakSpeedKmh) {
    peakSpeedKmh = speedKmh;
  }

  if (speedKmh >= TARGET_SPEED_KMH) {
    finishBenchRun(now);
    return;
  }

  if (speedKmh - speedAtArmKmh >= LAUNCH_CONFIRM_SPEED_DELTA_KMH) {
    stallCheckStartMillis = now;
    stallCheckStartSpeedKmh = speedKmh;
    currentStage = STAGE_BENCH_RUNNING;
    return;
  }

  if (now - armStartMillis >= LAUNCH_CONFIRM_WINDOW_MS) {
    resetBenchRun();
    return;
  }

  if (!isMotionActive()) {
    resetBenchRun();
  }
}

static void runBenchRunningStage() {
  unsigned long now = millis();
  float speedKmh = getSpeedKmh();

  if (speedKmh >= TARGET_SPEED_KMH) {
    finishBenchRun(now);
    return;
  }

  if (speedKmh > peakSpeedKmh) {
    peakSpeedKmh = speedKmh;
  }

  if (now - stallCheckStartMillis >= BENCH_STALL_TIMEOUT_MS) {
    if (speedKmh - stallCheckStartSpeedKmh >= BENCH_MIN_PROGRESS_KMH) {
      stallCheckStartMillis = now;
      stallCheckStartSpeedKmh = speedKmh;
    } else {
      resetBenchRun();
      return;
    }
  }

  if (speedKmh < peakSpeedKmh - BENCH_ABORT_SPEED_DROP_KMH) {
    resetBenchRun();
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

  int speedKmh = (int)getSpeedKmh();
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
    case STAGE_BENCH_ARMING:
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
    case STAGE_BENCH_ARMING:
      runBenchArmingStage();
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
