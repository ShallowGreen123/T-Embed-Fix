#include <Arduino.h>
#include <TFT_eSPI.h>
#include <TEmbedBoard.h>
#include <driver/i2s.h>
#include <math.h>

namespace {

// ---------- layout ----------
constexpr uint8_t  kRotation          = 1;
constexpr int16_t  kUiMargin          = 8;
constexpr int16_t  kHeaderHeight      = 24;
constexpr int16_t  kFooterHeight      = 18;
constexpr uint32_t kBusSettleMs       = 20;
constexpr uint32_t kUiFrameIntervalMs = 33;

constexpr int16_t  kStageCardY = 32;
constexpr int16_t  kStageCardW = 96;
constexpr int16_t  kStageCardH = 28;
constexpr int16_t  kStageGap   = 8;

constexpr int16_t  kPanelX     = 8;
constexpr int16_t  kPanelY     = 68;
constexpr int16_t  kPanelW     = 304;
constexpr int16_t  kPanelH     = 52;

constexpr int16_t  kMetricY    = 128;
constexpr int16_t  kMetricW    = 96;
constexpr int16_t  kMetricH    = 20;

// ---------- I2S ports ----------
constexpr i2s_port_t kMicPort = I2S_NUM_0;
constexpr i2s_port_t kSpkPort = I2S_NUM_1;

// ---------- audio params ----------
constexpr int    kSampleRate   = 16000;
constexpr size_t kBufSamples   = 512;

constexpr uint16_t kMicSignalThreshold = 200;
constexpr uint32_t kMicTestDurationMs  = 5000;
constexpr uint32_t kSpkFreqDwellMs     = 1500;
constexpr uint32_t kLoopbackDurationMs = 8000;

constexpr int16_t kToneAmplitude = 12000;
constexpr float   kToneFreqs[]   = {440.0f, 1000.0f, 880.0f};

// loopback ring buffer (~0.5 s @ 16 kHz)
constexpr size_t kLoopBufSamples = 8192;

// ---------- display theme ----------
constexpr uint16_t kColorBg        = 0x0841;
constexpr uint16_t kColorPanel     = 0x1082;
constexpr uint16_t kColorPanelEdge = 0x31A6;
constexpr uint16_t kColorCard      = 0x18C3;
constexpr uint16_t kColorMeterBg   = 0x2104;
constexpr uint16_t kColorPassBg    = 0x0A41;
constexpr uint16_t kColorFailBg    = 0x3006;

// ---------- state machine ----------
enum class State : uint8_t {
    INIT_MIC = 0,
    MIC_TEST,
    INIT_SPK,
    SPEAKER_TEST,
    INIT_LOOPBACK,
    LOOPBACK_TEST,
    DONE_PASS,
    DONE_FAIL,
};

TEmbedXL9555 ioExpander;
TFT_eSPI     tft;
TFT_eSprite  canvas(&tft);

State    gState         = State::INIT_MIC;
uint32_t gStateEnterMs  = 0;
bool     gScreenDirty   = true;
bool     gCanvasReady   = false;
uint32_t gLastUiDrawMs  = 0;

bool     gMicOk         = false;
bool     gSpkOk         = false;
bool     gLoopOk        = false;
bool     gMicStarted    = false;
bool     gSpkStarted    = false;
bool     gLoopStarted   = false;

uint16_t gVuRms         = 0;
uint16_t gVuPeak        = 0;
uint32_t gVuPeakDecayMs = 0;

uint8_t  gFreqIndex        = 0;
uint16_t gTonePhase        = 0;
uint32_t gFreqLastChangeMs = 0;

int16_t  gLoopBuf[kLoopBufSamples];
size_t   gLoopWrite   = 0;
size_t   gLoopRead    = 0;
bool     gLoopFilled  = false;

// ---------- helpers ----------

void setState(State s) {
    gState        = s;
    gStateEnterMs = millis();
    gScreenDirty  = true;
    gLastUiDrawMs = 0;
}

uint16_t computeRms(const int16_t* buf, size_t n) {
    if (n == 0) return 0;
    int64_t sum = 0;
    for (size_t i = 0; i < n; ++i) {
        int32_t s = buf[i];
        sum += s * s;
    }
    return (uint16_t)sqrt((double)sum / n);
}

void fillTone(int16_t* out, size_t n, float freq, uint16_t& phase) {
    const uint16_t inc = (uint16_t)(freq * 65536.0f / kSampleRate);
    for (size_t i = 0; i < n; ++i) {
        out[i] = (int16_t)(kToneAmplitude * sinf(phase * (2.0f * M_PI / 65536.0f)));
        phase += inc;
    }
}

bool initMic() {
    i2s_config_t cfg = {};
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
    cfg.sample_rate          = kSampleRate;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_PCM_SHORT;
    cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count        = 4;
    cfg.dma_buf_len          = 256;
    cfg.use_apll             = false;
    cfg.tx_desc_auto_clear   = false;
    cfg.fixed_mclk           = 0;

    i2s_pin_config_t pins = {};
    pins.mck_io_num   = I2S_PIN_NO_CHANGE;
    pins.bck_io_num   = I2S_PIN_NO_CHANGE;
    pins.ws_io_num    = BOARD_MIC_CLK;   // PDM clock on ws pin
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num  = BOARD_MIC_DATA;

    if (i2s_driver_install(kMicPort, &cfg, 0, nullptr) != ESP_OK) return false;
    if (i2s_set_pin(kMicPort, &pins) != ESP_OK) {
        i2s_driver_uninstall(kMicPort);
        return false;
    }
    i2s_zero_dma_buffer(kMicPort);
    return true;
}

void deinitMic() {
    i2s_driver_uninstall(kMicPort);
}

bool initSpeaker() {
    i2s_config_t cfg = {};
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate          = kSampleRate;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count        = 8;
    cfg.dma_buf_len          = 256;
    cfg.use_apll             = false;
    cfg.tx_desc_auto_clear   = true;
    cfg.fixed_mclk           = 0;

    i2s_pin_config_t pins = {};
    pins.mck_io_num   = I2S_PIN_NO_CHANGE;
    pins.bck_io_num   = BOARD_VOICE_BCLK;
    pins.ws_io_num    = BOARD_VOICE_LRCLK;
    pins.data_out_num = BOARD_VOICE_DIN;
    pins.data_in_num  = I2S_PIN_NO_CHANGE;

    if (i2s_driver_install(kSpkPort, &cfg, 0, nullptr) != ESP_OK) return false;
    if (i2s_set_pin(kSpkPort, &pins) != ESP_OK) {
        i2s_driver_uninstall(kSpkPort);
        return false;
    }
    i2s_zero_dma_buffer(kSpkPort);
    // Enable amp after I2S clock is running to avoid click
    t_embed::board::setAudioAmplifierEnabled(ioExpander, true);
    return true;
}

void deinitSpeaker() {
    t_embed::board::setAudioAmplifierEnabled(ioExpander, false);
    i2s_zero_dma_buffer(kSpkPort);
    i2s_driver_uninstall(kSpkPort);
}

// ---------- display ----------

#include "mic_speaker_ui.h"

void redrawScreen() {
    if (gCanvasReady) {
        drawUi(canvas);
        canvas.pushSprite(0, 0);
        return;
    }

    drawUi(tft);
}

// ---------- board init ----------

bool initDisplayPower() {
    t_embed::board::deselectSharedSpiDevices();

    if (!t_embed::board::beginExpander(ioExpander)) {
        Serial.println(F("[AUD] XL9555 init failed."));
        return false;
    }
    if (!t_embed::board::setLowPowerEnabled(ioExpander, true)) {
        Serial.println(F("[AUD] Failed to enable LOW_PWR_3V3."));
        return false;
    }
    delay(kBusSettleMs);

    pinMode(BOARD_LCD_BL, OUTPUT);
    digitalWrite(BOARD_LCD_BL, HIGH);

    if (!t_embed::board::setLcdReset(ioExpander, true))  return false;
    delay(5);
    if (!t_embed::board::setLcdReset(ioExpander, false)) return false;
    delay(20);
    if (!t_embed::board::setLcdReset(ioExpander, true))  return false;
    delay(120);
    return true;
}

// ---------- state machine ----------

void resetTestState() {
    gMicOk        = false;
    gSpkOk        = false;
    gLoopOk       = false;
    gMicStarted   = false;
    gSpkStarted   = false;
    gLoopStarted  = false;
    gVuRms        = 0;
    gVuPeak       = 0;
    gVuPeakDecayMs = 0;
    gFreqIndex    = 0;
    gTonePhase    = 0;
    gLoopWrite    = 0;
    gLoopRead     = 0;
    gLoopFilled   = false;
    setState(State::INIT_MIC);
}

void updateStateMachine() {
    const uint32_t now = millis();

    switch (gState) {

    case State::INIT_MIC:
        gMicStarted    = true;
        gVuRms         = 0;
        gVuPeak        = 0;
        gVuPeakDecayMs = now;
        Serial.println(F("[AUD] Starting mic test (5 s — make some noise)..."));
        if (!initMic()) {
            Serial.println(F("[AUD] Mic I2S init failed."));
            setState(State::DONE_FAIL);
        } else {
            setState(State::MIC_TEST);
        }
        break;

    case State::MIC_TEST: {
        int16_t buf[kBufSamples];
        size_t bytesRead = 0;
        i2s_read(kMicPort, buf, sizeof(buf), &bytesRead, 0);
        const size_t n = bytesRead / sizeof(int16_t);
        if (n > 0) {
            const uint16_t rms = computeRms(buf, n);
            gVuRms = rms;
            if (rms > gVuPeak) { gVuPeak = rms; gVuPeakDecayMs = now; }
            if (now - gVuPeakDecayMs > 800) { gVuPeak = (gVuPeak * 15) / 16; }
            if (rms > kMicSignalThreshold) gMicOk = true;
            gScreenDirty = true;
        }
        if (now - gStateEnterMs >= kMicTestDurationMs) {
            deinitMic();
            Serial.print(F("[AUD] Mic test "));
            Serial.println(gMicOk ? F("PASS") : F("FAIL (no signal)"));
            setState(State::INIT_SPK);
        }
        break;
    }

    case State::INIT_SPK:
        gSpkStarted      = true;
        gVuRms           = 0;
        gVuPeak          = 0;
        gVuPeakDecayMs   = now;
        Serial.println(F("[AUD] Starting speaker test (3 tones)..."));
        gFreqIndex       = 0;
        gTonePhase       = 0;
        gFreqLastChangeMs = now;
        if (!initSpeaker()) {
            Serial.println(F("[AUD] Speaker I2S init failed."));
            gSpkOk = false;
            setState(State::INIT_LOOPBACK);
        } else {
            gSpkOk = true;  // pass if TX installs OK; user can hear it
            setState(State::SPEAKER_TEST);
        }
        break;

    case State::SPEAKER_TEST: {
        int16_t buf[kBufSamples];
        fillTone(buf, kBufSamples, kToneFreqs[gFreqIndex], gTonePhase);
        size_t bytesWritten = 0;
        i2s_write(kSpkPort, buf, sizeof(buf), &bytesWritten, 10);

        if (now - gFreqLastChangeMs >= kSpkFreqDwellMs) {
            gFreqIndex = (gFreqIndex + 1) % 3;
            gFreqLastChangeMs = now;
            gScreenDirty = true;
            Serial.print(F("[AUD] Tone -> "));
            Serial.print(kToneFreqs[gFreqIndex]);
            Serial.println(F(" Hz"));
        }

        if (now - gStateEnterMs >= kSpkFreqDwellMs * 3) {
            deinitSpeaker();
            Serial.println(F("[AUD] Speaker test done."));
            setState(State::INIT_LOOPBACK);
        }
        break;
    }

    case State::INIT_LOOPBACK:
        gLoopStarted   = true;
        gVuPeakDecayMs = now;
        Serial.println(F("[AUD] Starting loopback test (8 s — speak into mic)..."));
        gLoopWrite  = 0;
        gLoopRead   = 0;
        gLoopFilled = false;
        gVuRms      = 0;
        gVuPeak     = 0;
        if (!initMic()) {
            Serial.println(F("[AUD] Loopback mic init failed."));
            setState(State::DONE_FAIL);
            break;
        }
        if (!initSpeaker()) {
            Serial.println(F("[AUD] Loopback speaker init failed."));
            deinitMic();
            setState(State::DONE_FAIL);
            break;
        }
        setState(State::LOOPBACK_TEST);
        break;

    case State::LOOPBACK_TEST: {
        // Read mic into ring buffer
        int16_t inBuf[kBufSamples];
        size_t bytesRead = 0;
        i2s_read(kMicPort, inBuf, sizeof(inBuf), &bytesRead, 0);
        const size_t inN = bytesRead / sizeof(int16_t);
        for (size_t i = 0; i < inN; ++i) {
            gLoopBuf[gLoopWrite] = inBuf[i];
            gLoopWrite = (gLoopWrite + 1) % kLoopBufSamples;
            if (gLoopWrite == 0) gLoopFilled = true;
        }
        if (inN > 0) {
            gVuRms = computeRms(inBuf, inN);
            if (gVuRms > gVuPeak) { gVuPeak = gVuRms; gVuPeakDecayMs = now; }
            if (now - gVuPeakDecayMs > 800) { gVuPeak = (gVuPeak * 15) / 16; }
            if (gVuRms > kMicSignalThreshold) gLoopOk = true;
            gScreenDirty = true;
        }

        // Play back from ring buffer once it has enough data
        if (gLoopFilled) {
            int16_t outBuf[kBufSamples];
            for (size_t i = 0; i < kBufSamples; ++i) {
                outBuf[i] = gLoopBuf[gLoopRead];
                gLoopRead = (gLoopRead + 1) % kLoopBufSamples;
            }
            size_t bytesWritten = 0;
            i2s_write(kSpkPort, outBuf, sizeof(outBuf), &bytesWritten, 10);
        }

        if (now - gStateEnterMs >= kLoopbackDurationMs) {
            deinitMic();
            deinitSpeaker();
            Serial.print(F("[AUD] Loopback test "));
            Serial.println(gLoopOk ? F("PASS") : F("FAIL (no mic signal)"));
            const bool allPass = gMicOk && gSpkOk && gLoopOk;
            setState(allPass ? State::DONE_PASS : State::DONE_FAIL);
        }
        break;
    }

    case State::DONE_PASS:
    case State::DONE_FAIL:
        break;
    }
}

void pollUserKey() {
    static bool lastPressed = false;
    const bool pressed = (digitalRead(BOARD_USER_KEY) == LOW);
    if (pressed && !lastPressed) {
        if (gState == State::DONE_PASS || gState == State::DONE_FAIL) {
            Serial.println(F("[AUD] Restarting test..."));
            resetTestState();
        }
    }
    lastPressed = pressed;
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println(F("\nT-Embed Mic & Speaker Test"));

    if (!initDisplayPower()) {
        Serial.println(F("[AUD] Board init failed - halting."));
        while (true) { delay(1000); }
    }

    tft.init();
    tft.setRotation(kRotation);
    tft.fillScreen(TFT_BLACK);
    t_embed::board::deselectSharedSpiDevices();

    canvas.setColorDepth(16);
    gCanvasReady = (canvas.createSprite(tft.width(), tft.height()) != nullptr);
    if (!gCanvasReady) {
        Serial.println(F("[AUD] Sprite allocation failed, using direct TFT redraw."));
    }

    pinMode(BOARD_USER_KEY, INPUT_PULLUP);

    resetTestState();
    redrawScreen();
    gScreenDirty  = false;
    gLastUiDrawMs = millis();
}

void loop() {
    pollUserKey();
    updateStateMachine();

    if (gState == State::SPEAKER_TEST && millis() - gLastUiDrawMs >= kUiFrameIntervalMs) {
        gScreenDirty = true;
    }

    if (gScreenDirty && (gLastUiDrawMs == 0 || millis() - gLastUiDrawMs >= kUiFrameIntervalMs)) {
        redrawScreen();
        gScreenDirty  = false;
        gLastUiDrawMs = millis();
    }

    delay(2);
}
