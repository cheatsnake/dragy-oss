#include "gps.h"

#include <Arduino.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>

#include "config.h"

static HardwareSerial gpsSerial(1);
static TinyGPSPlus gpsParser;

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
  }
}

bool isFixValid() {
  return gpsParser.location.isValid();
}

int getSpeedKmh() {
  return (int)gpsParser.speed.kmph();
}

int getSatelliteCount() {
  return (int)gpsParser.satellites.value();
}
