// --- Motor Driver Pins (TB6612FNG) ---
#define PWMA  10 
#define AIN2  9 
#define AIN1  8
#define STBY  7 

const int ENCODER_A = 2; // D2 - Encoder pin(Yellow)
const int ENCODER_B = 3; // D3 - Encoder pin(Green)

const int ENCODER_PPR  = 11;    // 11-pulse/rotation
const int EFFECTIVE_PPR = ENCODER_PPR * 2;  // using A and B

volatile long pulse_count = 0;
volatile unsigned long last_time = 0;
volatile bool current_direction = true; 

float current_rpm = 0.0;
unsigned long sample_time_ms = 1000; 

// --- Sequence Control Variables ---
unsigned long state_timer = 0;
enum State { FORWARD_RUN, STOP_1, REVERSE_RUN, STOP_2 };
State motor_state = FORWARD_RUN; 

const int FORWARD_SPEED = 150; 
const int REVERSE_SPEED = 100;  

const unsigned long RUN_DURATION_MS = 4000;  // 4 seconds
const unsigned long STOP_DURATION_MS = 3000; // 3 second

// --- Interrupt Service Routine (ISR) for Channel A ---
void read_encoder() {
  int b = digitalRead(ENCODER_B);

  if (b == HIGH) {
    pulse_count++;
    current_direction = true;   // Forward
  } else {
    pulse_count--;
    current_direction = false;  // Reverse
  }
}

void set_motor_speed(int speed_val, bool direction) {
  digitalWrite(STBY, HIGH); 
  
  if (speed_val == 0) {
    // Stop / coast
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);
  } else if (direction) {
    // Forward (true)
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
  } else {
    // Reverse (false)
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
  }
  analogWrite(PWMA, speed_val);
}

// --- Setup ---
void setup() {
  Serial.begin(9600);

  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(STBY, OUTPUT);

  pinMode(ENCODER_A, INPUT_PULLUP); 
  pinMode(ENCODER_B, INPUT_PULLUP); 

  // Use only A RISING edge + B for direction
  attachInterrupt(digitalPinToInterrupt(ENCODER_A), read_encoder, RISING);
  
  Serial.println("JGB37-520 Motor Sequence Initialized.");
  
  set_motor_speed(FORWARD_SPEED, true);
  state_timer = millis();
  last_time = millis(); 
}

// --- Loop ---
void loop() {
  unsigned long current_time = millis();

  switch (motor_state) {
    case FORWARD_RUN:
      if (current_time - state_timer >= RUN_DURATION_MS) {
        set_motor_speed(0, true); // Stop
        motor_state = STOP_1;
        state_timer = current_time;
        Serial.println("\nState Change: STOP_1 (Braking)");
      }
      break;
      
    case STOP_1:
      if (current_time - state_timer >= STOP_DURATION_MS) {
        set_motor_speed(REVERSE_SPEED, false); 
        motor_state = REVERSE_RUN;
        state_timer = current_time;
        Serial.println("\nState Change: REVERSE_RUN");
      }
      break;
      
    case REVERSE_RUN:
      if (current_time - state_timer >= RUN_DURATION_MS) {
        set_motor_speed(0, true); // Stop
        motor_state = STOP_2;
        state_timer = current_time;
        Serial.println("\nState Change: STOP_2 (Braking)");
      }
      break;
      
    case STOP_2:
      if (current_time - state_timer >= STOP_DURATION_MS) {
        set_motor_speed(FORWARD_SPEED, true); 
        motor_state = FORWARD_RUN;
        state_timer = current_time;
        Serial.println("\nState Change: FORWARD_RUN");
      }
      break;
  }
  
  // Speed measurement block
  if (current_time - last_time >= sample_time_ms) {
    float time_in_minutes = (float)(current_time - last_time) / 60000.0; 
    
    noInterrupts();
    long pulses = pulse_count;
    pulse_count = 0;
    interrupts();
    
    long abs_pulses = abs(pulses); 
    
    current_rpm = ((float)abs_pulses / EFFECTIVE_PPR) / time_in_minutes;

    // NEW: direction text based on state machine
    if (abs_pulses > 0) {
      Serial.print("Direction: ");
      // Use commanded motor state for direction display
      if (motor_state == REVERSE_RUN || motor_state == STOP_2) {
        Serial.print("Rev");
      } else {
        Serial.print("Fwd");
      }
      Serial.print(" | Pulses/sample: ");
      Serial.print(abs_pulses);
      Serial.print(" | RPM: ");
      Serial.println(current_rpm);
    }
   
    last_time = current_time;
  }
}
