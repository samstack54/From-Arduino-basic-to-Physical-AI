// ESP32-S3 Free send data to "Processing Receive INMP V1"
#include <driver/i2s.h>

#define I2S_SD 10
#define I2S_WS 11
#define I2S_SCK 12
#define I2S_PORT I2S_NUM_0

#define SAMPLE_RATE     16000
#define RECORD_SECONDS  10
#define BUFFER_LEN      1024
int16_t sBuffer[BUFFER_LEN];
#define START_BTN 1

void setup() {
  Serial.begin(2000000);
  pinMode(START_BTN, INPUT_PULLUP);
  delay(1000);

  // Setup I2S
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | 
    I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);

  Serial.println(" "); delay(500);
  Serial.println("Ready: Close Serial Monitor. Run Processing. Press Button to START");
}

void loop() {
  // wait for button press (LOW)
  while (digitalRead(START_BTN) == HIGH) {
    delay(50);
  }
  // simple debounce
  delay(50);
  while (digitalRead(START_BTN) == LOW) {
    delay(50); // wait until release
  }

  uint32_t startMillis = millis();
  while (millis() - startMillis < RECORD_SECONDS * 1000) {
    size_t bytesRead;
    int32_t samples[BUFFER_LEN];
    i2s_read(I2S_PORT, (void*)samples, sizeof(samples), &bytesRead, portMAX_DELAY);
    int sampleCount = bytesRead / sizeof(int32_t);

    for (int i = 0; i < sampleCount; i++) {
      int16_t val = samples[i] >> 14; // Scale down 32-bit PDM to int16
      Serial.println(val);
    }
  }

  Serial.println("DONE");
  delay(5000);
}
