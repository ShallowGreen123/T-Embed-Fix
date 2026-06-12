#pragma once
#include <RadioLib.h>

namespace page_cc1101 {

namespace {
    SPIClass radioSPI(HSPI);
    CC1101*  radio = nullptr;

    static const float kFreqs[]  = {315.0f, 433.92f, 868.0f};
    static const char* kFreqLabels[] = {"315 MHz", "433 MHz", "868 MHz"};
    constexpr uint8_t kFreqCount = 3;

    uint8_t  gFreqIdx    = 1;   // default 433
    bool     gInitOk     = false;
    bool     gDirty      = true;
    float    gRssi       = -120.0f;
    uint32_t gLastRssiMs = 0;
    uint32_t gRxCount    = 0;
    char     gLastPkt[32] = "--";
    bool     gReceiving  = false;

    Module*  gModule = nullptr;

    void startReceive() {
        if (!gInitOk) return;
        radio->startReceive();
        gReceiving = true;
    }

    void applyFreq() {
        if (!gInitOk) return;
        radio->standby();
        t_embed::board::setCc1101RfPath(ioExpander, (int)kFreqs[gFreqIdx]);
        radio->setFrequency(kFreqs[gFreqIdx]);
        startReceive();
        gDirty = true;
    }
}  // namespace

void init() {
    gInitOk    = false;
    gRxCount   = 0;
    gRssi      = -120.0f;
    gFreqIdx   = 1;
    strncpy(gLastPkt, "--", sizeof(gLastPkt));

    t_embed::board::deselectSharedSpiDevices();
    delay(5);

    t_embed::board::setCc1101RfPath(ioExpander, (int)kFreqs[gFreqIdx]);
    radioSPI.begin(BOARD_CC1101_SCK, BOARD_CC1101_MISO, BOARD_CC1101_MOSI);
    delay(5);

    if (gModule) { delete gModule; gModule = nullptr; }
    if (radio)   { delete radio;   radio   = nullptr; }
    gModule = new Module(BOARD_CC1101_CS, BOARD_CC1101_GDO0, RADIOLIB_NC, BOARD_CC1101_GDO2, radioSPI);
    radio   = new CC1101(gModule);

    int state = radio->begin(kFreqs[gFreqIdx]);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("[CC1101] begin() failed: ")); Serial.println(state);
    } else {
        gInitOk = true;
        startReceive();
        Serial.println(F("[CC1101] Initialized."));
    }

    // Header
    tft.fillRect(0, 0, tft.width(), 22, 0x000F);
    tft.setTextColor(TFT_WHITE, 0x000F);
    tft.drawCentreString("CC1101 Radio", tft.width() / 2, 4, 2);
    tft.fillRect(0, 22, tft.width(), tft.height() - 22, TFT_BLACK);
    gDirty = true;
    gLastRssiMs = millis();
}

void update() {
    if (!gInitOk) return;
    const uint32_t now = millis();

    // Check encBtn for frequency cycling
    if (g.encBtn.event) {
        g.encBtn.event = false;
        gFreqIdx = (gFreqIdx + 1) % kFreqCount;
        applyFreq();
    }

    // Poll for received packet
    if (radio->available()) {
        uint8_t pkt[64];
        size_t  len = sizeof(pkt);
        if (radio->readData(pkt, len) == RADIOLIB_ERR_NONE && len > 0) {
            ++gRxCount;
            uint8_t printLen = len < 8 ? (uint8_t)len : 8;
            char* p = gLastPkt;
            for (uint8_t i = 0; i < printLen; ++i) {
                p += snprintf(p, 4, "%02X ", pkt[i]);
            }
            gDirty = true;
        }
        radio->startReceive();
    }

    // RSSI every 500 ms
    if (now - gLastRssiMs >= 500) {
        gLastRssiMs = now;
        gRssi  = radio->getRSSI();
        gDirty = true;
    }
}

void render() {
    if (!gDirty) return;
    gDirty = false;

    const int16_t W  = tft.width();
    tft.fillRect(0, 22, W, tft.height() - 22, TFT_BLACK);

    if (!gInitOk) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawCentreString("INIT FAILED", W / 2, 80, 4);
        return;
    }

    // Frequency (big)
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawCentreString(kFreqLabels[gFreqIdx], W / 2, 28, 4);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString("ENCBTN=cycle freq", W / 2, 68, 1);

    // RSSI bar
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("RSSI:", 8, 84, 1);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f dBm", gRssi);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(buf, 52, 84, 1);

    // RSSI bar graphic
    const int16_t barMaxW = W - 16;
    const int16_t filled  = (int16_t)((gRssi + 120.0f) / 80.0f * barMaxW);
    const int16_t clamped = filled < 0 ? 0 : (filled > barMaxW ? barMaxW : filled);
    tft.fillRect(8, 96, barMaxW, 10, 0x2104);
    if (clamped > 0) tft.fillRect(8, 96, clamped, 10, TFT_GREEN);

    // Last packet
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Last RX:", 8, 114, 1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(gLastPkt, 8, 126, 1);
    snprintf(buf, sizeof(buf), "RX count: %lu", (unsigned long)gRxCount);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString(buf, 8, 140, 1);

    // Footer
    tft.fillRect(0, tft.height() - 12, W, 12, 0x2104);
    tft.setTextColor(TFT_DARKGREY, 0x2104);
    tft.drawCentreString("USR=back", W / 2, tft.height() - 11, 1);
}

void deinit() {
    if (gInitOk && radio) radio->standby();
    radioSPI.end();
    t_embed::board::deselectSharedSpiDevices();
    gInitOk = false;
}

}  // namespace page_cc1101
