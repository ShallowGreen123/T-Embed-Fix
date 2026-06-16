#pragma once
#include <TFT_eSPI.h>
#include <driver/i2s.h>
#include <math.h>

namespace page_mic {

namespace {

constexpr int16_t  kUiMargin          = 8;
constexpr int16_t  kHeaderHeight      = 24;
constexpr int16_t  kFooterHeight      = 18;
constexpr uint32_t kUiFrameIntervalMs = 33;
constexpr uint32_t kSpeakerUiFrameIntervalMs = 120;

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

constexpr int16_t  kBackBtnW   = 58;
constexpr int16_t  kBackBtnH   = 14;

constexpr i2s_port_t kMicPort = I2S_NUM_0;
constexpr i2s_port_t kSpkPort = I2S_NUM_1;

constexpr int    kSampleRate   = 16000;
constexpr size_t kBufSamples   = 512;

constexpr uint16_t kMicSignalThreshold = 200;
constexpr uint32_t kMicTestDurationMs  = 5000;
constexpr uint32_t kSpkFreqDwellMs     = 1500;
constexpr uint32_t kLoopbackDurationMs = 8000;

constexpr int16_t kToneAmplitude = 12000;
constexpr float   kToneFreqs[]   = {440.0f, 1000.0f, 880.0f};
constexpr size_t  kSpkChunkSamples = 1024;
constexpr uint8_t kSpkWarmupChunks = 2;

constexpr size_t kLoopBufSamples = 8192;

constexpr uint16_t kColorBg        = 0x0841;
constexpr uint16_t kColorPanel     = 0x1082;
constexpr uint16_t kColorPanelEdge = 0x31A6;
constexpr uint16_t kColorCard      = 0x18C3;
constexpr uint16_t kColorMeterBg   = 0x2104;
constexpr uint16_t kColorPassBg    = 0x0A41;
constexpr uint16_t kColorFailBg    = 0x3006;

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

enum class FocusItem : uint8_t {
    Controls = 0,
    Back,
    kCount,
};

enum class StageStatus : uint8_t {
    PENDING = 0,
    ACTIVE,
    PASS,
    FAIL,
};

struct StageStyle {
    uint16_t    fill;
    uint16_t    border;
    uint16_t    label;
    uint16_t    tag;
    const char* tagText;
};

State       gState         = State::INIT_MIC;
FocusItem   gFocus         = FocusItem::Controls;
uint32_t    gStateEnterMs  = 0;
uint32_t    gLastUiDrawMs  = 0;
bool        gScreenDirty   = true;
bool        gCanvasReady   = false;
bool        gMicDriverOk   = false;
bool        gSpkDriverOk   = false;
bool        gMicOk         = false;
bool        gSpkOk         = false;
bool        gLoopOk        = false;
bool        gMicStarted    = false;
bool        gSpkStarted    = false;
bool        gLoopStarted   = false;
uint16_t    gVuRms         = 0;
uint16_t    gVuPeak        = 0;
uint32_t    gVuPeakDecayMs = 0;
uint8_t     gFreqIndex     = 0;
uint16_t    gTonePhase     = 0;
uint32_t    gFreqLastChangeMs = 0;
int32_t     gEncSnapshot   = 0;
int16_t     gLoopBuf[kLoopBufSamples];
size_t      gLoopWrite     = 0;
size_t      gLoopRead      = 0;
bool        gLoopFilled    = false;
TFT_eSprite gCanvas(&tft);

void markDirty()
{
    gScreenDirty = true;
}

void setState(const State s)
{
    gState = s;
    gStateEnterMs = millis();
    gLastUiDrawMs = 0;
    markDirty();
}

uint16_t computeRms(const int16_t* buf, const size_t n)
{
    if (n == 0) {
        return 0;
    }

    int64_t sum = 0;
    for (size_t i = 0; i < n; ++i) {
        const int32_t s = buf[i];
        sum += s * s;
    }
    return static_cast<uint16_t>(sqrt(static_cast<double>(sum) / n));
}

void fillTone(int16_t* out, const size_t n, const float freq, uint16_t& phase)
{
    const uint16_t inc = static_cast<uint16_t>(freq * 65536.0f / kSampleRate);
    for (size_t i = 0; i < n; ++i) {
        out[i] = static_cast<int16_t>(
            kToneAmplitude * sinf(phase * (2.0f * static_cast<float>(M_PI) / 65536.0f)));
        phase += inc;
    }
}

void writeSpeakerSamples(const int16_t* samples, const size_t count)
{
    if (!gSpkDriverOk || count == 0) {
        return;
    }

    const uint8_t* data = reinterpret_cast<const uint8_t*>(samples);
    size_t remaining = count * sizeof(int16_t);
    while (remaining > 0) {
        size_t bytesWritten = 0;
        i2s_write(kSpkPort, data, remaining, &bytesWritten, portMAX_DELAY);
        if (bytesWritten == 0) {
            break;
        }
        data += bytesWritten;
        remaining -= bytesWritten;
    }
}

void writeToneChunk()
{
    int16_t buf[kSpkChunkSamples];
    fillTone(buf, kSpkChunkSamples, kToneFreqs[gFreqIndex], gTonePhase);
    writeSpeakerSamples(buf, kSpkChunkSamples);
}

bool initMic()
{
    if (gMicDriverOk) {
        return true;
    }

    i2s_config_t cfg = {};
    cfg.mode                 = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
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
    pins.ws_io_num    = BOARD_MIC_CLK;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num  = BOARD_MIC_DATA;

    if (i2s_driver_install(kMicPort, &cfg, 0, nullptr) != ESP_OK) {
        return false;
    }
    if (i2s_set_pin(kMicPort, &pins) != ESP_OK) {
        i2s_driver_uninstall(kMicPort);
        return false;
    }

    i2s_zero_dma_buffer(kMicPort);
    gMicDriverOk = true;
    return true;
}

void deinitMic()
{
    if (!gMicDriverOk) {
        return;
    }

    i2s_driver_uninstall(kMicPort);
    gMicDriverOk = false;
}

bool initSpeaker()
{
    if (gSpkDriverOk) {
        return true;
    }

    i2s_config_t cfg = {};
    cfg.mode                 = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
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

    if (i2s_driver_install(kSpkPort, &cfg, 0, nullptr) != ESP_OK) {
        return false;
    }
    if (i2s_set_pin(kSpkPort, &pins) != ESP_OK) {
        i2s_driver_uninstall(kSpkPort);
        return false;
    }

    i2s_zero_dma_buffer(kSpkPort);
    t_embed::board::setAudioAmplifierEnabled(ioExpander, true);
    gSpkDriverOk = true;
    return true;
}

void deinitSpeaker()
{
    if (!gSpkDriverOk) {
        return;
    }

    t_embed::board::setAudioAmplifierEnabled(ioExpander, false);
    i2s_zero_dma_buffer(kSpkPort);
    i2s_driver_uninstall(kSpkPort);
    gSpkDriverOk = false;
}

void stopAllAudio()
{
    deinitMic();
    deinitSpeaker();
}

void resetTestState()
{
    stopAllAudio();

    gMicOk          = false;
    gSpkOk          = false;
    gLoopOk         = false;
    gMicStarted     = false;
    gSpkStarted     = false;
    gLoopStarted    = false;
    gVuRms          = 0;
    gVuPeak         = 0;
    gVuPeakDecayMs  = 0;
    gFreqIndex      = 0;
    gTonePhase      = 0;
    gFreqLastChangeMs = 0;
    gLoopWrite      = 0;
    gLoopRead       = 0;
    gLoopFilled     = false;

    setState(State::INIT_MIC);
}

uint16_t stateAccent()
{
    switch (gState) {
        case State::INIT_MIC:
        case State::MIC_TEST:
            return TFT_CYAN;
        case State::INIT_SPK:
        case State::SPEAKER_TEST:
            return TFT_YELLOW;
        case State::INIT_LOOPBACK:
        case State::LOOPBACK_TEST:
            return TFT_ORANGE;
        case State::DONE_PASS:
            return TFT_GREEN;
        case State::DONE_FAIL:
            return TFT_RED;
        default:
            return TFT_DARKGREY;
    }
}

const char* stateTitle()
{
    switch (gState) {
        case State::INIT_MIC:
            return "Initializing microphone";
        case State::MIC_TEST:
            return "Microphone capture test";
        case State::INIT_SPK:
            return "Preparing speaker output";
        case State::SPEAKER_TEST:
            return "Speaker tone sweep";
        case State::INIT_LOOPBACK:
            return "Configuring loopback";
        case State::LOOPBACK_TEST:
            return "Mic to speaker loopback";
        case State::DONE_PASS:
            return "Audio self-check passed";
        case State::DONE_FAIL:
            return "Audio self-check failed";
        default:
            return "Audio self-check";
    }
}

const char* stateHint()
{
    switch (gState) {
        case State::INIT_MIC:
            return "Starting the PDM mic path...";
        case State::MIC_TEST:
            return "Make some noise close to the microphone.";
        case State::INIT_SPK:
            return "Enabling I2S and amplifier...";
        case State::SPEAKER_TEST:
            return "Listen for the three test tones.";
        case State::INIT_LOOPBACK:
            return "Bringing mic and speaker online together...";
        case State::LOOPBACK_TEST:
            return "Speak now. You should hear delayed playback.";
        case State::DONE_PASS:
            return "All three stages completed successfully.";
        case State::DONE_FAIL:
            return "Check mic, speaker and power path, then rerun.";
        default:
            return "";
    }
}

const char* footerHint()
{
    if (gState == State::DONE_PASS || gState == State::DONE_FAIL) {
        return (gFocus == FocusItem::Back)
            ? "BOOT=back  USR=rerun"
            : "USR=rerun  turn=BACK";
    }

    return (gFocus == FocusItem::Back)
        ? "BOOT=back"
        : "Testing...  turn=BACK";
}

uint32_t activeStageDurationMs()
{
    switch (gState) {
        case State::MIC_TEST:
            return kMicTestDurationMs;
        case State::SPEAKER_TEST:
            return kSpkFreqDwellMs * 3;
        case State::LOOPBACK_TEST:
            return kLoopbackDurationMs;
        default:
            return 0;
    }
}

uint32_t activeStageElapsedMs()
{
    const uint32_t duration = activeStageDurationMs();
    if (duration == 0) {
        return 0;
    }

    const uint32_t elapsed = millis() - gStateEnterMs;
    return elapsed > duration ? duration : elapsed;
}

bool hasInputMeter()
{
    return gState == State::MIC_TEST ||
           gState == State::LOOPBACK_TEST ||
           gState == State::DONE_PASS ||
           gState == State::DONE_FAIL;
}

StageStatus micStageStatus()
{
    if (gState == State::INIT_MIC || gState == State::MIC_TEST) {
        return StageStatus::ACTIVE;
    }
    if (!gMicStarted) {
        return StageStatus::PENDING;
    }
    return gMicOk ? StageStatus::PASS : StageStatus::FAIL;
}

StageStatus spkStageStatus()
{
    if (gState == State::INIT_SPK || gState == State::SPEAKER_TEST) {
        return StageStatus::ACTIVE;
    }
    if (!gSpkStarted) {
        return StageStatus::PENDING;
    }
    return gSpkOk ? StageStatus::PASS : StageStatus::FAIL;
}

StageStatus loopStageStatus()
{
    if (gState == State::INIT_LOOPBACK || gState == State::LOOPBACK_TEST) {
        return StageStatus::ACTIVE;
    }
    if (!gLoopStarted) {
        return StageStatus::PENDING;
    }
    return gLoopOk ? StageStatus::PASS : StageStatus::FAIL;
}

StageStyle stageStyle(const StageStatus status, const uint16_t accent)
{
    switch (status) {
        case StageStatus::ACTIVE:
            return {kColorCard, accent, TFT_WHITE, accent, "RUN"};
        case StageStatus::PASS:
            return {kColorPassBg, TFT_GREEN, TFT_WHITE, TFT_GREEN, "PASS"};
        case StageStatus::FAIL:
            return {kColorFailBg, TFT_RED, TFT_WHITE, TFT_RED, "FAIL"};
        case StageStatus::PENDING:
        default:
            return {kColorCard, kColorPanelEdge, TFT_LIGHTGREY, TFT_DARKGREY, "WAIT"};
    }
}

uint16_t meterBarColor(const uint16_t width, const uint16_t fullWidth)
{
    if (width < fullWidth * 60 / 100) {
        return TFT_GREEN;
    }
    if (width < fullWidth * 85 / 100) {
        return TFT_YELLOW;
    }
    return TFT_RED;
}

const char* levelLabel(const uint16_t rms)
{
    if (rms < 100) {
        return "SILENT";
    }
    if (rms < 500) {
        return "LOW";
    }
    if (rms < 2000) {
        return "MED";
    }
    return "LOUD";
}

uint16_t levelColor(const uint16_t rms)
{
    if (rms < 100) {
        return TFT_DARKGREY;
    }
    if (rms < 500) {
        return TFT_CYAN;
    }
    if (rms < 2000) {
        return TFT_GREEN;
    }
    return TFT_RED;
}

template <typename Canvas>
void drawBackButton(Canvas& gfx, const bool selected)
{
    const int16_t x = gfx.width() - kBackBtnW - 6;
    const int16_t y = gfx.height() - kFooterHeight + 2;
    const uint16_t bg = selected ? TFT_WHITE : 0x2104;
    const uint16_t fg = selected ? TFT_BLACK : TFT_LIGHTGREY;

    gfx.fillRoundRect(x, y, kBackBtnW, kBackBtnH, 5, bg);
    gfx.drawRoundRect(x, y, kBackBtnW, kBackBtnH, 5,
                      selected ? TFT_YELLOW : TFT_DARKGREY);
    gfx.setTextColor(fg, bg);
    gfx.drawCentreString("BACK", x + kBackBtnW / 2, y + 3, 1);
}

template <typename Canvas>
void drawHeader(Canvas& gfx)
{
    gfx.fillRect(0, 0, gfx.width(), kHeaderHeight, TFT_NAVY);
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_WHITE, TFT_NAVY);
    gfx.drawString("Mic & Speaker Test", kUiMargin, 5, 2);
    gfx.setTextDatum(TR_DATUM);
    gfx.setTextColor(TFT_CYAN, TFT_NAVY);
    gfx.drawString("16 kHz audio", gfx.width() - kUiMargin, 7, 1);
    gfx.setTextDatum(TL_DATUM);
}

template <typename Canvas>
void drawFooter(Canvas& gfx)
{
    const int16_t y = gfx.height() - kFooterHeight;
    gfx.fillRect(0, y, gfx.width(), kFooterHeight, TFT_DARKGREY);
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_WHITE, TFT_DARKGREY);
    gfx.drawString(footerHint(), kUiMargin, y + 4, 1);
    drawBackButton(gfx, gFocus == FocusItem::Back);
}

template <typename Canvas>
void drawStageCard(Canvas& gfx, const int16_t x, const char* label,
                   const StageStatus status, const uint16_t accent)
{
    const StageStyle style = stageStyle(status, accent);
    gfx.fillRoundRect(x, kStageCardY, kStageCardW, kStageCardH, 6, style.fill);
    gfx.drawRoundRect(x, kStageCardY, kStageCardW, kStageCardH, 6, style.border);
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(style.label, style.fill);
    gfx.drawString(label, x + 8, kStageCardY + 6, 2);
    gfx.setTextDatum(TR_DATUM);
    gfx.setTextColor(style.tag, style.fill);
    gfx.drawString(style.tagText, x + kStageCardW - 8, kStageCardY + 9, 1);
    gfx.setTextDatum(TL_DATUM);
}

template <typename Canvas>
void drawStageRow(Canvas& gfx)
{
    drawStageCard(gfx, kUiMargin, "MIC", micStageStatus(), TFT_CYAN);
    drawStageCard(gfx, kUiMargin + kStageCardW + kStageGap, "SPK", spkStageStatus(), TFT_YELLOW);
    drawStageCard(gfx, kUiMargin + (kStageCardW + kStageGap) * 2, "LOOP", loopStageStatus(), TFT_ORANGE);
}

template <typename Canvas>
void drawProgressBar(Canvas& gfx, const int16_t x, const int16_t y,
                     const int16_t w, const int16_t h)
{
    gfx.drawRect(x, y, w, h, kColorPanelEdge);
    gfx.fillRect(x + 1, y + 1, w - 2, h - 2, kColorMeterBg);

    const uint32_t duration = activeStageDurationMs();
    if (duration == 0 || w <= 2 || h <= 2) {
        return;
    }

    const int16_t fillW = static_cast<int16_t>(
        static_cast<uint32_t>(w - 2) * activeStageElapsedMs() / duration);
    if (fillW > 0) {
        gfx.fillRect(x + 1, y + 1, fillW, h - 2, stateAccent());
    }
}

template <typename Canvas>
void drawVuBar(Canvas& gfx, const int16_t x, const int16_t y, const int16_t w,
               const int16_t h, const uint16_t rms, const uint16_t peak)
{
    gfx.drawRect(x - 1, y - 1, w + 2, h + 2, kColorPanelEdge);
    gfx.fillRect(x, y, w, h, kColorMeterBg);

    int16_t fillW = static_cast<int16_t>(static_cast<uint32_t>(rms) * w / 8000);
    if (fillW > w) {
        fillW = w;
    }

    if (fillW > 0) {
        gfx.fillRect(x, y, fillW, h, meterBarColor(fillW, w));
    }

    int16_t peakX = static_cast<int16_t>(static_cast<uint32_t>(peak) * w / 8000);
    if (peakX > w) {
        peakX = w;
    }
    if (peakX > 0) {
        gfx.drawFastVLine(x + peakX - 1, y, h, TFT_WHITE);
    }
}

template <typename Canvas>
void drawActivePanel(Canvas& gfx)
{
    gfx.fillRoundRect(kPanelX, kPanelY, kPanelW, kPanelH, 8, kColorPanel);
    gfx.drawRoundRect(kPanelX, kPanelY, kPanelW, kPanelH, 8, stateAccent());

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_WHITE, kColorPanel);
    gfx.drawString(stateTitle(), kPanelX + 10, kPanelY + 7, 2);

    drawProgressBar(gfx, kPanelX + kPanelW - 108, kPanelY + 10, 96, 8);

    gfx.setTextColor(TFT_LIGHTGREY, kColorPanel);
    gfx.drawString(stateHint(), kPanelX + 10, kPanelY + 24, 1);

    if (gState == State::MIC_TEST || gState == State::LOOPBACK_TEST) {
        drawVuBar(gfx, kPanelX + 12, kPanelY + 36, kPanelW - 24, 10, gVuRms, gVuPeak);
        return;
    }

    if (gState == State::SPEAKER_TEST) {
        char freqBuf[16];
        snprintf(freqBuf, sizeof(freqBuf), "%.0f Hz", kToneFreqs[gFreqIndex]);
        gfx.setTextDatum(MC_DATUM);
        gfx.setTextColor(TFT_YELLOW, kColorPanel);
        gfx.drawString(freqBuf, kPanelX + kPanelW / 2, kPanelY + 35, 4);
        gfx.setTextDatum(TL_DATUM);
        return;
    }

    if (gState == State::DONE_PASS || gState == State::DONE_FAIL) {
        gfx.setTextDatum(MC_DATUM);
        gfx.setTextColor(stateAccent(), kColorPanel);
        gfx.drawString(gState == State::DONE_PASS ? "PASS" : "FAIL",
                       kPanelX + kPanelW / 2, kPanelY + 35, 4);
        gfx.setTextDatum(TL_DATUM);
    }
}

template <typename Canvas>
void drawMetricCard(Canvas& gfx, const int16_t x, const char* title, const char* value,
                    const uint16_t valueColor, const uint16_t borderColor)
{
    gfx.fillRoundRect(x, kMetricY, kMetricW, kMetricH, 6, kColorCard);
    gfx.drawRoundRect(x, kMetricY, kMetricW, kMetricH, 6, borderColor);

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_LIGHTGREY, kColorCard);
    gfx.drawString(title, x + 6, kMetricY + 5, 1);

    gfx.setTextDatum(TR_DATUM);
    gfx.setTextColor(valueColor, kColorCard);
    gfx.drawString(value, x + kMetricW - 6, kMetricY + 4, 1);
    gfx.setTextDatum(TL_DATUM);
}

template <typename Canvas>
void drawMetricRow(Canvas& gfx)
{
    char rmsBuf[12];
    char peakBuf[12];
    char infoBuf[16];

    if (hasInputMeter()) {
        snprintf(rmsBuf, sizeof(rmsBuf), "%u", gVuRms);
        snprintf(peakBuf, sizeof(peakBuf), "%u", gVuPeak);
    } else {
        snprintf(rmsBuf, sizeof(rmsBuf), "--");
        snprintf(peakBuf, sizeof(peakBuf), "--");
    }

    uint16_t infoColor = TFT_DARKGREY;
    if (gState == State::SPEAKER_TEST) {
        snprintf(infoBuf, sizeof(infoBuf), "%.0fHz", kToneFreqs[gFreqIndex]);
        infoColor = TFT_YELLOW;
    } else if (gState == State::DONE_PASS) {
        snprintf(infoBuf, sizeof(infoBuf), "OK");
        infoColor = TFT_GREEN;
    } else if (gState == State::DONE_FAIL) {
        snprintf(infoBuf, sizeof(infoBuf), "CHECK");
        infoColor = TFT_RED;
    } else if (hasInputMeter()) {
        snprintf(infoBuf, sizeof(infoBuf), "%s", levelLabel(gVuRms));
        infoColor = levelColor(gVuRms);
    } else {
        snprintf(infoBuf, sizeof(infoBuf), "WAIT");
    }

    drawMetricCard(gfx, kUiMargin, "RMS", rmsBuf, TFT_WHITE, kColorPanelEdge);
    drawMetricCard(gfx, kUiMargin + kMetricW + kStageGap, "PEAK", peakBuf, TFT_WHITE, kColorPanelEdge);
    drawMetricCard(gfx, kUiMargin + (kMetricW + kStageGap) * 2, "INFO", infoBuf, infoColor, stateAccent());
}

template <typename Canvas>
void drawUi(Canvas& gfx)
{
    gfx.fillRect(0, 0, gfx.width(), gfx.height(), kColorBg);
    drawHeader(gfx);
    drawStageRow(gfx);
    drawActivePanel(gfx);
    drawMetricRow(gfx);
    drawFooter(gfx);
}

void redrawScreen()
{
    if (gCanvasReady) {
        drawUi(gCanvas);
        gCanvas.pushSprite(0, 0);
        return;
    }

    drawUi(tft);
}

void handleEncoderFocus()
{
    const int32_t cur = g.encRaw;
    const int32_t delta = (cur - gEncSnapshot) / 2;
    if (delta == 0) {
        return;
    }

    gEncSnapshot += delta * 2;

    int focus = static_cast<int>(gFocus);
    focus += static_cast<int>(delta);
    focus %= static_cast<int>(FocusItem::kCount);
    if (focus < 0) {
        focus += static_cast<int>(FocusItem::kCount);
    }

    const FocusItem nextFocus = static_cast<FocusItem>(focus);
    if (nextFocus != gFocus) {
        gFocus = nextFocus;
        markDirty();
    }
}

void handleButtons()
{
    if (g.usrBtn.event) {
        g.usrBtn.event = false;
        if (gState == State::DONE_PASS || gState == State::DONE_FAIL) {
            Serial.println(F("[AUD] Restarting test..."));
            resetTestState();
        }
    }

    if (!g.encBtn.event) {
        return;
    }

    g.encBtn.event = false;
    if (gFocus == FocusItem::Back) {
        requestExitSubPage();
    }
}

void updateStateMachine()
{
    const uint32_t now = millis();

    switch (gState) {
        case State::INIT_MIC:
            gMicStarted    = true;
            gVuRms         = 0;
            gVuPeak        = 0;
            gVuPeakDecayMs = now;
            Serial.println(F("[AUD] Starting mic test (5 s - make some noise)..."));
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
                if (rms > gVuPeak) {
                    gVuPeak = rms;
                    gVuPeakDecayMs = now;
                }
                if (now - gVuPeakDecayMs > 800) {
                    gVuPeak = (gVuPeak * 15) / 16;
                }
                if (rms > kMicSignalThreshold) {
                    gMicOk = true;
                }
                markDirty();
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
            gFreqIndex       = 0;
            gTonePhase       = 0;
            gFreqLastChangeMs = now;
            Serial.println(F("[AUD] Starting speaker test (3 tones)..."));
            if (!initSpeaker()) {
                Serial.println(F("[AUD] Speaker I2S init failed."));
                gSpkOk = false;
                setState(State::INIT_LOOPBACK);
            } else {
                gSpkOk = true;
                for (uint8_t i = 0; i < kSpkWarmupChunks; ++i) {
                    writeToneChunk();
                }
                setState(State::SPEAKER_TEST);
            }
            break;

        case State::SPEAKER_TEST: {
            writeToneChunk();

            if (now - gFreqLastChangeMs >= kSpkFreqDwellMs) {
                gFreqIndex = (gFreqIndex + 1) % 3;
                gFreqLastChangeMs = now;
                markDirty();
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
            gLoopStarted    = true;
            gLoopWrite      = 0;
            gLoopRead       = 0;
            gLoopFilled     = false;
            gVuRms          = 0;
            gVuPeak         = 0;
            gVuPeakDecayMs  = now;
            Serial.println(F("[AUD] Starting loopback test (8 s - speak into mic)..."));
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
            int16_t inBuf[kBufSamples];
            size_t bytesRead = 0;
            i2s_read(kMicPort, inBuf, sizeof(inBuf), &bytesRead, 0);
            const size_t inN = bytesRead / sizeof(int16_t);
            for (size_t i = 0; i < inN; ++i) {
                gLoopBuf[gLoopWrite] = inBuf[i];
                gLoopWrite = (gLoopWrite + 1) % kLoopBufSamples;
                if (gLoopWrite == 0) {
                    gLoopFilled = true;
                }
            }

            if (inN > 0) {
                gVuRms = computeRms(inBuf, inN);
                if (gVuRms > gVuPeak) {
                    gVuPeak = gVuRms;
                    gVuPeakDecayMs = now;
                }
                if (now - gVuPeakDecayMs > 800) {
                    gVuPeak = (gVuPeak * 15) / 16;
                }
                if (gVuRms > kMicSignalThreshold) {
                    gLoopOk = true;
                }
                markDirty();
            }

            if (gLoopFilled) {
                int16_t outBuf[kBufSamples];
                for (size_t i = 0; i < kBufSamples; ++i) {
                    outBuf[i] = gLoopBuf[gLoopRead];
                    gLoopRead = (gLoopRead + 1) % kLoopBufSamples;
                }
                writeSpeakerSamples(outBuf, kBufSamples);
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

}  // namespace

void init()
{
    stopAllAudio();

    gFocus       = FocusItem::Controls;
    gEncSnapshot = g.encRaw;
    gScreenDirty = true;
    gLastUiDrawMs = 0;
    gCanvasReady = false;
    gMicDriverOk = false;
    gSpkDriverOk = false;

    g.encBtn.event = false;
    g.usrBtn.event = false;

    gCanvas.deleteSprite();
    gCanvas.setColorDepth(16);
    gCanvasReady = (gCanvas.createSprite(tft.width(), tft.height()) != nullptr);
    if (!gCanvasReady) {
        Serial.println(F("[AUD] Sprite allocation failed, using direct TFT redraw."));
    }

    Serial.println(F("[AUD] Mic & speaker page ready."));
    resetTestState();
}

void update()
{
    handleEncoderFocus();
    handleButtons();
    updateStateMachine();

    if (gState == State::SPEAKER_TEST &&
        millis() - gLastUiDrawMs >= kSpeakerUiFrameIntervalMs) {
        markDirty();
    }
}

void render()
{
    if (!gScreenDirty) {
        return;
    }

    const uint32_t now = millis();
    const uint32_t minFrameInterval =
        (gState == State::SPEAKER_TEST) ? kSpeakerUiFrameIntervalMs : kUiFrameIntervalMs;
    if (gLastUiDrawMs != 0 && (now - gLastUiDrawMs) < minFrameInterval) {
        return;
    }

    redrawScreen();
    gScreenDirty = false;
    gLastUiDrawMs = now;
    t_embed::board::deselectSharedSpiDevices();
}

void deinit()
{
    stopAllAudio();
    gCanvas.deleteSprite();
    gCanvasReady = false;
    gScreenDirty = true;
    gMicDriverOk = false;
    gSpkDriverOk = false;
    g.encBtn.event = false;
    g.usrBtn.event = false;
}

}  // namespace page_mic
