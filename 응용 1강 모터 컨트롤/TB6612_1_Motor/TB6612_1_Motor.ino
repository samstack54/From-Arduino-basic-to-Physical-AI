 #define PWMA  10 
 #define AIN2  9 
 #define AIN1  8
 #define STBY  7 

 void setup() {
  Serial.begin(9600) ;

  pinMode(PWMA, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(STBY, OUTPUT); 
  digitalWrite(STBY, HIGH);   
 }

 void loop() {

  for (int Speed = 0 ; Speed <=255 ; Speed++) {
  Serial.print(" RT_Speed = ") ;
  Serial.println(Speed) ;
    
  analogWrite(PWMA, Speed) ;
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  delay(100) ;
  }

  digitalWrite(STBY, LOW);  // Standby motor
  Serial.println(" STBY ") ;  
  delay(3000) ;                           

  digitalWrite(STBY, HIGH); // Start motor
  
  for (int Speed = 255 ; Speed >=0 ; Speed--) {    
  Serial.print(" LT_Speed = ") ; 
  Serial.println(Speed) ;   

  analogWrite(PWMA, Speed) ;
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  delay(100) ;
  }                           
 }//==========================
