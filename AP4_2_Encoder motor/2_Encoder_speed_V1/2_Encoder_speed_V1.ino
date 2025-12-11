// Two JGB37-520 encoder motor, TB6612FNG driver, Speed

const int AIN1 = 8;
const int AIN2 = 9;
const int PWMA = 10;
const int STBY = 7;

const int BIN1 = 4;
const int BIN2 = 5;
const int PWMB = 6;

const int ITR_A = 2; // Interrupt pin
const int ITR_B = 3;

volatile long pulse_countA = 0;
volatile long pulse_countB = 0;

const unsigned long eventInterval = 300;
unsigned long previousTime = 0;

int pwmA = 80;
int pwmB = 80;   

void ISR_A() {
  int b = digitalRead(2);

  if (b == HIGH) {
    pulse_countA++;
  }
}

void ISR_B() {
  int b = digitalRead(3);

  if (b == HIGH) {
    pulse_countB++;
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);

  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);

  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, HIGH);

  pinMode(ITR_A, INPUT_PULLUP);
  pinMode(ITR_B, INPUT_PULLUP);

  pinMode(11, INPUT_PULLUP);  // Motor A Yellow
  pinMode(12, INPUT_PULLUP);  // Motor B Yellow

  attachInterrupt(digitalPinToInterrupt(ITR_A), ISR_A, RISING);
  attachInterrupt(digitalPinToInterrupt(ITR_B), ISR_B, RISING);
}

void loop() {
  unsigned long currentTime = millis();

  analogWrite(PWMA, pwmA);
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);

  analogWrite(PWMB, pwmB);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);

  if( currentTime - previousTime >= eventInterval) {

  Serial.print("PWMA = "); Serial.print(pwmA);
  Serial.print("  pulse_countA = "); Serial.print(pulse_countA);
  Serial.print("    PWMB = "); Serial.print(pwmB);
  Serial.print("  pulse_countB = "); Serial.println(pulse_countB);

  if(pulse_countA > pulse_countB) 
  { pwmB ++; }
  else if (pulse_countA < pulse_countB) 
  { pwmB --; }
  pulse_countA = 0; pulse_countB = 0 ;

  previousTime = currentTime ;
  }
}//=================
