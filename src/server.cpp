#include "server.h"

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "config.h"
#include "page.h"

static AsyncWebServer server(80);
static AsyncEventSource events("/events");

static UiState latestState = {"SEARCHING_GPS", MODE_NAME_0_100, 0.0f, 0, false, 0.0f, 0.0f, false};
static volatile bool recalibrationRequested = false;
static volatile int requestedModeIndex = -1;
static unsigned long lastHeartbeatMillis = 0;

static void formatStateJson(char *buffer, size_t bufferSize) {
  char resultField[16];
  if (latestState.hasLastResult) {
    snprintf(resultField, sizeof(resultField), "%.2f", latestState.lastResultSeconds);
  } else {
    strcpy(resultField, "null");
  }

  snprintf(buffer, bufferSize,
           "{\"stage\":\"%s\",\"mode\":\"%s\",\"speedKmh\":%.1f,\"satellites\":%d,\"calibrating\":%s,"
           "\"elapsedSeconds\":%.2f,\"lastResultSeconds\":%s}",
           latestState.stage, latestState.mode, latestState.speedKmh, latestState.satellites,
           latestState.calibrating ? "true" : "false", latestState.elapsedSeconds, resultField);
}

void publishUiState(const UiState &state) {
  latestState = state;

  if (events.count() == 0) {
    return;
  }

  unsigned long now = millis();
  if (now - lastHeartbeatMillis >= SSE_HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMillis = now;
    events.send("{}", "ping");
  }

  char buffer[208];
  formatStateJson(buffer, sizeof(buffer));
  events.send(buffer, "state");
}

void publishRunResult(const RunMeta &meta) {
  char buffer[160];
  snprintf(buffer, sizeof(buffer), "{\"id\":%d,\"resultSeconds\":%.2f,\"date\":\"%s\",\"mode\":\"%s\"}",
           meta.id, meta.resultSeconds, meta.date, meta.mode);
  events.send(buffer, "result");
}

void publishSleepNotice() {
  events.send("{}", "sleep");
}

void stopWebServer() {
  server.end();
}

bool isRecalibrationRequested() {
  return recalibrationRequested;
}

void clearRecalibrationRequest() {
  recalibrationRequested = false;
}

int takeModeChangeRequest() {
  int index = requestedModeIndex;
  requestedModeIndex = -1;
  return index;
}

void initWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", INDEX_PAGE);
  });

  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest *request) {
    char buffer[208];
    formatStateJson(buffer, sizeof(buffer));
    request->send(200, "application/json", buffer);
  });

  events.onConnect([](AsyncEventSourceClient *client) {
    if (events.count() > 2) {
      client->close();
      return;
    }

    char buffer[208];
    formatStateJson(buffer, sizeof(buffer));
    client->send(buffer, "state", millis());
  });
  server.addHandler(&events);

  server.on("/api/recalibrate", HTTP_POST, [](AsyncWebServerRequest *request) {
    recalibrationRequested = true;
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/mode", HTTP_POST, [](AsyncWebServerRequest *request) {
    const String &url = request->url();
    if (!url.startsWith("/api/mode/")) {
      request->send(400, "application/json", "{\"error\":\"missing mode\"}");
      return;
    }

    String value = url.substring(strlen("/api/mode/"));
    int index = benchModeIndexFromName(value.c_str());
    if (index < 0) {
      request->send(400, "application/json", "{\"error\":\"invalid mode\"}");
      return;
    }

    requestedModeIndex = index;

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "{\"ok\":true,\"mode\":\"%s\"}",
             benchModeName((BenchMode)index));
    request->send(200, "application/json", buffer);
  });

  server.on("/api/runs", HTTP_GET, [](AsyncWebServerRequest *request) {
    const String &url = request->url();

    if (url == "/api/runs") {
      RunMeta metas[MAX_RUNS];
      int count = listRuns(metas, MAX_RUNS);

      size_t capacity = 24 + (size_t)count * 128;
      char *buffer = (char *)malloc(capacity);
      if (buffer == nullptr) {
        request->send(500, "application/json", "{\"error\":\"out of memory\"}");
        return;
      }

      size_t offset = (size_t)snprintf(buffer, capacity, "[");
      for (int i = 0; i < count; i++) {
        offset += (size_t)snprintf(buffer + offset, capacity - offset,
                                   "%s{\"id\":%d,\"resultSeconds\":%.2f,\"date\":\"%s\",\"mode\":\"%s\"}",
                                   i == 0 ? "" : ",", metas[i].id, metas[i].resultSeconds,
                                   metas[i].date, metas[i].mode);
      }
      snprintf(buffer + offset, capacity - offset, "]");

      request->send(200, "application/json", buffer);
      free(buffer);
      return;
    }

    int id = atoi(url.c_str() + strlen("/api/runs/"));
    fs::File file = loadRun(id);
    if (!file) {
      request->send(404, "application/json", "{\"error\":\"not found\"}");
      return;
    }

    char path[32];
    snprintf(path, sizeof(path), "%s/%d.json", RUNS_DIRECTORY, id);
    request->send(request->beginResponse(file, path, "application/json"));
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });

  server.begin();

  Serial.println(F("HTTP server started"));
}
