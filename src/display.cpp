#include "display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

#include "config.h"

static Adafruit_SSD1306 display(SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX, &Wire, -1);

static int textSizeFittingOneLine(const char *message) {
  int messageLength = (int)strlen(message);
  if (messageLength == 0) {
    return SMALL_TEXT_SIZE;
  }

  int textSize = SCREEN_WIDTH_PX / (FONT_CHAR_WIDTH_PX * messageLength);
  if (textSize < SMALL_TEXT_SIZE) {
    textSize = SMALL_TEXT_SIZE;
  }

  int maxTextSizeByHeight = SCREEN_HEIGHT_PX / FONT_CHAR_HEIGHT_PX;
  if (textSize > maxTextSizeByHeight) {
    textSize = maxTextSizeByHeight;
  }

  return textSize;
}

static void drawSatelliteCountTopLeft(int satelliteCount) {
  display.setTextSize(SMALL_TEXT_SIZE);
  display.setCursor(0, 0);
  display.print(satelliteCount);
}

static void drawCalibrationDotTopRight(bool dotOn) {
  if (!dotOn) {
    return;
  }

  display.fillCircle(SCREEN_WIDTH_PX - CALIBRATION_DOT_MARGIN_PX, CALIBRATION_DOT_MARGIN_PX,
                     CALIBRATION_DOT_RADIUS_PX, SSD1306_WHITE);
}

static void drawBigCenteredValue(const char *text) {
  int textWidthPx = (int)strlen(text) * FONT_CHAR_WIDTH_PX * BIG_TEXT_SIZE;
  int textHeightPx = FONT_CHAR_HEIGHT_PX * BIG_TEXT_SIZE;

  display.setTextSize(BIG_TEXT_SIZE);
  display.setCursor((SCREEN_WIDTH_PX - textWidthPx) / 2, (SCREEN_HEIGHT_PX - textHeightPx) / 2);
  display.print(text);
}

static void drawSmallValueBottomRight(const char *text) {
  int textWidthPx = (int)strlen(text) * FONT_CHAR_WIDTH_PX * SMALL_TEXT_SIZE;

  display.setTextSize(SMALL_TEXT_SIZE);
  display.setCursor(SCREEN_WIDTH_PX - textWidthPx, SCREEN_HEIGHT_PX - FONT_CHAR_HEIGHT_PX);
  display.print(text);
}

bool initDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS, true, false)) {
    return false;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.display();
  
  return true;
}

void showMessage(const char *message) {
  int textSize = textSizeFittingOneLine(message);
  int textWidthPx = (int)strlen(message) * FONT_CHAR_WIDTH_PX * textSize;
  int textHeightPx = FONT_CHAR_HEIGHT_PX * textSize;

  display.clearDisplay();
  display.setTextSize(textSize);
  display.setCursor((SCREEN_WIDTH_PX - textWidthPx) / 2, (SCREEN_HEIGHT_PX - textHeightPx) / 2);
  display.print(message);
  display.display();
}

void showSpeedView(int speedKmh, int satelliteCount, bool calibrationDotOn) {
  char speedText[8];
  snprintf(speedText, sizeof(speedText), "%d", speedKmh);

  display.clearDisplay();
  drawSatelliteCountTopLeft(satelliteCount);
  drawCalibrationDotTopRight(calibrationDotOn);
  drawBigCenteredValue(speedText);
  display.display();
}

void showBenchView(int speedKmh, int satelliteCount, float elapsedSeconds) {
  char speedText[8];
  char timerText[8];
  snprintf(speedText, sizeof(speedText), "%d", speedKmh);
  snprintf(timerText, sizeof(timerText), "%.1f", elapsedSeconds);

  display.clearDisplay();
  drawSatelliteCountTopLeft(satelliteCount);
  drawBigCenteredValue(speedText);
  drawSmallValueBottomRight(timerText);
  display.display();
}

void showResultView(float finalSeconds, int speedKmh, int satelliteCount) {
  char timeText[8];
  char speedText[8];
  snprintf(timeText, sizeof(timeText), "%.1f", finalSeconds);
  snprintf(speedText, sizeof(speedText), "%d", speedKmh);

  display.clearDisplay();
  drawSatelliteCountTopLeft(satelliteCount);
  drawBigCenteredValue(timeText);
  drawSmallValueBottomRight(speedText);
  display.display();
}
