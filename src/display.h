#ifndef DISPLAY_H
#define DISPLAY_H

bool initDisplay();
void showMessage(const char *message);
void showSpeedView(int speedKmh, int satelliteCount, bool calibrationDotOn);
void showBenchView(int speedKmh, int satelliteCount, float elapsedSeconds);
void showResultView(float finalSeconds, int speedKmh, int satelliteCount);

#endif
