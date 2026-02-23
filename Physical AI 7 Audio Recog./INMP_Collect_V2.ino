// Send Audio data(INMP441) to Processing Processing Receive INMP V2

#include "driver/i2s.h"

#define BUTTON_PIN   1
#define LED_PIN      2
#define I2S_SD_PIN   10
#define I2S_WS_PIN   11
#define I2S_SCK_PIN  12
#define I2S_PORT     I2S_NUM_0

#define SAMPLE_RATE    16000
#define RECORD_MS      1000
#define TOTAL_SAMPLES  (SAMPLE_RATE * RECORD_MS / 1000)  // 16000
#define DMA_READ       512

static int32_t raw[DMA_READ];
static int16_t audio_buf[TOTAL_SAMPLES];

// ── Debounce ──────────────────────────────────────────────────
#define DEBOUNCE_MS 50
static unsigned long lastDebounce = 0;
static bool lastBtnState = HIGH;

static bool buttonJustPressed() {
    bool reading = (digitalRead(BUTTON_PIN) == LOW);
    if (reading != lastBtnState) lastDebounce = millis();
    lastBtnState = reading;
    if ((millis() - lastDebounce) > DEBOUNCE_MS && reading == true) {
        lastBtnState = false;
        lastDebounce = millis();
        return true;
    }
    return false;
}

// ── I2S ───────────────────────────────────────────────────────
void i2s_setup() {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,  // ← matches inference sketch
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags     = 0,
        .dma_buf_count        = 8,
        .dma_buf_len          = 256,
        .use_apll             = false,
        .tx_desc_auto_clear   = false,
        .fixed_mclk           = 0,
    };
    i2s_pin_config_t pins = {
        .bck_io_num   = I2S_SCK_PIN,
        .ws_io_num    = I2S_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = I2S_SD_PIN,
    };
    i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
    i2s_set_pin(I2S_PORT, &pins);
    i2s_zero_dma_buffer(I2S_PORT);
    delay(50);  // let DMA settle
}

void i2s_teardown() {
    i2s_driver_uninstall(I2S_PORT);
}

// ── Record and send ───────────────────────────────────────────
void record_and_send() {
    // Fill buffer
    uint32_t count = 0;
    while (count < TOTAL_SAMPLES) {
        size_t bytes_read = 0;
        i2s_read(I2S_PORT, raw, sizeof(raw), &bytes_read, portMAX_DELAY);
        int n = bytes_read / sizeof(int32_t);
        for (int i = 0; i < n && count < TOTAL_SAMPLES; i++) {
            audio_buf[count++] = (int16_t)(raw[i] >> 14);
        }
    }

    // Send all samples to Processing — one per line
    for (int i = 0; i < TOTAL_SAMPLES; i++) {
        Serial.println(audio_buf[i]);
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Serial.println("# INMP441 Collector ready. Press button to record.");
    Serial.println("# I2S: LEFT ch, >>14 shift, 16000 Hz — matches inference sketch.");
}

void loop() {
    if (buttonJustPressed()) {
        Serial.println("START");
        delay(10);  // delay time for Processing sees START before samples

        digitalWrite(LED_PIN, HIGH);
        i2s_setup();
        record_and_send();
        i2s_teardown();
        digitalWrite(LED_PIN, LOW);

        Serial.println("# Done. Press button for next sample.");
    }
    delay(10);
}
