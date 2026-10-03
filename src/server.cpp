#include "server.h"

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "config.h"
#include "page.h"

static AsyncWebServer server(80);
static AsyncEventSource events("/events");

static UiState latestState = {"SEARCHING_GPS", 0.0f, 0, false, 0.0f, 0.0f, false};
static volatile bool recalibrationRequested = false;
static unsigned long lastHeartbeatMillis = 0;

static void formatStateJson(char *buffer, size_t bufferSize) {
  char resultField[16];
  if (latestState.hasLastResult) {
    snprintf(resultField, sizeof(resultField), "%.2f", latestState.lastResultSeconds);
  } else {
    strcpy(resultField, "null");
  }

  snprintf(buffer, bufferSize,
           "{\"stage\":\"%s\",\"speedKmh\":%.1f,\"satellites\":%d,\"calibrating\":%s,"
           "\"elapsedSeconds\":%.2f,\"lastResultSeconds\":%s}",
           latestState.stage, latestState.speedKmh, latestState.satellites,
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

  char buffer[192];
  formatStateJson(buffer, sizeof(buffer));
  events.send(buffer, "state");
}

void publishRunResult(const RunMeta &meta) {
  char buffer[128];
  snprintf(buffer, sizeof(buffer), "{\"id\":%d,\"resultSeconds\":%.2f,\"date\":\"%s\"}", meta.id,
           meta.resultSeconds, meta.date);
  events.send(buffer, "result");
}

bool isRecalibrationRequested() {
  return recalibrationRequested;
}

void clearRecalibrationRequest() {
  recalibrationRequested = false;
}

void initWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", INDEX_PAGE);
  });

  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest *request) {
    char buffer[192];
    formatStateJson(buffer, sizeof(buffer));
    request->send(200, "application/json", buffer);
  });

  events.onConnect([](AsyncEventSourceClient *client) {
    if (events.count() > 2) {
      client->close();
      return;
    }

    char buffer[192];
    formatStateJson(buffer, sizeof(buffer));
    client->send(buffer, "state", millis());
  });
  server.addHandler(&events);

  server.on("/api/recalibrate", HTTP_POST, [](AsyncWebServerRequest *request) {
    recalibrationRequested = true;
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/runs", HTTP_GET, [](AsyncWebServerRequest *request) {
    const String &url = request->url();

    if (url == "/api/runs") {
      RunMeta metas[MAX_RUNS];
      int count = listRuns(metas, MAX_RUNS);

      size_t capacity = 16 + (size_t)count * 96;
      char *buffer = (char *)malloc(capacity);
      if (buffer == nullptr) {
        request->send(500, "application/json", "{\"error\":\"out of memory\"}");
        return;
      }

      size_t offset = (size_t)snprintf(buffer, capacity, "[");
      for (int i = 0; i < count; i++) {
        offset += (size_t)snprintf(buffer + offset, capacity - offset,
                                   "%s{\"id\":%d,\"resultSeconds\":%.2f,\"date\":\"%s\"}",
                                   i == 0 ? "" : ",", metas[i].id, metas[i].resultSeconds,
                                   metas[i].date);
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
