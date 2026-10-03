#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <FS.h>

struct RunMeta {
  int id;
  float resultSeconds;
  char date[20];
};

void initStorage();
int getNextRunId();
int saveRun(const char *json);
int listRuns(RunMeta *runs, int maxCount);
bool getLatestRunResult(float &resultSeconds);
fs::File loadRun(int id);

#endif
