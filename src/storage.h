#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <FS.h>

struct RunMeta {
  int id;
  float resultSeconds;
  char date[20];
  char mode[10];
};

void initStorage();
int getNextRunId();
int saveRun(const char *json);
int listRuns(RunMeta *runs, int maxCount);
bool getLatestRunResult(float &resultSeconds);
fs::File loadRun(int id);
bool loadStoredMode(char *buffer, size_t bufferSize);
void saveStoredMode(const char *mode);

#endif
