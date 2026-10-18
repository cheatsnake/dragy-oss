#include "sleep.h"

#include <Arduino.h>
#include <esp_sleep.h>

#include "accelerometer.h"
#include "config.h"
#include "gps.h"
#include "server.h"
#include "wifi.h"

bool wokeByMotion() {
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO;
}

void enterSleepMode() {
  Serial.println(F("Sleeping"));
  publishSleepNotice();
  delay(SLEEP_SHUTDOWN_FLUSH_MS);

  stopWebServer();
  stopWifi();
  setGpsStandby();
  armSleepWakeup();

  pinMode(SLEEP_WAKEUP_GPIO, INPUT);
  Serial.flush();

  esp_deep_sleep_enable_gpio_wakeup(1ULL << SLEEP_WAKEUP_GPIO, ESP_GPIO_WAKEUP_GPIO_HIGH);
  esp_deep_sleep_start();
}
