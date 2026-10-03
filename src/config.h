#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

const int I2C_SDA_PIN = 1;
const int I2C_SCL_PIN = 0;

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

const int TARGET_SPEED_KMH = 100;
const int RESET_SPEED_KMH = 3;

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

const unsigned long ACCELEROMETER_INTERVAL_MS = 100;

const unsigned long UI_STATE_INTERVAL_MS = 200;
const unsigned long SSE_HEARTBEAT_INTERVAL_MS = 1000;

const int MAX_RUNS = 10;
const unsigned long RUN_SAMPLE_INTERVAL_MS = 100;
const int RUN_MAX_SAMPLES = 400;
const char RUNS_DIRECTORY[] = "/runs";
const char RUN_NEXT_ID_PATH[] = "/runs/nextid";

const char WIFI_AP_SSID[] = "Dragy";
const char WIFI_AP_PASSWORD[] = "dragybench";

#endif
