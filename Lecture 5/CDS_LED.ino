 // analog read from A0 then LED at 8

 int LED = 8 ;
 
 void setup() {
  pinMode(LED,OUTPUT) ;
  Serial.begin(9600) ;
 }

 void loop() {
 int CDS = analogRead(A0) ;
 Serial.print("CDS= ") ;
 Serial.println(CDS) ;

  if(CDS <= 300) {
   digitalWrite(LED, HIGH) ;  
   } 
  else { 
    digitalWrite(LED, LOW) ; 
    }
  delay(50) ;
 }
