#pragma once
#include <SD.h>

namespace page_sd {

namespace {
    constexpr uint32_t kFreqs[] = {10000000UL, 4000000UL, 1000000UL};

    bool     gInitDone = false;
    bool     gDirty    = true;

    // Results stored from one-shot init test
    bool     gMountOk  = false;
    bool     gWriteOk  = false;
    bool     gReadOk   = false;
    uint32_t gMountedHz= 0;
    uint8_t  gCardType = 0;
    uint32_t gTotalMB  = 0;
    uint32_t gUsedMB   = 0;

    String cardTypeStr(uint8_t t) {
        switch (t) {
            case CARD_MMC:  return "MMC";
            case CARD_SD:   return "SD";
            case CARD_SDHC: return "SDHC";
            default:        return "UNKNOWN";
        }
    }

    void drawRow(int16_t y, const char* label, const char* val, uint16_t valColor) {
        tft.fillRect(0, y, tft.width(), 14, TFT_BLACK);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString(label, 8, y, 1);
        tft.setTextColor(valColor, TFT_BLACK);
        tft.drawString(val, 90, y, 1);
    }

    void runTest() {
        gMountOk = false; gWriteOk = false; gReadOk = false;

        t_embed::board::deselectSharedSpiDevices();
        pinMode(BOARD_SD_CS, OUTPUT);
        digitalWrite(BOARD_SD_CS, HIGH);
        delay(120);

        SPIClass& sharedSpi = tft.getSPIinstance();

        for (uint32_t freq : kFreqs) {
            SD.end();
            t_embed::board::deselectSharedSpiDevices();
            delay(20);
            Serial.print(F("[SD] Try mount @ "));
            Serial.print(freq / 1000000UL);
            Serial.println(F(" MHz"));
            if (SD.begin(BOARD_SD_CS, sharedSpi, freq)) {
                gMountOk  = true;
                gMountedHz = freq;
                break;
            }
        }

        if (!gMountOk) {
            Serial.println(F("[SD] Mount failed."));
            return;
        }

        gCardType = SD.cardType();
        gTotalMB  = (uint32_t)(SD.totalBytes() / (1024ULL * 1024ULL));
        gUsedMB   = (uint32_t)(SD.usedBytes()  / (1024ULL * 1024ULL));

        const char* testPath = "/factory_test.tmp";
        const char* testData = "T-Embed factory test OK";

        File f = SD.open(testPath, FILE_WRITE);
        if (f) { f.print(testData); f.close(); gWriteOk = true; }

        f = SD.open(testPath, FILE_READ);
        if (f) {
            String s = f.readStringUntil('\n');
            f.close();
            gReadOk = s.startsWith("T-Embed factory test OK");
        }
        SD.remove(testPath);
        Serial.print(F("[SD] Mount OK  Write:")); Serial.print(gWriteOk);
        Serial.print(F("  Read:")); Serial.println(gReadOk);
    }
}  // namespace

void init() {
    gInitDone = false;
    gDirty    = true;

    tft.fillRect(0, 0, tft.width(), 22, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.drawCentreString("SD Card", tft.width() / 2, 4, 2);
    tft.fillRect(0, 22, tft.width(), tft.height() - 22, TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawCentreString("Testing...", tft.width() / 2, 80, 2);

    runTest();
    gInitDone = true;
}

void update() {}  // one-shot

void render() {
    if (!gDirty) return;
    gDirty = false;

    const int16_t W = tft.width();
    tft.fillRect(0, 22, W, tft.height() - 22, TFT_BLACK);

    // Overall result (big label)
    const bool pass = gMountOk && gWriteOk && gReadOk;
    tft.setTextColor(pass ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.drawCentreString(pass ? "PASS" : "FAIL", W / 2, 26, 6);

    // Detail rows
    char buf[20];

    snprintf(buf, sizeof(buf), gMountOk ? "OK" : "FAIL");
    drawRow(92, "Mount", buf, gMountOk ? TFT_GREEN : TFT_RED);

    if (gMountOk) {
        snprintf(buf, sizeof(buf), "%lu MHz", (unsigned long)(gMountedHz / 1000000UL));
        drawRow(104, "SPI", buf, TFT_WHITE);
        drawRow(116, "Type", cardTypeStr(gCardType).c_str(), TFT_WHITE);
        snprintf(buf, sizeof(buf), "%lu MB", (unsigned long)gTotalMB);
        drawRow(128, "Total", buf, TFT_WHITE);
        snprintf(buf, sizeof(buf), "%lu MB", (unsigned long)gUsedMB);
        drawRow(140, "Used", buf, TFT_WHITE);
    }

    drawRow(152, "Write", gWriteOk ? "OK" : "FAIL", gWriteOk ? TFT_GREEN : TFT_RED);
    drawRow(164, "Read",  gReadOk  ? "OK" : "FAIL", gReadOk  ? TFT_GREEN : TFT_RED);

    // Footer
    tft.fillRect(0, tft.height() - 12, W, 12, 0x2104);
    tft.setTextColor(TFT_DARKGREY, 0x2104);
    tft.drawCentreString("USR=back", W / 2, tft.height() - 11, 1);
}

void deinit() {
    SD.end();
    t_embed::board::deselectSharedSpiDevices();
}

}  // namespace page_sd
