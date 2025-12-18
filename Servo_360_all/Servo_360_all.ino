#include <Servo.h> 

Servo myservo; 
int speed = 90; 

void setup() {
  myservo.attach(9); 
  myservo.write(90); // Stop
}

void loop() {
  for (speed = 90; speed > 0; speed -= 1) {
    myservo.write(speed); 
    delay(10); 
  }
  delay(2000); 

  myservo.write(90); // Stop
  delay(3000);

  for (speed = 90; speed < 180; speed += 1) {
    myservo.write(speed); 
    delay(10);
  }
  delay(2000);

  myservo.write(90); // Stop
  delay(3000);
}

