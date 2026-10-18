#include "runrecorder.h"

#include <Arduino.h>

#include "accelerometer.h"
#include "config.h"
#include "gps.h"

struct RunSample {
  unsigned long tMillis;
  float speedKmh;
  float accelerationG;
};

static RunSample samples[RUN_MAX_SAMPLES];
static int sampleCount = 0;
static bool recordingActive = false;
static unsigned long launchMillis = 0;
static unsigned long nextSampleMillis = 0;

static void pushSample(unsigned long tMillis, float speedKmh, float accelerationG) {
  if (sampleCount >= RUN_MAX_SAMPLES) {
    recordingActive = false;
    Serial.println(F("Run recorder: buffer full"));
    return;
  }

  samples[sampleCount++] = {tMillis, speedKmh, accelerationG};
}

void startRunRecording(unsigned long launchStartMillis) {
  launchMillis = launchStartMillis;
  nextSampleMillis = launchStartMillis;
  sampleCount = 0;
  recordingActive = true;
}

void updateRunRecording() {
  if (!recordingActive) {
    return;
  }

  unsigned long now = millis();

  while (recordingActive && nextSampleMillis <= now) {
    float speedKmh = 0.0f;
    float accelerationG = 0.0f;
    bool haveSpeed = getSpeedAtMillis(nextSampleMillis, speedKmh);
    bool haveAcceleration = getAccelerationAtMillis(nextSampleMillis, accelerationG);

    if (haveSpeed || haveAcceleration) {
      pushSample(nextSampleMillis - launchMillis, speedKmh, accelerationG);
    }

    nextSampleMillis += RUN_SAMPLE_INTERVAL_MS;
  }
}

bool finishRunRecording(float resultSeconds, const char *mode, RunMeta &meta) {
  updateRunRecording();

  if (recordingActive) {
    unsigned long now = millis();
    float speedKmh = getSpeedKmh();
    float accelerationG = 0.0f;
    getAccelerationAtMillis(now, accelerationG);
    unsigned long tMillis = now - launchMillis;

    if (sampleCount == 0 || tMillis > samples[sampleCount - 1].tMillis) {
      pushSample(tMillis, speedKmh, accelerationG);
    }
  }

  recordingActive = false;

  int id = getNextRunId();

  char date[24];
  bool haveDate = getGpsDateTime(date, sizeof(date));

  char dateField[32];
  if (haveDate) {
    snprintf(dateField, sizeof(dateField), "\"%s\"", date);
  } else {
    strcpy(dateField, "null");
  }

  size_t capacity = 224 + (size_t)sampleCount * 48;
  char *json = (char *)malloc(capacity);
  if (json == nullptr) {
    Serial.println(F("Run recorder: out of memory"));
    sampleCount = 0;
    return false;
  }

  size_t offset = (size_t)snprintf(
      json, capacity,
      "{\"id\":%d,\"gpsDateTime\":%s,\"resultSeconds\":%.2f,\"mode\":\"%s\",\"samples\":[", id,
      dateField, resultSeconds, mode);

  for (int i = 0; i < sampleCount; i++) {
    const RunSample &sample = samples[i];
    size_t remaining = capacity - offset;
    int written = snprintf(json + offset, remaining, "%s{\"t\":%.1f,\"s\":%.1f,\"a\":%.2f}",
                           i == 0 ? "" : ",", sample.tMillis / 1000.0f, sample.speedKmh,
                           sample.accelerationG);

    if (written < 0 || (size_t)written >= remaining) {
      break;
    }
    offset += (size_t)written;
  }

  offset += (size_t)snprintf(json + offset, capacity - offset, "]}");

  int savedId = saveRun(json);
  free(json);
  sampleCount = 0;

  if (savedId < 0) {
    return false;
  }

  meta.id = savedId;
  meta.resultSeconds = resultSeconds;
  strncpy(meta.mode, mode, sizeof(meta.mode) - 1);
  meta.mode[sizeof(meta.mode) - 1] = '\0';
  if (haveDate) {
    strncpy(meta.date, date, sizeof(meta.date) - 1);
    meta.date[sizeof(meta.date) - 1] = '\0';
  } else {
    meta.date[0] = '\0';
  }

  Serial.print(F("Run saved: id "));
  Serial.println(savedId);
  return true;
}

void abortRunRecording() {
  recordingActive = false;
  sampleCount = 0;
}
