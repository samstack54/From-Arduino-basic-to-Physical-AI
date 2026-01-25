#include <Wire.h>
#include <MPU6050.h>

MPU6050 mpu;

const int BUTTON_PIN = 1;         // External pull-up on GPIO1
const unsigned long DURATION_MS = 10000;
const unsigned long INTERVAL_MS = 16;  // ~62.5 Hz

void setup() {
  Serial.begin(115200);
  Wire.begin(7, 6);  // SDA = GPIO7, SCL = GPIO6 (default on ESP32-S3)
  
  pinMode(BUTTON_PIN, INPUT);  // External pull-up
  Serial.println("Ready press button to START");

  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed");
    while (1);
  }
}

void loop() {
  // Wait for button press (LOW = pressed)
  while (digitalRead(BUTTON_PIN) == HIGH) {
    delay(10);
  }

  Serial.println("START");

  unsigned long startTime = millis();
  unsigned long lastSampleTime = 0;

  while (millis() - startTime < DURATION_MS) {
    unsigned long currentTime = millis();
    if (currentTime - lastSampleTime >= INTERVAL_MS) {
      lastSampleTime = currentTime;

    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);

      float fax = ax / 16384.0;
      float fay = ay / 16384.0;
      float faz = az / 16384.0;

      Serial.print(fax, 4);
      Serial.print(",");
      Serial.print(fay, 4);
      Serial.print(",");
      Serial.println(faz, 4);
    }
  }

  Serial.println("DONE");

  while (digitalRead(BUTTON_PIN) == LOW) {
    delay(10); // ~62.5 Hz
  }
}
