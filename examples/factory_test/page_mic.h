#pragma once
#include <driver/i2s.h>
#include <math.h>

namespace page_mic {

namespace {
    constexpr i2s_port_t kPort      = I2S_NUM_0;
    constexpr int        kSampleRate = 16000;
    constexpr size_t     kBufSamples = 512;

    bool     gInitOk  = false;
    bool     gDirty   = true;
    uint16_t gRms     = 0;
    uint16_t gPeak    = 0;
    uint32_t gPeakMs  = 0;

    constexpr int16_t kBarX = 8;
    constexpr int16_t kBarY = 80;
    constexpr int16_t kBarW = 304;
    constexpr int16_t kBarH = 24;

    uint16_t computeRms(const int16_t* buf, size_t n) {
        if (!n) return 0;
        int64_t sum = 0;
        for (size_t i = 0; i < n; ++i) { int32_t s = buf[i]; sum += s * s; }
        return (uint16_t)sqrt((double)sum / n);
    }

    void drawBar(uint16_t rms, uint16_t peak) {
        tft.fillRect(kBarX, kBarY, kBarW, kBarH, 0x2104);
        const int16_t fill = (int16_t)((uint32_t)rms * kBarW / 8000);
        const int16_t clamped = fill > kBarW ? kBarW : fill;
        uint16_t c = (clamped < kBarW * 60 / 100) ? TFT_GREEN :
                     (clamped < kBarW * 85 / 100) ? TFT_YELLOW : TFT_RED;
        if (clamped > 0) tft.fillRect(kBarX, kBarY, clamped, kBarH, c);
        const int16_t peakX = kBarX + (int16_t)((uint32_t)peak * kBarW / 8000);
        if (peakX > kBarX) tft.drawFastVLine(peakX > kBarX + kBarW ? kBarX + kBarW : peakX,
                                              kBarY, kBarH, TFT_WHITE);
    }
}  // namespace

void init() {
    gInitOk = false;
    gRms = gPeak = 0;
    gPeakMs = millis();

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
    pins.ws_io_num    = BOARD_MIC_CLK;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num  = BOARD_MIC_DATA;

    if (i2s_driver_install(kPort, &cfg, 0, nullptr) != ESP_OK) {
        Serial.println(F("[MIC] I2S install failed."));
        goto draw_header;
    }
    if (i2s_set_pin(kPort, &pins) != ESP_OK) {
        i2s_driver_uninstall(kPort);
        Serial.println(F("[MIC] I2S set_pin failed."));
        goto draw_header;
    }
    i2s_zero_dma_buffer(kPort);
    gInitOk = true;
    Serial.println(F("[MIC] PDM mic initialized."));

draw_header:
    tft.fillRect(0, 0, tft.width(), 22, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.drawCentreString("Microphone (PDM)", tft.width() / 2, 4, 2);
    tft.fillRect(0, 22, tft.width(), tft.height() - 22, TFT_BLACK);

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Speak into the device:", 8, 50, 2);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("RMS level:", 8, 116, 1);
    gDirty = true;
}

void update() {
    if (!gInitOk) return;
    int16_t buf[kBufSamples];
    size_t bytesRead = 0;
    i2s_read(kPort, buf, sizeof(buf), &bytesRead, 0);
    const size_t n = bytesRead / sizeof(int16_t);
    if (!n) return;

    const uint32_t now = millis();
    gRms = computeRms(buf, n);
    if (gRms > gPeak) { gPeak = gRms; gPeakMs = now; }
    if (now - gPeakMs > 800) gPeak = (gPeak * 15) / 16;
    gDirty = true;
}

void render() {
    if (!gDirty) return;
    gDirty = false;

    const int16_t W = tft.width();
    if (!gInitOk) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawCentreString("INIT FAILED", W / 2, 80, 4);
        return;
    }

    drawBar(gRms, gPeak);

    char buf[16];
    tft.fillRect(8, 116, W - 16, 14, TFT_BLACK);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("RMS:", 8, 116, 1);
    snprintf(buf, sizeof(buf), "%u", gRms);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(buf, 40, 116, 1);

    // Color-coded signal level label
    tft.fillRect(8, 132, W - 16, 16, TFT_BLACK);
    const char* lvlLabel;
    uint16_t lvlColor;
    if (gRms < 100)       { lvlLabel = "Silence";  lvlColor = TFT_DARKGREY; }
    else if (gRms < 500)  { lvlLabel = "Low";      lvlColor = TFT_CYAN; }
    else if (gRms < 2000) { lvlLabel = "Medium";   lvlColor = TFT_GREEN; }
    else                  { lvlLabel = "Loud";     lvlColor = TFT_RED; }
    tft.setTextColor(lvlColor, TFT_BLACK);
    tft.drawCentreString(lvlLabel, W / 2, 132, 2);

    // Footer
    tft.fillRect(0, tft.height() - 12, W, 12, 0x2104);
    tft.setTextColor(TFT_DARKGREY, 0x2104);
    tft.drawCentreString("USR=back", W / 2, tft.height() - 11, 1);
}

void deinit() {
    if (gInitOk) i2s_driver_uninstall(kPort);
    gInitOk = false;
}

}  // namespace page_mic
