#include "wifi.h"

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"

void initWifi() {
  IPAddress apIp(192, 168, 4, 1);
  IPAddress apGateway(192, 168, 4, 1);
  IPAddress apSubnet(255, 255, 255, 0);

  WiFi.mode(WIFI_AP);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.softAPConfig(apIp, apGateway, apSubnet);

  bool started = false;
  for (int attempt = 1; attempt <= 3 && !started; attempt++) {
    started = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
    if (!started) {
      Serial.print(F("WiFi softAP attempt "));
      Serial.print(attempt);
      Serial.println(F(" failed"));
      delay(500);
    }
  }

  Serial.print(F("WiFi AP "));
  Serial.print(started ? F("started: ") : F("FAILED: "));
  Serial.println(WIFI_AP_SSID);
  Serial.print(F("AP address: http://"));
  Serial.println(WiFi.softAPIP());
  Serial.print(F("AP channel: "));
  Serial.println(WiFi.channel());
}
