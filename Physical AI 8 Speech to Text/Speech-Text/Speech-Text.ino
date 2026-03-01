#include <Arduino.h>
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "FS.h"
#include "SD_MMC.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "  ";
const char* password = "  ";

// ElevenLabs API configuration
const char* elevenlabs_api_key = " ";
const char* elevenlabs_stt_url = "https://api.elevenlabs.io/v1/speech-to-text";

// Hardware Pins for Freenove ESP32-S3
#define BUTTON_PIN 1   
#define LED_PIN    2   

// Audio recording settings
#define WAV_FILE_NAME "recording"
#define SAMPLE_RATE 16000U
#define SAMPLE_BITS 16
#define WAV_HEADER_SIZE 44
#define VOLUME_GAIN 3        // Increased gain for clearer voice
#define I2S_SHIFT_BITS 11    

// I2S Configuration (INMP441)
#define I2S_NUM I2S_NUM_0
#define I2S_SCK_GPIO (gpio_num_t)12   
#define I2S_WS_GPIO  (gpio_num_t)11   
#define I2S_SD_GPIO  (gpio_num_t)10   

bool wifi_connected = false;
String current_recording_file = "";

// Function Declarations
bool connectToWiFi();
bool init_i2s_legacy();
void record_wav_streaming();
void process_recording();
String send_to_elevenlabs_stt(String filename);
void generate_wav_header(uint8_t* wav_header, uint32_t wav_size, uint32_t sample_rate);
bool mountSDMMC();
void cleanupOldRecordings();

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); 

  if (!init_i2s_legacy()) { Serial.println("I2S Fail"); while (1); }
  if (!mountSDMMC()) { Serial.println("SD Fail"); while (1); }

  cleanupOldRecordings();
  connectToWiFi();

  digitalWrite(LED_PIN, HIGH); 
  Serial.println(">>> System Ready. HOLD button to record.");
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(300); // Debounce 
    if (digitalRead(BUTTON_PIN) == LOW) {
      digitalWrite(LED_PIN, LOW); 
      Serial.println("Recording...");
      
      record_wav_streaming();
      process_recording();
      
      while(digitalRead(BUTTON_PIN) == LOW) { delay(300); } 
      digitalWrite(LED_PIN, HIGH); 
      Serial.println(">>> Ready.");
    }
  }
}

bool init_i2s_legacy() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    // CHANGE: Changed to LEFT channel as most INMP441 have L/R tied to GND
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, 
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 6,
    .dma_buf_len = 256,
    .use_apll = false
  };

  if (i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL) != ESP_OK) return false;

  i2s_pin_config_t pin_config = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = (int)I2S_SCK_GPIO,
    .ws_io_num = (int)I2S_WS_GPIO,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = (int)I2S_SD_GPIO
  };
  
  if (i2s_set_pin(I2S_NUM, &pin_config) != ESP_OK) return false;
  return (i2s_set_clk(I2S_NUM, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_MONO) == ESP_OK);
}

void record_wav_streaming() {
  const uint32_t max_record_time = 15; 
  String filename = "/" + String(WAV_FILE_NAME) + "_" + String(millis()) + ".wav";
  current_recording_file = filename;

  File file = SD_MMC.open(filename.c_str(), FILE_WRITE);
  if (!file) return;

  uint8_t wav_header[WAV_HEADER_SIZE];
  generate_wav_header(wav_header, 0, SAMPLE_RATE);
  file.write(wav_header, WAV_HEADER_SIZE);

  const size_t rx_bytes = 1024;
  int32_t* rx_buffer = (int32_t*)malloc(rx_bytes);
  int16_t* tx_buffer = (int16_t*)malloc(rx_bytes / 2);

  size_t total_bytes = 0;
  unsigned long startTime = millis();

  while (digitalRead(BUTTON_PIN) == LOW && (millis() - startTime < max_record_time * 1000)) {
    size_t bytes_read = 0;
    i2s_read(I2S_NUM, (void*)rx_buffer, rx_bytes, &bytes_read, portMAX_DELAY);

    size_t samples_read = bytes_read / sizeof(int32_t);
    for (size_t i = 0; i < samples_read; i++) {
      int32_t v = rx_buffer[i] >> I2S_SHIFT_BITS;
      v = v << VOLUME_GAIN;
      tx_buffer[i] = (int16_t)constrain(v, -32768, 32767);
    }
    file.write((uint8_t*)tx_buffer, samples_read * sizeof(int16_t));
    total_bytes += (samples_read * sizeof(int16_t));
  }

  free(rx_buffer); free(tx_buffer);
  file.seek(0);
  generate_wav_header(wav_header, total_bytes, SAMPLE_RATE);
  file.write(wav_header, WAV_HEADER_SIZE);
  file.close();
}

void process_recording() {
  if (current_recording_file.isEmpty()) return;
  Serial.println("Sending to ElevenLabs...");
  String transcription = send_to_elevenlabs_stt(current_recording_file);
  Serial.println("Result: " + (transcription.length() ? transcription : "(No speech detected)"));
  current_recording_file = "";
}

String send_to_elevenlabs_stt(String filename) {
  if (!wifi_connected) return "";
  File file = SD_MMC.open(filename.c_str());
  if (!file) return "";

  size_t file_size = file.size();
  uint8_t* audio_data = (uint8_t*)malloc(file_size);
  file.read(audio_data, file_size);
  file.close();

  HTTPClient http;
  http.begin(elevenlabs_stt_url);
  http.setTimeout(30000);
  http.addHeader("xi-api-key", elevenlabs_api_key);

  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

  String body_start = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"model_id\"\r\n\r\nscribe_v1\r\n";
  body_start += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
  String body_end = "\r\n--" + boundary + "--\r\n";

  size_t total_size = body_start.length() + file_size + body_end.length();
  uint8_t* complete_body = (uint8_t*)malloc(total_size);
  memcpy(complete_body, body_start.c_str(), body_start.length());
  memcpy(complete_body + body_start.length(), audio_data, file_size);
  memcpy(complete_body + body_start.length() + file_size, body_end.c_str(), body_end.length());

  int httpCode = http.POST(complete_body, total_size);
  String response = http.getString();
  
  free(audio_data); free(complete_body);
  http.end();

  if (httpCode == 200) {
    JsonDocument doc;
    deserializeJson(doc, response);
    return doc["text"].as<String>();
  }
  return "";
}

bool connectToWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  wifi_connected = true;
  Serial.println("\nWiFi Connected");
  return true;
}

void generate_wav_header(uint8_t* wav_header, uint32_t wav_size, uint32_t sample_rate) {
  uint32_t file_size = wav_size + WAV_HEADER_SIZE - 8;
  uint32_t byte_rate = sample_rate * 2;
  const uint8_t header[] = {
    'R','I','F','F', (uint8_t)file_size, (uint8_t)(file_size>>8), (uint8_t)(file_size>>16), (uint8_t)(file_size>>24),
    'W','A','V','E','f','m','t',' ', 0x10, 0, 0, 0, 0x01, 0, 0x01, 0,
    (uint8_t)sample_rate, (uint8_t)(sample_rate>>8), (uint8_t)(sample_rate>>16), (uint8_t)(sample_rate>>24),
    (uint8_t)byte_rate, (uint8_t)(byte_rate>>8), (uint8_t)(byte_rate>>16), (uint8_t)(byte_rate>>24),
    0x02, 0, 0x10, 0, 'd','a','t','a', (uint8_t)wav_size, (uint8_t)(wav_size>>8), (uint8_t)(wav_size>>16), (uint8_t)(wav_size>>24)
  };
  memcpy(wav_header, header, sizeof(header));
}

bool mountSDMMC() {
  SD_MMC.setPins(39, 38, 40); 
  return SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5);
}

void cleanupOldRecordings() {
  File root = SD_MMC.open("/");
  File file = root.openNextFile();
  while (file) {
    if (String(file.name()).endsWith(".wav")) SD_MMC.remove(file.name());
    file = root.openNextFile();
  }
}