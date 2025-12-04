const uint8_t PIN_A = 2; 
const uint8_t PIN_B = 3;  

volatile long position = 0;
volatile uint8_t lastAB = 0;
volatile bool indexSeen = false;

inline uint8_t readAB() {
  uint8_t a = digitalRead(PIN_A);
  uint8_t b = digitalRead(PIN_B);
  return (a << 1) | b;
}

void isrAB() {
  uint8_t curr = readAB();
  int8_t d = 0;

  switch (lastAB) {
    case 0:  // 00
      if (curr == 1)      d = -1;   // 00 -> 01
      else if (curr == 2) d = +1;   // 00 -> 10
      break;
    case 1:  // 01
      if (curr == 3)      d = -1;   // 01 -> 11
      else if (curr == 0) d = +1;   // 01 -> 00
      break;
    case 3:  // 11
      if (curr == 2)      d = -1;   // 11 -> 10
      else if (curr == 1) d = +1;   // 11 -> 01
      break;
    case 2:  // 10
      if (curr == 0)      d = -1;   // 10 -> 00
      else if (curr == 3) d = +1;   // 10 -> 11
      break;
  }

  position += d;
  lastAB = curr;
}

void setup() {
  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);

  lastAB = readAB();

  attachInterrupt(0, isrAB, CHANGE);
  attachInterrupt(1, isrAB, CHANGE);

  Serial.begin(115200);
}

void loop() {
  static long last = 0;
  noInterrupts();
  long p = position;
  interrupts();

  if (p != last) {
    Serial.print("pos: "); Serial.print(p);
    long Angle = (p*360)/1024;
    Serial.print("  Angle : "); Serial.println(Angle);
    last = p;
  }
}//=====================