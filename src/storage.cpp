#include "storage.h"

#include <LittleFS.h>

#include "config.h"

static RunMeta runMetas[MAX_RUNS];
static int runMetaCount = 0;
static int nextRunId = 1;

static void runFilePath(int id, char *buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize, "%s/%d.json", RUNS_DIRECTORY, id);
}

static bool parseRunMeta(const char *json, RunMeta &meta) {
  const char *idField = strstr(json, "\"id\":");
  const char *secondsField = strstr(json, "\"resultSeconds\":");
  const char *dateField = strstr(json, "\"gpsDateTime\":");

  if (idField == nullptr || secondsField == nullptr || dateField == nullptr) {
    return false;
  }

  meta.id = atoi(idField + 5);
  meta.resultSeconds = atof(secondsField + 16);

  const char *dateValue = dateField + 14;
  if (strncmp(dateValue, "null", 4) == 0) {
    meta.date[0] = '\0';
    return true;
  }

  const char *dateStart = strchr(dateValue, '"');
  const char *dateEnd = (dateStart != nullptr) ? strchr(dateStart + 1, '"') : nullptr;
  if (dateStart == nullptr || dateEnd == nullptr) {
    meta.date[0] = '\0';
    return true;
  }

  size_t length = (size_t)(dateEnd - dateStart - 1);
  if (length >= sizeof(meta.date)) {
    length = sizeof(meta.date) - 1;
  }
  memcpy(meta.date, dateStart + 1, length);
  meta.date[length] = '\0';
  return true;
}

static bool readRunMeta(int id, RunMeta &meta) {
  fs::File file = loadRun(id);
  if (!file) {
    return false;
  }

  char buffer[128];
  int length = file.read((uint8_t *)buffer, sizeof(buffer) - 1);
  file.close();

  if (length <= 0) {
    return false;
  }
  buffer[length] = '\0';
  return parseRunMeta(buffer, meta);
}

static void writeNextRunId() {
  fs::File file = LittleFS.open(RUN_NEXT_ID_PATH, "w");
  if (file) {
    file.print(nextRunId);
    file.close();
  }
}

void initStorage() {
  if (!LittleFS.begin(true)) {
    Serial.println(F("Storage: LittleFS mount failed"));
    return;
  }

  LittleFS.mkdir(RUNS_DIRECTORY);

  int ids[64];
  int idCount = 0;

  fs::File directory = LittleFS.open(RUNS_DIRECTORY);
  if (directory && directory.isDirectory()) {
    fs::File entry = directory.openNextFile();
    while (entry && idCount < (int)(sizeof(ids) / sizeof(ids[0]))) {
      const char *name = entry.name();
      const char *base = strrchr(name, '/');
      base = (base != nullptr) ? base + 1 : name;

      size_t length = strlen(base);
      if (length > 5 && strcmp(base + length - 5, ".json") == 0) {
        ids[idCount++] = atoi(base);
      }
      entry = directory.openNextFile();
    }
  }

  for (int i = 1; i < idCount; i++) {
    int value = ids[i];
    int j = i - 1;
    while (j >= 0 && ids[j] > value) {
      ids[j + 1] = ids[j];
      j--;
    }
    ids[j + 1] = value;
  }

  runMetaCount = 0;
  int firstKept = (idCount > MAX_RUNS) ? idCount - MAX_RUNS : 0;
  for (int i = firstKept; i < idCount; i++) {
    RunMeta meta;
    if (readRunMeta(ids[i], meta)) {
      runMetas[runMetaCount++] = meta;
    }
  }

  nextRunId = 1;
  bool counterValid = false;

  fs::File counter = LittleFS.open(RUN_NEXT_ID_PATH, "r");
  if (counter) {
    int value = counter.parseInt();
    counter.close();
    if (value > 0) {
      nextRunId = value;
      counterValid = true;
    }
  }

  if (idCount > 0 && nextRunId <= ids[idCount - 1]) {
    nextRunId = ids[idCount - 1] + 1;
    counterValid = false;
  }

  if (!counterValid) {
    writeNextRunId();
  }

  Serial.print(F("Storage: "));
  Serial.print(runMetaCount);
  Serial.print(F(" run(s), next id "));
  Serial.println(nextRunId);
}

int getNextRunId() {
  return nextRunId;
}

int saveRun(const char *json) {
  int id = nextRunId;

  char path[32];
  runFilePath(id, path, sizeof(path));

  fs::File file = LittleFS.open(path, "w");
  if (!file) {
    Serial.println(F("Storage: failed to open run file for writing"));
    return -1;
  }

  size_t length = strlen(json);
  size_t written = file.write((const uint8_t *)json, length);
  file.close();

  if (written != length) {
    Serial.println(F("Storage: failed to write run file"));
    LittleFS.remove(path);
    return -1;
  }

  nextRunId++;
  writeNextRunId();

  RunMeta meta;
  if (parseRunMeta(json, meta)) {
    if (runMetaCount >= MAX_RUNS) {
      char oldestPath[32];
      runFilePath(runMetas[0].id, oldestPath, sizeof(oldestPath));
      LittleFS.remove(oldestPath);

      for (int i = 1; i < MAX_RUNS; i++) {
        runMetas[i - 1] = runMetas[i];
      }
      runMetaCount = MAX_RUNS - 1;
    }
    runMetas[runMetaCount++] = meta;
  }

  return id;
}

int listRuns(RunMeta *runs, int maxCount) {
  int count = 0;
  for (int i = runMetaCount - 1; i >= 0 && count < maxCount; i--) {
    runs[count++] = runMetas[i];
  }
  return count;
}

bool getLatestRunResult(float &resultSeconds) {
  if (runMetaCount == 0) {
    return false;
  }
  resultSeconds = runMetas[runMetaCount - 1].resultSeconds;
  return true;
}

fs::File loadRun(int id) {
  char path[32];
  runFilePath(id, path, sizeof(path));
  return LittleFS.open(path, "r");
}
