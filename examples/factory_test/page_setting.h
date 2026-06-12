#pragma once

namespace page_setting {

namespace {
    bool     gDirty    = true;
    uint32_t gLastMs   = 0;

    // Static info (populated once in init)
    String   gChipModel;
    uint32_t gCpuMhz    = 0;
    uint32_t gFlashMB   = 0;
    uint32_t gPsramKB   = 0;
    String   gBuildDate;

    // Dynamic (refreshed periodically)
    uint32_t gFreeHeap  = 0;
    uint32_t gMinHeap   = 0;

    void drawRow(int16_t y, const char* label, const String& val, uint16_t valColor = TFT_WHITE) {
        tft.fillRect(0, y, tft.width(), 13, TFT_BLACK);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString(label, 8, y, 1);
        tft.setTextColor(valColor, TFT_BLACK);
        tft.drawString(val, 110, y, 1);
    }
}  // namespace

void init() {
    gChipModel  = String(ESP.getChipModel());
    gCpuMhz     = ESP.getCpuFreqMHz();
    gFlashMB    = ESP.getFlashChipSize() / (1024 * 1024);
    gPsramKB    = ESP.getPsramSize() / 1024;
    gBuildDate  = String(__DATE__) + " " + String(__TIME__);
    gFreeHeap   = ESP.getFreeHeap();
    gMinHeap    = ESP.getMinFreeHeap();

    tft.fillRect(0, 0, tft.width(), 22, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.drawCentreString("Device Info", tft.width() / 2, 4, 2);
    tft.fillRect(0, 22, tft.width(), tft.height() - 22, TFT_BLACK);

    gDirty = true;
    gLastMs = millis();
}

void update() {
    if (millis() - gLastMs >= 2000) {
        gLastMs   = millis();
        gFreeHeap = ESP.getFreeHeap();
        gMinHeap  = ESP.getMinFreeHeap();
        gDirty    = true;
    }
}

void render() {
    if (!gDirty) return;
    gDirty = false;

    const int16_t W = tft.width();
    tft.fillRect(0, 22, W, tft.height() - 22, TFT_BLACK);

    char buf[32];
    int16_t y = 26;
    const int16_t step = 14;

    drawRow(y, "Chip",     gChipModel);   y += step;
    snprintf(buf, sizeof(buf), "%lu MHz", (unsigned long)gCpuMhz);
    drawRow(y, "CPU Freq", String(buf));  y += step;
    snprintf(buf, sizeof(buf), "%lu MB",  (unsigned long)gFlashMB);
    drawRow(y, "Flash",    String(buf));  y += step;
    if (gPsramKB > 0) {
        snprintf(buf, sizeof(buf), "%lu KB", (unsigned long)gPsramKB);
        drawRow(y, "PSRAM", String(buf)); y += step;
    }
    snprintf(buf, sizeof(buf), "%lu B", (unsigned long)gFreeHeap);
    drawRow(y, "Free Heap", String(buf), gFreeHeap > 50000 ? TFT_GREEN : TFT_YELLOW); y += step;
    snprintf(buf, sizeof(buf), "%lu B", (unsigned long)gMinHeap);
    drawRow(y, "Min Heap",  String(buf), TFT_LIGHTGREY); y += step;
    drawRow(y, "Built",     gBuildDate); y += step;

    // Board label
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString("T-Embed CC1101 V1.1", W / 2, y + 2, 1); y += 14;

    // Footer
    tft.fillRect(0, tft.height() - 12, W, 12, 0x2104);
    tft.setTextColor(TFT_DARKGREY, 0x2104);
    tft.drawCentreString("USR=back", W / 2, tft.height() - 11, 1);
}

void deinit() {}

}  // namespace page_setting
