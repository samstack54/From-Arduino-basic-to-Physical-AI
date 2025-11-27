 
 #define PWMA  10 
 #define AIN2  9 
 #define AIN1  8
 #define STBY  7 

 const int ENCODER_A = 2; // D2 - Encoder pin(Yellow)
 const int ENCODER_PPR = 11; // 11-pulse/rotation

 volatile long pulse_count = 0;
 volatile unsigned long last_time = 0;

 float current_rpm = 0.0;
 unsigned long sample_time_ms = 1000; // Measure pulses

// --- Interrupt Service Routine (ISR) ---
void count_pulse() {
  pulse_count++;
}

void set_motor_speed(int speed_val, bool direction) {
  digitalWrite(STBY, HIGH); 
  
  if (direction) {
    // Clockwise (or forward)
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
  } else {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
  }
  
  analogWrite(PWMA, speed_val);
}//--------------------------------------------

 void setup() {
  Serial.begin(9600) ;

  pinMode(PWMA, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(STBY, OUTPUT); 
  digitalWrite(STBY, HIGH);

  pinMode(ENCODER_A, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(ENCODER_A), count_pulse, RISING);
  
  Serial.println("JGB37-520 Motor Test Initialized (11 PPR).");
  set_motor_speed(127, true); // Half speed forward
  last_time = millis();   
 } //---------------------------------------------

 void loop() {
  unsigned long current_time = millis();
 
  if (current_time - last_time >= sample_time_ms) {

    float time_in_minutes = (float)(current_time - last_time) / 60000.0; 

    noInterrupts();
    long pulses = pulse_count;
    pulse_count = 0; 
    interrupts();

    current_rpm = ((float)pulses / ENCODER_PPR) / time_in_minutes;

    Serial.print("Pulses: ");
    Serial.print(pulses);
    Serial.print(" | Calculated RPM: ");
    Serial.println(current_rpm);
  
    last_time = current_time;
  }
}//==========================