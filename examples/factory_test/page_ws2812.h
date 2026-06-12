#pragma once
#include <Adafruit_NeoPixel.h>

namespace page_ws2812 {

namespace {
    Adafruit_NeoPixel* strip = nullptr;
    bool     gDirty     = true;
    bool     gInitOk    = false;
    uint16_t gHue       = 0;
    uint32_t gLastMs    = 0;
    uint32_t gFrames    = 0;

    // Convert Adafruit gamma-corrected 32-bit color to approximate 16-bit 565
    uint16_t color32To565(uint32_t c) {
        uint8_t r = (c >> 16) & 0xFF;
        uint8_t g = (c >>  8) & 0xFF;
        uint8_t b = (c >>  0) & 0xFF;
        return tft.color565(r, g, b);
    }
}  // namespace

void init() {
    gDirty = true; gInitOk = false; gHue = 0; gFrames = 0; gLastMs = millis();

    if (strip) { delete strip; strip = nullptr; }
    strip = new Adafruit_NeoPixel(BOARD_WS2812_NUM_LEDS, BOARD_WS2812_DATA_PIN, NEO_GRB + NEO_KHZ800);
    strip->begin();
    strip->setBrightness(32);
    strip->show();
    gInitOk = true;

    tft.fillRect(0, 0, tft.width(), 22, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.drawCentreString("WS2812 LEDs", tft.width() / 2, 4, 2);
    tft.fillRect(0, 22, tft.width(), tft.height() - 22, TFT_BLACK);
}

void update() {
    if (!gInitOk || !strip) return;
    const uint32_t now = millis();
    if (now - gLastMs < 30) return;
    gLastMs = now;

    // Rainbow cycle
    for (uint8_t i = 0; i < BOARD_WS2812_NUM_LEDS; ++i) {
        uint16_t pixHue = gHue + (uint16_t)(i * 65536UL / BOARD_WS2812_NUM_LEDS);
        strip->setPixelColor(i, strip->gamma32(strip->ColorHSV(pixHue)));
    }
    strip->show();
    gHue += 512;
    ++gFrames;
    gDirty = true;
}

void render() {
    if (!gInitOk || !strip) return;
    if (!gDirty) return;
    gDirty = false;

    const int16_t W = tft.width();
    tft.fillRect(0, 22, W, tft.height() - 22, TFT_BLACK);

    // Draw 8 color swatches reflecting current LED colors
    const int16_t swW  = 28;
    const int16_t swH  = 28;
    const int16_t swY  = 50;
    const int16_t totalW = BOARD_WS2812_NUM_LEDS * swW + (BOARD_WS2812_NUM_LEDS - 1) * 4;
    int16_t swX = (W - totalW) / 2;

    for (uint8_t i = 0; i < BOARD_WS2812_NUM_LEDS; ++i) {
        uint32_t c = strip->getPixelColor(i);
        uint16_t c565 = color32To565(c);
        tft.fillRect(swX, swY, swW, swH, c565);
        tft.drawRect(swX, swY, swW, swH, TFT_DARKGREY);
        swX += swW + 4;
    }

    // Frame counter
    char buf[24];
    snprintf(buf, sizeof(buf), "frames: %lu", (unsigned long)gFrames);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString(buf, W / 2, 94, 1);

    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawCentreString("Rainbow cycle running", W / 2, 110, 1);

    // Footer
    tft.fillRect(0, tft.height() - 12, W, 12, 0x2104);
    tft.setTextColor(TFT_DARKGREY, 0x2104);
    tft.drawCentreString("USR=back  (LEDs stop on exit)", W / 2, tft.height() - 11, 1);
}

void deinit() {
    if (strip) {
        strip->clear();
        strip->show();
        delete strip;
        strip = nullptr;
    }
    gInitOk = false;
}

}  // namespace page_ws2812
