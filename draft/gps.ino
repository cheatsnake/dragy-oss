#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

// ---------- Display settings ----------
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS  0x3C

// I2C pins for OLED (ESP32-C3 SuperMini)
#define OLED_SDA 1
#define OLED_SCL 0

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------- GPS settings ----------
#define GPS_RX_PIN 4
#define GPS_TX_PIN 3
#define GPS_BAUD_1  9600
#define GPS_BAUD_2  115200
HardwareSerial gpsSerial(1);  // UART1

TinyGPSPlus gps;              // NMEA parsing object

// ---------- Speaker (optional) ----------
#define SPEAKER_PIN 10

// ---------- Timing variables ----------
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_INTERVAL = 200;

uint8_t isGpsReady = 0;
unsigned long lastDiagPrint = 0;

void readyNotify() {
  if (isGpsReady == 1) return;
  tone(SPEAKER_PIN, 700, 700);
  isGpsReady = 1;
}

void initGps() {
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  delay(100);

  gpsSerial.print(F("$PCAS01,5*19\r\n"));  // 115200 baud
  delay(200);
  gpsSerial.updateBaudRate(115200);
  delay(100);

  // 10 Hz + GPS+BDS
  gpsSerial.print("$PCAS02,100*1E\r\n");
  gpsSerial.print("$PCAS04,3*1A\r\n");
  gpsSerial.print("$PCAS00*01\r\n");

  Serial.println(F("GPS @ 115200 baud"));
}

void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA, OLED_SCL);

  tone(SPEAKER_PIN, 300, 500);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 not found!"));
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("GPS Diag Mode"));
  display.display();

  initGps();
  Serial.println(F("Waiting for GPS data..."));
}

void loop() {
  // ----- Read GPS data, parse + mirror to Serial -----
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    gps.encode(c);
    Serial.write(c);                 // mirror: see raw NMEA sentences
  }

  // ----- Update the screen on a timer -----
  if (millis() - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    lastDisplayUpdate = millis();

    display.clearDisplay();
    display.setCursor(0, 0);

    if (gps.location.isValid()) {
      readyNotify();
      display.setTextSize(4.5);
      display.println(gps.speed.kmph(), 0);
      display.setTextSize(1);
      display.println(gps.location.lat(), 6);
      display.println(gps.location.lng(), 6);
      display.println(gps.altitude.meters(), 1);
      display.print(F("Sat: "));
      display.println(gps.satellites.value());
    } else {
      display.println(F("   SEARCHING..."));
      display.print(F("Sats: "));
      display.println(gps.satellites.value());

      display.print(F("Chars: "));
      display.println(gps.charsProcessed());

      display.print(F("Fail: "));
      display.println(gps.failedChecksum());
    }

    display.display();
  }

  // ----- Periodic report to Serial (every 2 s) -----
  if (millis() - lastDiagPrint >= 2000) {
    lastDiagPrint = millis();
    Serial.println(F("--- DIAG ---"));
    Serial.print(F("chars="));
    Serial.println(gps.charsProcessed());
    Serial.print(F("sentences_ok="));
    Serial.println(gps.passedChecksum());
    Serial.print(F("sentences_fail="));
    Serial.println(gps.failedChecksum());
    Serial.print(F("sats="));
    Serial.println(gps.satellites.value());
    Serial.println(F("------------"));
  }
}
