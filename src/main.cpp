#include <Arduino.h>
#include <Wire.h>

#include "accelerometer.h"
#include "config.h"
#include "gps.h"
#include "runrecorder.h"
#include "server.h"
#include "storage.h"
#include "wifi.h"

enum Stage {
  STAGE_SEARCHING_GPS,
  STAGE_CALIBRATING_FIRST,
  STAGE_READY,
  STAGE_BENCH_ARMING,
  STAGE_BENCH_RUNNING
};

static Stage currentStage = STAGE_SEARCHING_GPS;
static BenchMode currentBenchMode = BENCH_MODE_0_100;

static unsigned long benchStartMillis = 0;

static float speedAtArmKmh = 0.0f;
static float peakSpeedKmh = 0.0f;
static float stallCheckStartSpeedKmh = 0.0f;
static unsigned long armStartMillis = 0;
static unsigned long stallCheckStartMillis = 0;

static bool carMovedSinceLastCalibration = false;
static bool carStationary = false;
static unsigned long carStationarySinceMillis = 0;

static float lastResultSeconds = 0.0f;
static bool hasLastResult = false;
static unsigned long lastUiPublishMillis = 0;

static const char *stageName(Stage stage) {
  switch (stage) {
    case STAGE_SEARCHING_GPS:
      return "SEARCHING_GPS";
    case STAGE_CALIBRATING_FIRST:
      return "CALIBRATING_FIRST";
    case STAGE_READY:
      return "READY";
    case STAGE_BENCH_ARMING:
      return "BENCH_ARMING";
    case STAGE_BENCH_RUNNING:
      return "BENCH_RUNNING";
  }

  return "UNKNOWN";
}

static void setStage(Stage stage) {
  if (stage == currentStage) {
    return;
  }

  Serial.print(F("Stage: "));
  Serial.println(stageName(stage));
  currentStage = stage;
}

static void startBenchRun(unsigned long launchStartMillis) {
  benchStartMillis = launchStartMillis;
  speedAtArmKmh = getSpeedKmh();
  peakSpeedKmh = speedAtArmKmh;
  stallCheckStartSpeedKmh = speedAtArmKmh;
  armStartMillis = millis();
  stallCheckStartMillis = armStartMillis;
  abortCalibration();
  notifyLaunchDetected();
  startRunRecording(launchStartMillis);
  setStage(STAGE_BENCH_ARMING);
}

static void finishBenchRun(unsigned long now) {
  unsigned long crossingMillis = 0;
  float resultSeconds = 0.0f;

  int targetSpeedKmh = benchModeTargetSpeedKmh(currentBenchMode);
  if (getSpeedCrossingMillis(targetSpeedKmh, crossingMillis) && crossingMillis > benchStartMillis) {
    resultSeconds = (crossingMillis - benchStartMillis) / 1000.0f;
  } else {
    resultSeconds = (now - benchStartMillis) / 1000.0f;
  }

  Serial.print(F("Run finished ("));
  Serial.print(benchModeName(currentBenchMode));
  Serial.print(F("): "));
  Serial.print(resultSeconds, 2);
  Serial.println(F(" s"));

  RunMeta meta;
  if (finishRunRecording(resultSeconds, benchModeName(currentBenchMode), meta)) {
    lastResultSeconds = meta.resultSeconds;
    hasLastResult = true;
    publishRunResult(meta);
  }

  setStage(STAGE_READY);
}

static void resetBenchRun() {
  abortRunRecording();
  setStage(STAGE_READY);
}

static void runSearchingStage() {
  if (!isFixValid()) {
    return;
  }
  startCalibration();
  setStage(STAGE_CALIBRATING_FIRST);
}

static void runFirstCalibrationStage() {
  if (isCalibrationInProgress()) {
    return;
  }
  setStage(STAGE_READY);
}

static void runReadyStage() {
  float speedKmh = getSpeedKmh();

  if (isCalibrationInProgress() && speedKmh > MOVING_SPEED_KMH) {
    abortCalibration();
  }

  if (speedKmh > MOVING_SPEED_KMH) {
    carMovedSinceLastCalibration = true;
    carStationary = false;
  } else if (!carStationary) {
    carStationary = true;
    carStationarySinceMillis = millis();
  }

  if (carMovedSinceLastCalibration && carStationary &&
      millis() - carStationarySinceMillis >= RECALIBRATION_SETTLE_TIME_MS) {
    startCalibration();
    carMovedSinceLastCalibration = false;
  }

  if (!benchModeMeasures(currentBenchMode)) {
    return;
  }

  unsigned long launchStartMillis = 0;
  if (!detectLaunchStart(launchStartMillis)) {
    return;
  }

  float speedAtLaunchKmh = 0.0f;
  bool speedAtLaunchKnown = getSpeedAtMillis(launchStartMillis, speedAtLaunchKmh);
  if (!speedAtLaunchKnown) {
    speedAtLaunchKnown = getOldestSpeedKmh(speedAtLaunchKmh);
  }
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

  if (speedKmh >= benchModeTargetSpeedKmh(currentBenchMode)) {
    finishBenchRun(now);
    return;
  }

  if (speedKmh - speedAtArmKmh >= LAUNCH_CONFIRM_SPEED_DELTA_KMH) {
    stallCheckStartMillis = now;
    stallCheckStartSpeedKmh = speedKmh;
    setStage(STAGE_BENCH_RUNNING);
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

  if (speedKmh >= benchModeTargetSpeedKmh(currentBenchMode)) {
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

static void publishCurrentUiState(unsigned long now) {
  UiState state;
  state.stage = stageName(currentStage);
  state.mode = benchModeName(currentBenchMode);
  state.speedKmh = getSpeedKmh();
  state.satellites = getSatelliteCount();
  state.calibrating = isCalibrationInProgress();
  state.elapsedSeconds = (currentStage == STAGE_BENCH_ARMING || currentStage == STAGE_BENCH_RUNNING)
                             ? (now - benchStartMillis) / 1000.0f
                             : 0.0f;
  state.lastResultSeconds = lastResultSeconds;
  state.hasLastResult = hasLastResult;
  publishUiState(state);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  initAccelerometer();
  initGps();
  initStorage();

  char storedMode[16];
  if (loadStoredMode(storedMode, sizeof(storedMode))) {
    int storedModeIndex = benchModeIndexFromName(storedMode);
    if (storedModeIndex >= 0) {
      currentBenchMode = (BenchMode)storedModeIndex;
    }
  }

  float storedResultSeconds = 0.0f;
  if (getLatestRunResult(storedResultSeconds)) {
    lastResultSeconds = storedResultSeconds;
    hasLastResult = true;
  }

  initWifi();
  initWebServer();

  Serial.println(F("Dragy started"));
}

void loop() {
  feedGps();
  updateAccelerometer();

  if (isRecalibrationRequested()) {
    clearRecalibrationRequest();
    Serial.println(F("Recalibration requested"));
    startCalibration();
  }

  int requestedModeIndex = takeModeChangeRequest();
  if (requestedModeIndex >= 0 && requestedModeIndex != (int)currentBenchMode) {
    if (currentStage == STAGE_BENCH_ARMING || currentStage == STAGE_BENCH_RUNNING) {
      resetBenchRun();
    }
    currentBenchMode = (BenchMode)requestedModeIndex;
    saveStoredMode(benchModeName(currentBenchMode));
    Serial.print(F("Mode: "));
    Serial.println(benchModeName(currentBenchMode));
  }

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
  }

  updateRunRecording();

  unsigned long now = millis();
  if (now - lastUiPublishMillis >= UI_STATE_INTERVAL_MS) {
    lastUiPublishMillis = now;
    publishCurrentUiState(now);
  }
}
