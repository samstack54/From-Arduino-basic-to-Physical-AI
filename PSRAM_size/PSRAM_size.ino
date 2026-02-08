// PSRAM and Flash size check

void setup() {
  Serial.begin(115200);
  delay(2000); // time to connect

  // If PSRAM is detected and initialized
  if (psramInit()) {
    Serial.println("PSRAM is found and initialized.");
  } else {
    Serial.println("PSRAM not found or not enabled in Tools menu.");
  }

  uint32_t total_psram = ESP.getPsramSize(); // Total PSRAM Size
  uint32_t free_psram = ESP.getFreePsram();  // Free PSRAM Size

  Serial.print("Total PSRAM: ");
  Serial.print(total_psram / 1024.0 / 1024.0);
  Serial.println(" MB");

  Serial.print("Free PSRAM:  ");
  Serial.print(free_psram / 1024.0 / 1024.0);
  Serial.println(" MB");

  uint32_t flash_size = ESP.getFlashChipSize(); // Flash Size 
  Serial.print("Flash Size:  ");
  Serial.print(flash_size / 1024.0 / 1024.0);
  Serial.println(" MB");
  
  Serial.println("---------------------------------");
}

void loop() {  }