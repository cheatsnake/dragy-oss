#ifndef RUNRECORDER_H
#define RUNRECORDER_H

#include <Arduino.h>

#include "storage.h"

void startRunRecording(unsigned long launchStartMillis);
void updateRunRecording();
bool finishRunRecording(float resultSeconds, RunMeta &meta);
void abortRunRecording();

#endif
