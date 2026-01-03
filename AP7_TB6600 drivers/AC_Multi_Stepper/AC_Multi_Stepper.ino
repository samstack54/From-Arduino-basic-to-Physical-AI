#include <AccelStepper.h>

const uint8_t DIR1 = 2, STEP1 = 3;
const uint8_t DIR2 = 5, STEP2 = 6;
const uint8_t DIR3 = 8, STEP3 = 9;

const uint8_t ENA1 = 4, ENA2 = 7, ENA3 = 10;
const bool ENA_ACTIVE_LEVEL = LOW;

AccelStepper m1(1, STEP1, DIR1); 
AccelStepper m2(1, STEP2, DIR2);
AccelStepper m3(1, STEP3, DIR3);

const int FULL_STEPS_PER_REV = 200; // 1.8°/step
const int MICROSTEP = 8;            
const int STEPS_PER_REV = FULL_STEPS_PER_REV * MICROSTEP; // 1600

void enableAll(bool en) {
  digitalWrite(ENA1, en ? ENA_ACTIVE_LEVEL : !ENA_ACTIVE_LEVEL);
  digitalWrite(ENA2, en ? ENA_ACTIVE_LEVEL : !ENA_ACTIVE_LEVEL);
  digitalWrite(ENA3, en ? ENA_ACTIVE_LEVEL : !ENA_ACTIVE_LEVEL);
}

void runAllToPosition() {
  // Run until all motors reach their target
  while (m1.distanceToGo() != 0 || m2.distanceToGo() != 0 || m3.distanceToGo() != 0) {
    m1.run();
    m2.run();
    m3.run();
  }
}

void setup() {
  pinMode(ENA1, OUTPUT);
  pinMode(ENA2, OUTPUT);
  pinMode(ENA3, OUTPUT);
  enableAll(true);

  m1.setMaxSpeed(2000);
  m2.setMaxSpeed(2000);
  m3.setMaxSpeed(2000);

  m1.setAcceleration(1800);
  m2.setAcceleration(1800);
  m3.setAcceleration(1800);
}

void loop() {
  // 360°
  m1.moveTo(STEPS_PER_REV);
  m2.moveTo(STEPS_PER_REV);
  m3.moveTo(STEPS_PER_REV);
  runAllToPosition();

  delay(500);

  // Back to zero
  m1.moveTo(0);
  m2.moveTo(0);
  m3.moveTo(0);
  runAllToPosition();

  delay(1500);
}