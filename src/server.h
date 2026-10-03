#ifndef SERVER_H
#define SERVER_H

#include "storage.h"

struct UiState {
  const char *stage;
  const char *mode;
  float speedKmh;
  int satellites;
  bool calibrating;
  float elapsedSeconds;
  float lastResultSeconds;
  bool hasLastResult;
};

void initWebServer();
void publishUiState(const UiState &state);
void publishRunResult(const RunMeta &meta);
bool isRecalibrationRequested();
void clearRecalibrationRequest();
int takeModeChangeRequest();

#endif
