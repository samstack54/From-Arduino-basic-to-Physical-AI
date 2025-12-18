 // 서보모터 테스트
 #include <Servo.h>
 Servo smotor;  
  
 void setup() {
   smotor.attach(6);  //  6 번 핀에 모터
   Serial.begin(9600);
   smotor.write(0); // initilize to 0 degree
 }
 
 void loop()
 {
   for(int angle = 0; angle <= 160; angle++)  { 
     smotor.write(angle); 
     Serial.println(angle);            
     delay(50);                     
   }
   for(int angle = 160; angle > 0 ; angle--)   {
     smotor.write(angle);  
     Serial.println(angle);               
     delay(50);               
  }
 }
