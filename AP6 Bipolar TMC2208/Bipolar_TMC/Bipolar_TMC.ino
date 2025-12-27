#include <AccelStepper.h>   

const int dir  = 4;
const int step = 5;   
long motorPos = 0;  

AccelStepper stepper(AccelStepper::DRIVER, step, dir);   
void setup()   
{   
  Serial.begin(9600);  
  stepper_init();    
  stepper.setCurrentPosition(0);  
}   
  
void loop()   
{   
  stepper.moveTo(1600);  
  stepper.runToPosition();  
  motorPos = stepper.currentPosition();  
  Serial.println(motorPos);  
  stepper.stop();  
  delay(1000);  
  
  stepper.moveTo(0);  
  stepper.runToPosition();  
  motorPos = stepper.currentPosition();  
  Serial.println(motorPos);  
  delay(1000);  
}   
  
void stepper_init(){   
  stepper.setMaxSpeed(2000);   
  stepper.setAcceleration(1500);   
}  
