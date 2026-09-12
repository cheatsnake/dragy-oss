#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

const int OLED_SDA_PIN = 1;
const int OLED_SCL_PIN = 0;
const uint8_t OLED_ADDRESS = 0x3C;
const int SCREEN_WIDTH_PX = 128;
const int SCREEN_HEIGHT_PX = 64;
const int FONT_CHAR_WIDTH_PX = 6;
const int FONT_CHAR_HEIGHT_PX = 8;
const int BIG_TEXT_SIZE = 4;
const int SMALL_TEXT_SIZE = 1;
const int CALIBRATION_DOT_RADIUS_PX = 3;
const int CALIBRATION_DOT_MARGIN_PX = 4;

const int GPS_RX_PIN = 4;
const int GPS_TX_PIN = 3;
const unsigned long GPS_BAUD_INITIAL = 9600;
const unsigned long GPS_BAUD_FINAL = 115200;
const char GPS_COMMAND_SET_BAUD_115200[] = "$PCAS01,5*19\r\n";
const char GPS_COMMAND_SET_UPDATE_RATE_10HZ[] = "$PCAS02,100*1E\r\n";
const char GPS_COMMAND_SET_CONSTELLATIONS_GPS_BDS[] = "$PCAS04,3*1A\r\n";
const char GPS_COMMAND_SAVE_SETTINGS[] = "$PCAS00*01\r\n";

const uint8_t ACCELEROMETER_ADDRESS = 0x53;
const uint8_t ACCELEROMETER_REGISTER_POWER_CTL = 0x2D;
const uint8_t ACCELEROMETER_REGISTER_DATA_START = 0x32;
const uint8_t ACCELEROMETER_MEASURE_MODE_VALUE = 8;
const uint8_t ACCELEROMETER_DATA_BYTES = 6;
const float ACCELERATION_SCALE_G_PER_BIT = 0.0039f;
const float MOTION_START_THRESHOLD_G = 0.2f;
const float MOTION_STOP_THRESHOLD_G = 0.1f;
const float BUMP_FILTER_ALPHA = 0.2f;
const int CALIBRATION_SAMPLE_COUNT = 50;
const unsigned long CALIBRATION_SAMPLE_INTERVAL_MS = 20;
const unsigned long ACCELEROMETER_POWER_UP_DELAY_MS = 200;

const int SPEAKER_PIN = 10;
const unsigned int SATELLITES_FOUND_FREQ_HZ = 700;
const unsigned long SATELLITES_FOUND_DURATION_MS = 700;
const unsigned int SIXTY_REACHED_FREQ_HZ = 1000;
const unsigned long SIXTY_REACHED_DURATION_MS = 700;

const int TARGET_SPEED_KMH = 100;
const int RESET_SPEED_KMH = 3;
const unsigned long RESULT_DISPLAY_DURATION_MS = 20000;

const float MOVING_SPEED_KMH = 1.0f;
const unsigned long RECALIBRATION_SETTLE_TIME_MS = 3000;
const float HISTORY_RESET_BIAS_CHANGE_G = 0.05f;

const int ACCELEROMETER_HISTORY_SIZE = 16;
const int LAUNCH_CONFIRM_SAMPLES = 4;
const float CALIBRATION_ABORT_DEVIATION_G = 0.3f;
const float LAUNCH_MAX_START_SPEED_KMH = 3.0f;
const float LAUNCH_CONFIRM_SPEED_DELTA_KMH = 4.0f;
const unsigned long LAUNCH_CONFIRM_WINDOW_MS = 3000;

const int SPEED_HISTORY_SIZE = 12;
const float BENCH_ABORT_SPEED_DROP_KMH = 3.0f;
const unsigned long BENCH_STALL_TIMEOUT_MS = 3000;
const float BENCH_MIN_PROGRESS_KMH = 0.5f;

const unsigned long DISPLAY_INTERVAL_MS = 200;
const unsigned long ACCELEROMETER_INTERVAL_MS = 100;

#endif
