#include <Wire.h>

byte ADXL346 = 0x53;

// Threshold settings (in g)
const float START_THRESHOLD = 0.2;  // Start threshold
const float STOP_THRESHOLD  = 0.1;  // Sound off threshold

const int TONE_PIN = 4;
const int TONE_FREQ = 350;

// Bump filtering coefficient (EMA filter)
const float FILTER_ALPHA = 0.2; 

// Rest calibration matrix
float biasX = 0, biasY = 0, biasZ = 0;

// Axis indices: which one became vertical?
int verticalAxis = 2; // 0=X, 1=Y, 2=Z (Z by default)

// Filtered acceleration value
float filteredAcc = 0;

bool movementActive = false;
unsigned long startTime = 0;

void setup() {
  Serial.begin(9600);
  Wire.begin();
  pinMode(TONE_PIN, OUTPUT);

  // Enable measurement mode (POWER_CTL = 8)
  Wire.beginTransmission(ADXL346);
  Wire.write(0x2D);
  Wire.write(8);
  Wire.endTransmission();
  delay(200);

  // Full position-in-space calibration
  calibratePosition();
}

void loop() {
  Wire.beginTransmission(ADXL346);
  Wire.write(0x32);
  Wire.endTransmission(false);
  Wire.requestFrom(ADXL346, 6, true);

  if (Wire.available() == 6) {
    int rawX = (Wire.read() | (Wire.read() << 8));
    int rawY = (Wire.read() | (Wire.read() << 8));
    int rawZ = (Wire.read() | (Wire.read() << 8));

    // Convert to "clean" g without rest calibration
    float gX = (rawX * 0.0039) - biasX;
    float gY = (rawY * 0.0039) - biasY;
    float gZ = (rawZ * 0.0039) - biasZ;

    float accHorizontal = 0;

    // Compute acceleration only along the two axes lying in the road plane
    if (verticalAxis == 0) {
      // Vertical is X. Road is Y and Z
      accHorizontal = sqrt(gY * gY + gZ * gZ);
    } else if (verticalAxis == 1) {
      // Vertical is Y. Road is X and Z
      accHorizontal = sqrt(gX * gX + gZ * gZ);
    } else {
      // Vertical is Z. Road is X and Y
      accHorizontal = sqrt(gX * gX + gY * gY);
    }

    // Filter out suspension shake and bumps
    filteredAcc = (accHorizontal * FILTER_ALPHA) + (filteredAcc * (1.0 - FILTER_ALPHA));

    // Output to Arduino plotter for testing
    // Serial.print("Acc_Horizontal:");
    // Serial.println(filteredAcc, 3);

    // RaceLogic trigger logic
    if (!movementActive) {
      if (filteredAcc > START_THRESHOLD) {
        movementActive = true;
        startTime = millis();
        tone(TONE_PIN, TONE_FREQ);
        Serial.println(">>> START! <<<");
      }
    } else {
      if (filteredAcc < STOP_THRESHOLD) {
        movementActive = false;
        noTone(TONE_PIN);
        float duration = (millis() - startTime) / 1000.0;
        Serial.print(">>> STOP! Time: ");
        Serial.print(duration, 2);
        Serial.println(" s <<<");
      }
    }
  }

  delay(100);
}

// Smart sensor position calibration
void calibratePosition() {
  Serial.println("Calibrating position... DON'T MOVE THE CAR!");
  long sumX = 0, sumY = 0, sumZ = 0;
  const int samples = 50;
  
  for (int i = 0; i < samples; i++) {
    Wire.beginTransmission(ADXL346);
    Wire.write(0x32);
    Wire.endTransmission(false);
    Wire.requestFrom(ADXL346, 6, true);
    if (Wire.available() == 6) {
      sumX += (int16_t)(Wire.read() | (Wire.read() << 8));
      sumY += (int16_t)(Wire.read() | (Wire.read() << 8));
      sumZ += (int16_t)(Wire.read() | (Wire.read() << 8));
    }
    delay(20);
  }
  
  // Average raw g values at rest
  float avgX = (sumX / (float)samples) * 0.0039;
  float avgY = (sumY / (float)samples) * 0.0039;
  float avgZ = (sumZ / (float)samples) * 0.0039;

  // Find the axis with the current maximum absolute value (Earth's gravity ~1g)
  float absX = abs(avgX);
  float absY = abs(avgY);
  float absZ = abs(avgZ);

  if (absX > absY && absX > absZ) {
    verticalAxis = 0; // Vertical axis is X
    Serial.println("Detected orientation: X is VERTICAL");
  } else if (absY > absX && absY > absZ) {
    verticalAxis = 1; // Vertical axis is Y
    Serial.println("Detected orientation: Y is VERTICAL");
  } else {
    verticalAxis = 2; // Vertical axis is Z
    Serial.println("Detected orientation: Z is VERTICAL");
  }

  // Record the biases.
  // For horizontal axes we compensate them to 0.
  // For the vertical axis we keep all Earth gravity so it can be subtracted later.
  biasX = avgX;
  biasY = avgY;
  biasZ = avgZ;
  
  Serial.println("Calibration success!");
}
