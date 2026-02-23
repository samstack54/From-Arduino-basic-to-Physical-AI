/* ============================================================
 *  Speech Inference — Pre-roll Buffer (Reaction Time Fix)
 *  Board  : ESP32-S3 Freenove WROOM
 *  Mic    : INMP441 — SD=10, WS=11, SCK=12 (I2S_NUM_0)
 *  Button : GPIO 1, active-LOW (internal pull-up)
 *
 *  PROBLEM SOLVED:
 *  Training data: word starts at ~0ms (LED triggers instantly)
 *  Inference:     human reaction adds 100-300ms delay after LED
 *  Result:        word is shifted late → model misclassifies
 *
 *  FIX — Pre-roll ring buffer:
 *  Mic runs CONTINUOUSLY in background.
 *  When button pressed, we already have 300ms of audio buffered.
 *  LED turns on, you speak, we capture remaining 700ms.
 *  Final 1-second window = 300ms pre-roll + 700ms post-press.
 *  Your word lands at ~300ms regardless of reaction time.
 *
 *  IMPORTANT: Also update your collection sketch to match!
 *  Change INMP_Collect_Fixed.ino to use 300ms pre-roll too,
 *  then recollect training data so timings match.
 *  OR: keep collection as-is but set PRE_ROLL_MS = 0 here
 *  and just speak faster after the LED turns on.
 * ============================================================ */

#define EIDSP_QUANTIZE_FILTERBANK 0

#include <ESP32_Audio_inferencing.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/i2s.h"
#include <math.h>

// ╔══════════════════════════════════════════════════════════════╗
// ║  Tune this to match your typical reaction time after LED on  ║
// ║  0   = word must start immediately (original behaviour)      ║
// ║  200 = 200ms pre-roll  (good starting point)                 ║
// ║  300 = 300ms pre-roll  (if you need a bit more time)         ║
#define PRE_ROLL_MS   200
// ╚══════════════════════════════════════════════════════════════╝

// ── Pin definitions ──────────────────────────────────────────
#define BUTTON_PIN      1
#define LED_PIN         2
#define I2S_SD_PIN      10
#define I2S_WS_PIN      11
#define I2S_SCK_PIN     12
#define I2S_PORT        I2S_NUM_0

// ── Sizes ─────────────────────────────────────────────────────
static const uint32_t SAMPLE_RATE    = EI_CLASSIFIER_FREQUENCY;
static const uint32_t MODEL_SAMPLES  = EI_CLASSIFIER_RAW_SAMPLE_COUNT;  // 16000
static const uint32_t PRE_ROLL_SAMP  = (SAMPLE_RATE * PRE_ROLL_MS) / 1000;
static const uint32_t POST_SAMP      = MODEL_SAMPLES - PRE_ROLL_SAMP;
// Ring buffer holds enough audio for one full pre-roll
static const uint32_t RING_SIZE      = PRE_ROLL_SAMP + 512;  // small extra headroom

#define DMA_READ_SAMPLES 256

// ── Buffers ───────────────────────────────────────────────────
static int16_t *ring_buffer  = nullptr;  // circular pre-roll buffer
static int16_t *post_buffer  = nullptr;  // samples captured after button press
static int16_t *model_buffer = nullptr;  // final MODEL_SAMPLES fed to classifier

static volatile uint32_t ring_head   = 0;   // write position in ring
static volatile uint32_t post_count  = 0;   // samples captured post-press
static volatile bool      capturing  = false; // true after button pressed
static volatile bool      rec_done   = false;

static bool debug_nn = false;

// ── State machine ─────────────────────────────────────────────
enum State { IDLE, WARMUP, LISTENING, CLASSIFYING };
static volatile State appState = IDLE;

// ── Debounce ─────────────────────────────────────────────────
#define DEBOUNCE_MS 50
static unsigned long lastDebounceTime = 0;
static bool          lastButtonState  = HIGH;

static bool buttonJustPressed() {
    bool reading = (digitalRead(BUTTON_PIN) == LOW);
    if (reading != lastButtonState) lastDebounceTime = millis();
    lastButtonState = reading;
    if ((millis() - lastDebounceTime) > DEBOUNCE_MS && reading == true) {
        lastButtonState = false; lastDebounceTime = millis();
        return true;
    }
    return false;
}

// ── I2S ───────────────────────────────────────────────────────
static int i2s_init(uint32_t rate) {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = rate,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
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
    esp_err_t r = i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
    if (r != ESP_OK) { ei_printf("i2s_driver_install failed: %d\n", r); return 1; }
    r = i2s_set_pin(I2S_PORT, &pins);
    if (r != ESP_OK) { ei_printf("i2s_set_pin failed: %d\n", r); return 1; }
    i2s_zero_dma_buffer(I2S_PORT);
    return 0;
}

static void i2s_deinit() { i2s_driver_uninstall(I2S_PORT); }

// ── Continuous capture task ───────────────────────────────────
// Runs the whole time from WARMUP through LISTENING.
// Phase 1 (capturing=false): fills ring buffer continuously
// Phase 2 (capturing=true):  fills post_buffer until POST_SAMP reached
static void capture_task(void *arg) {
    int32_t raw[DMA_READ_SAMPLES];

    while (true) {
        size_t bytes_read = 0;
        i2s_read(I2S_PORT, raw, sizeof(raw), &bytes_read, portMAX_DELAY);
        int n = bytes_read / sizeof(int32_t);

        for (int i = 0; i < n; i++) {
            int16_t sample = (int16_t)(raw[i] >> 14);

            if (!capturing) {
                // Pre-roll phase: keep writing into ring buffer (overwrites old)
                ring_buffer[ring_head % RING_SIZE] = sample;
                ring_head++;
            } else {
                // Post-press phase: fill post_buffer
                if (post_count < POST_SAMP) {
                    post_buffer[post_count++] = sample;
                }
                if (post_count >= POST_SAMP) {
                    rec_done = true;
                    goto done;
                }
            }
        }
    }

done:
    vTaskDelete(NULL);
}

// ── Build final model buffer from pre-roll + post ─────────────
static void assemble_model_buffer() {
    // Copy PRE_ROLL_SAMP samples from ring buffer (oldest first)
    // ring_head points to next write position, so oldest = ring_head % RING_SIZE
    uint32_t start = ring_head % RING_SIZE;
    for (uint32_t i = 0; i < PRE_ROLL_SAMP; i++) {
        model_buffer[i] = ring_buffer[(start + i) % RING_SIZE];
    }
    // Copy post-press samples
    memcpy(&model_buffer[PRE_ROLL_SAMP], post_buffer, POST_SAMP * sizeof(int16_t));
}

// ── Audio signal callback ─────────────────────────────────────
static int audio_signal_get_data(size_t offset, size_t length, float *out) {
    numpy::int16_to_float(&model_buffer[offset], out, length);
    return 0;
}

// ── RMS ───────────────────────────────────────────────────────
static float compute_rms(int16_t *buf, uint32_t len) {
    double sum = 0;
    for (uint32_t i = 0; i < len; i++) sum += (double)buf[i] * buf[i];
    return sqrtf((float)(sum / len));
}

// ── Run classifier ────────────────────────────────────────────
static void run_inference() {
    assemble_model_buffer();

    float rms = compute_rms(model_buffer, MODEL_SAMPLES);
    Serial.printf("  Audio RMS: %.1f", rms);
    if      (rms < 100)   Serial.println(" ← WARNING: Very quiet.");
    else if (rms > 30000) Serial.println(" ← WARNING: Clipping.");
    else                  Serial.println(" ← Good");

    signal_t signal;
    signal.total_length = MODEL_SAMPLES;
    signal.get_data     = &audio_signal_get_data;

    ei_impulse_result_t result = {0};
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
    if (err != EI_IMPULSE_OK) {
        ei_printf("ERR: Classifier failed (%d)\n", err);
        return;
    }

    Serial.println("\n========== INFERENCE RESULT ==========");
    Serial.printf("Timing — DSP: %d ms | Classification: %d ms\n",
                  result.timing.dsp, result.timing.classification);

    float       best_val   = -1.0f;
    const char *best_label = "";
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        float v = result.classification[i].value;
        Serial.printf("  %-20s %.4f\n", result.classification[i].label, v);
        if (v > best_val) { best_val = v; best_label = result.classification[i].label; }
    }
#if EI_CLASSIFIER_HAS_ANOMALY == 1
    Serial.printf("  %-20s %.4f\n", "anomaly", result.anomaly);
#endif
    Serial.printf(">> Predicted: \"%s\"  (%.1f%%)\n", best_label, best_val * 100.0f);
    Serial.println("======================================\n");
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    while (!Serial);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Allocate buffers
    ring_buffer  = (int16_t *)malloc(RING_SIZE     * sizeof(int16_t));
    post_buffer  = (int16_t *)malloc(POST_SAMP     * sizeof(int16_t));
    model_buffer = (int16_t *)malloc(MODEL_SAMPLES * sizeof(int16_t));

    if (!ring_buffer || !post_buffer || !model_buffer) {
        Serial.println("FATAL: Cannot allocate buffers. Halting.");
        while (1) delay(1000);
    }

    memset(ring_buffer,  0, RING_SIZE     * sizeof(int16_t));
    memset(post_buffer,  0, POST_SAMP     * sizeof(int16_t));
    memset(model_buffer, 0, MODEL_SAMPLES * sizeof(int16_t));

    Serial.println("=== Speech Inference — Pre-roll Buffer ===");
    Serial.printf("Model    : %d classes | %lu ms | %d Hz\n",
                  EI_CLASSIFIER_LABEL_COUNT,
                  (unsigned long)(MODEL_SAMPLES * 1000 / SAMPLE_RATE),
                  SAMPLE_RATE);
    Serial.printf("Pre-roll : %d ms (mic warms up before button press)\n", PRE_ROLL_MS);
    Serial.printf("Window   : [-%d ms ... 0 ... +%lu ms]\n",
                  PRE_ROLL_MS,
                  (unsigned long)(POST_SAMP * 1000 / SAMPLE_RATE));
    Serial.println("\nStarting mic warmup...");

    // Start I2S and capture task immediately so ring buffer fills
    if (i2s_init(SAMPLE_RATE) != 0) {
        Serial.println("FATAL: I2S init failed.");
        while (1) delay(1000);
    }

    appState  = WARMUP;
    capturing = false;
    rec_done  = false;
    ring_head = 0;

    xTaskCreatePinnedToCore(
        capture_task, "CaptureTask",
        1024 * 32, NULL, 10, NULL, 1
    );

    // Wait for ring buffer to fill with PRE_ROLL_SAMP samples
    uint32_t warmup_ms = (PRE_ROLL_MS * 12) / 10;  // 20% extra
    Serial.printf("Warming up for %lu ms...\n", (unsigned long)warmup_ms);
    delay(warmup_ms);

    appState = IDLE;
    Serial.println("Ready! Press button — you have time to react.\n");
}

// ── Main loop ─────────────────────────────────────────────────
void loop() {
    switch (appState) {

        case IDLE:
            if (buttonJustPressed()) {
                // Snapshot ring buffer position BEFORE capturing starts
                // (ring_head is already pointing past PRE_ROLL_SAMP of good audio)
                post_count = 0;
                rec_done   = false;
                memset(post_buffer, 0, POST_SAMP * sizeof(int16_t));

                capturing = true;  // capture task switches to post-press mode
                digitalWrite(LED_PIN, HIGH);
                Serial.println("● LED ON — say your word now!");
                appState = LISTENING;
            }
            break;

        case LISTENING:
            if (rec_done) {
                appState  = CLASSIFYING;
                capturing = false;
                digitalWrite(LED_PIN, LOW);

                // Stop and restart I2S for next round
                i2s_deinit();

                Serial.println("◉ Classifying...");
                run_inference();

                // Restart mic for next press
                memset(ring_buffer, 0, RING_SIZE * sizeof(int16_t));
                ring_head  = 0;
                rec_done   = false;
                capturing  = false;

                if (i2s_init(SAMPLE_RATE) != 0) {
                    Serial.println("ERR: I2S restart failed.");
                    while (1) delay(1000);
                }

                xTaskCreatePinnedToCore(
                    capture_task, "CaptureTask",
                    1024 * 32, NULL, 10, NULL, 1
                );

                appState = WARMUP;
                delay((PRE_ROLL_MS * 12) / 10);  // re-fill ring buffer
                appState = IDLE;
                Serial.println("Ready! Press button — you have time to react.\n");
            }
            break;

        case WARMUP:
        case CLASSIFYING:
            break;
    }

    delay(5);
}
