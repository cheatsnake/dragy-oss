#include "speaker.h"
#include <Arduino.h>
#include "config.h"

void initSpeaker() {
  pinMode(SPEAKER_PIN, OUTPUT);
  noTone(SPEAKER_PIN);
}

void playSatellitesFoundSignal() {
  tone(SPEAKER_PIN, SATELLITES_FOUND_FREQ_HZ, SATELLITES_FOUND_DURATION_MS);
}

void playSixtyReachedSignal() {
  tone(SPEAKER_PIN, SIXTY_REACHED_FREQ_HZ, SIXTY_REACHED_DURATION_MS);
}

void stopSound() {
  noTone(SPEAKER_PIN);
}
