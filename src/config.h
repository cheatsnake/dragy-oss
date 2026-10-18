#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <string.h>

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
const char GPS_COMMAND_STANDBY[] = "$PCAS12,65535*1E\r\n";
const char GPS_COMMAND_STANDBY_CANCEL[] = "$PCAS12,0*1E\r\n";
const char GPS_COMMAND_HOT_RESTART[] = "$PCAS10,0*1C\r\n";

const uint8_t ACCELEROMETER_ADDRESS = 0x53;
const uint8_t ACCELEROMETER_REGISTER_POWER_CTL = 0x2D;
const uint8_t ACCELEROMETER_REGISTER_DATA_START = 0x32;
const uint8_t ACCELEROMETER_REGISTER_BW_RATE = 0x2C;
const uint8_t ACCELEROMETER_REGISTER_THRESH_ACT = 0x24;
const uint8_t ACCELEROMETER_REGISTER_ACT_INACT_CTL = 0x27;
const uint8_t ACCELEROMETER_REGISTER_INT_ENABLE = 0x2E;
const uint8_t ACCELEROMETER_REGISTER_INT_MAP = 0x2F;
const uint8_t ACCELEROMETER_REGISTER_INT_SOURCE = 0x30;
const uint8_t ACCELEROMETER_STANDBY_MODE_VALUE = 0;
const uint8_t ACCELEROMETER_MEASURE_MODE_VALUE = 8;
const uint8_t ACCELEROMETER_RATE_DEFAULT = 0x0A;
const uint8_t ACCELEROMETER_RATE_LOW_POWER = 0x17;
const uint8_t ACCELEROMETER_ACTIVITY_THRESHOLD_CODE = 3;
const uint8_t ACCELEROMETER_ACTIVITY_ENABLES = 0xB8;
const uint8_t ACCELEROMETER_INTERRUPTS_INT1_VALUE = 0;
const uint8_t ACCELEROMETER_INTERRUPT_ACTIVITY_VALUE = 0x10;
const uint8_t ACCELEROMETER_ACTIVITY_DISABLED_VALUE = 0;
const uint8_t ACCELEROMETER_DATA_BYTES = 6;
const float ACCELERATION_SCALE_G_PER_BIT = 0.0039f;
const float MOTION_START_THRESHOLD_G = 0.2f;
const float MOTION_STOP_THRESHOLD_G = 0.1f;
const float BUMP_FILTER_ALPHA = 0.2f;
const int CALIBRATION_SAMPLE_COUNT = 50;
const unsigned long CALIBRATION_SAMPLE_INTERVAL_MS = 20;
const unsigned long ACCELEROMETER_POWER_UP_DELAY_MS = 200;

const int TARGET_SPEED_0_100_KMH = 100;
const int TARGET_SPEED_0_60_KMH = 60;
const int RESET_SPEED_KMH = 3;

enum BenchMode {
  BENCH_MODE_0_100 = 0,
  BENCH_MODE_0_60 = 1,
  BENCH_MODE_SPEEDO = 2
};

const char MODE_NAME_0_100[] = "0-100";
const char MODE_NAME_0_60[] = "0-60";
const char MODE_NAME_SPEEDO[] = "speedo";
const char MODE_PATH[] = "/mode";

inline const char *benchModeName(BenchMode mode) {
  switch (mode) {
    case BENCH_MODE_0_60:
      return MODE_NAME_0_60;
    case BENCH_MODE_SPEEDO:
      return MODE_NAME_SPEEDO;
    default:
      return MODE_NAME_0_100;
  }
}

inline int benchModeIndexFromName(const char *name) {
  if (strcmp(name, MODE_NAME_0_100) == 0) {
    return BENCH_MODE_0_100;
  }
  if (strcmp(name, MODE_NAME_0_60) == 0) {
    return BENCH_MODE_0_60;
  }
  if (strcmp(name, MODE_NAME_SPEEDO) == 0) {
    return BENCH_MODE_SPEEDO;
  }
  return -1;
}

inline int benchModeTargetSpeedKmh(BenchMode mode) {
  switch (mode) {
    case BENCH_MODE_0_60:
      return TARGET_SPEED_0_60_KMH;
    case BENCH_MODE_SPEEDO:
      return 0;
    default:
      return TARGET_SPEED_0_100_KMH;
  }
}

inline bool benchModeMeasures(BenchMode mode) {
  return mode != BENCH_MODE_SPEEDO;
}

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
const char WIFI_AP_PASSWORD[] = "11112222";

const int SLEEP_WAKEUP_GPIO = 5;
const unsigned long SLEEP_IDLE_TIMEOUT_MS = 300000;
const float SLEEP_SPEED_GUARD_KMH = 2.0f;
const unsigned long SLEEP_SHUTDOWN_FLUSH_MS = 300;

#endif
