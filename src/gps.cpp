#include "gps.h"

#include <Arduino.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>

#include "config.h"

struct SpeedSample {
  unsigned long timestampMillis;
  float speedKmh;
};

static HardwareSerial gpsSerial(1);
static TinyGPSPlus gpsParser;
static SpeedSample speedHistory[SPEED_HISTORY_SIZE];
static int speedHistoryCount = 0;
static int speedHistoryIndex = 0;

static SpeedSample speedHistoryAt(int index) {
  int oldestIndex = (speedHistoryCount < SPEED_HISTORY_SIZE) ? 0 : speedHistoryIndex;
  return speedHistory[(oldestIndex + index) % SPEED_HISTORY_SIZE];
}

static void pushSpeedSample(unsigned long timestampMillis, float speedKmh) {
  speedHistory[speedHistoryIndex] = {timestampMillis, speedKmh};
  speedHistoryIndex = (speedHistoryIndex + 1) % SPEED_HISTORY_SIZE;
  
  if (speedHistoryCount < SPEED_HISTORY_SIZE) {
    speedHistoryCount++;
  }
}

void initGps() {
  gpsSerial.begin(GPS_BAUD_INITIAL, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  delay(100);

  gpsSerial.print(GPS_COMMAND_SET_BAUD_115200);
  delay(200);

  gpsSerial.updateBaudRate(GPS_BAUD_FINAL);
  delay(100);

  gpsSerial.print(GPS_COMMAND_SET_UPDATE_RATE_10HZ);
  gpsSerial.print(GPS_COMMAND_SET_CONSTELLATIONS_GPS_BDS);
  gpsSerial.print(GPS_COMMAND_SAVE_SETTINGS);
}

void feedGps() {
  while (gpsSerial.available() > 0) {
    gpsParser.encode(gpsSerial.read());

    if (gpsParser.speed.isUpdated() && gpsParser.location.isValid()) {
      pushSpeedSample(millis(), (float)gpsParser.speed.kmph());
    }
  }
}

bool isFixValid() {
  return gpsParser.location.isValid();
}

float getSpeedKmh() {
  return (float)gpsParser.speed.kmph();
}

int getSatelliteCount() {
  return (int)gpsParser.satellites.value();
}

bool getSpeedCrossingMillis(float targetKmh, unsigned long &crossingMillis) {
  for (int i = 1; i < speedHistoryCount; i++) {
    SpeedSample previous = speedHistoryAt(i - 1);
    SpeedSample current = speedHistoryAt(i);

    if (previous.speedKmh < targetKmh && current.speedKmh >= targetKmh) {
      float speedSpan = current.speedKmh - previous.speedKmh;

      if (speedSpan <= 0.0f) {
        crossingMillis = current.timestampMillis;
      } else {
        float crossedFraction = (targetKmh - previous.speedKmh) / speedSpan;
        crossingMillis = previous.timestampMillis +
                         (unsigned long)(crossedFraction *
                                         (float)(current.timestampMillis - previous.timestampMillis));
      }
      return true;
    }
  }

  return false;
}

bool getSpeedAtMillis(unsigned long timestampMillis, float &speedKmh) {
  for (int i = speedHistoryCount - 1; i >= 0; i--) {
    SpeedSample sample = speedHistoryAt(i);

    if (sample.timestampMillis <= timestampMillis) {
      speedKmh = sample.speedKmh;
      return true;
    }
  }

  return false;
}

bool getOldestSpeedKmh(float &speedKmh) {
  if (speedHistoryCount == 0) {
    return false;
  }

  speedKmh = speedHistoryAt(0).speedKmh;
  return true;
}

bool getGpsDateTime(char *buffer, size_t bufferSize) {
  if (!gpsParser.date.isValid() || !gpsParser.time.isValid()) {
    return false;
  }

  snprintf(buffer, bufferSize, "%04d-%02d-%02d %02d:%02d:%02d",
           gpsParser.date.year(), gpsParser.date.month(), gpsParser.date.day(),
           gpsParser.time.hour(), gpsParser.time.minute(), gpsParser.time.second());
  return true;
}
