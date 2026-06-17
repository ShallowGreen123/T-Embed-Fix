#pragma once
#include <Adafruit_NeoPixel.h>
#include <TFT_eSPI.h>

namespace page_ws2812 {

namespace {

constexpr uint32_t kAnimIntervalMs    = 30;
constexpr uint32_t kUiFrameIntervalMs = 66;

constexpr int16_t kUiMargin     = 8;
constexpr int16_t kHeaderHeight = 24;
constexpr int16_t kFooterHeight = 18;
constexpr int16_t kBackBtnW     = 58;
constexpr int16_t kBackBtnH     = 14;

constexpr int16_t kStatusX = 8;
constexpr int16_t kStatusY = 34;
constexpr int16_t kStatusW = 304;
constexpr int16_t kStatusH = 34;

constexpr int16_t kPreviewX = 8;
constexpr int16_t kPreviewY = 76;
constexpr int16_t kPreviewW = 304;
constexpr int16_t kPreviewH = 42;

constexpr int16_t kMetricY = 126;
constexpr int16_t kMetricW = 72;
constexpr int16_t kMetricH = 18;
constexpr int16_t kMetricGap = 8;

constexpr uint16_t kColorBg        = 0x0841;
constexpr uint16_t kColorPanel     = 0x1082;
constexpr uint16_t kColorPanelEdge = 0x31A6;
constexpr uint16_t kColorCard      = 0x18C3;
constexpr uint16_t kColorPassBg    = 0x0A41;
constexpr uint16_t kColorFailBg    = 0x3006;

enum class FocusItem : uint8_t {
    Preview = 0,
    Back,
    kCount,
};

Adafruit_NeoPixel* gStrip          = nullptr;
TFT_eSprite        gCanvas(&tft);
FocusItem          gFocus          = FocusItem::Preview;
bool               gCanvasReady    = false;
bool               gScreenDirty    = true;
bool               gInitOk         = false;
uint8_t            gBrightness     = 32;
uint16_t           gHue            = 0;
uint16_t           gAnimFps        = 0;
uint32_t           gLastAnimMs     = 0;
uint32_t           gLastUiDrawMs   = 0;
uint32_t           gFpsWindowMs    = 0;
uint32_t           gFpsWindowCount = 0;
int32_t            gEncSnapshot    = 0;

constexpr uint8_t kBrightnessLevels[] = {16, 32, 64, 96};
constexpr uint8_t kDefaultBrightnessIndex = 1;
uint8_t gBrightnessIndex = kDefaultBrightnessIndex;

void markDirty()
{
    gScreenDirty = true;
}

uint16_t color32To565(const uint32_t color)
{
    const uint8_t r = (color >> 16) & 0xFF;
    const uint8_t g = (color >> 8) & 0xFF;
    const uint8_t b = color & 0xFF;
    return tft.color565(r, g, b);
}

uint16_t statusAccentColor()
{
    if (!gInitOk) {
        return TFT_RED;
    }
    return TFT_GREEN;
}

uint16_t statusFillColor()
{
    if (!gInitOk) {
        return kColorFailBg;
    }
    return kColorPassBg;
}

const char* statusTitle()
{
    if (!gInitOk) {
        return "WS2812 driver failed";
    }
    return "Rainbow running";
}

const char* statusDetail()
{
    if (!gInitOk) {
        return "Init failed. Select BACK to return.";
    }
    return "Check smooth color sweep. USR changes brightness.";
}

const char* statusTag()
{
    if (!gInitOk) {
        return "FAIL";
    }
    return "LIVE";
}

const char* footerText()
{
    if (gFocus == FocusItem::Back) {
        return gInitOk ? "BOOT=back  USR=brightness" : "BOOT=back";
    }
    if (!gInitOk) {
        return "Turn to BACK";
    }
    return "USR=brightness  turn=BACK";
}

String brightnessText()
{
    return String(gBrightness);
}

String fpsText()
{
    return String(gAnimFps);
}

String hueText()
{
    return String(static_cast<uint16_t>(gHue / 256U));
}

void applyRainbowFrame()
{
    if (!gStrip) {
        return;
    }

    for (uint8_t i = 0; i < BOARD_WS2812_NUM_LEDS; ++i) {
        const uint16_t pixelHue =
            gHue + static_cast<uint16_t>(i * 65536UL / BOARD_WS2812_NUM_LEDS);
        gStrip->setPixelColor(i, gStrip->gamma32(gStrip->ColorHSV(pixelHue)));
    }
    gStrip->show();
}

template <typename Canvas>
void drawBackButton(Canvas& gfx, const bool selected)
{
    const int16_t x = gfx.width() - kBackBtnW - 6;
    const int16_t y = gfx.height() - kFooterHeight + 2;
    const uint16_t bg = selected ? TFT_WHITE : TFT_DARKGREY;
    const uint16_t fg = selected ? TFT_BLACK : TFT_LIGHTGREY;

    gfx.fillRoundRect(x, y, kBackBtnW, kBackBtnH, 5, bg);
    gfx.drawRoundRect(x, y, kBackBtnW, kBackBtnH, 5,
                      selected ? TFT_YELLOW : 0x52AA);
    gfx.setTextColor(fg, bg);
    gfx.drawCentreString("BACK", x + kBackBtnW / 2, y + 3, 1);
}

template <typename Canvas>
void drawHeader(Canvas& gfx)
{
    gfx.fillRect(0, 0, gfx.width(), kHeaderHeight, TFT_NAVY);
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_WHITE, TFT_NAVY);
    gfx.drawString("WS2812 LEDs", kUiMargin, 5, 2);

    gfx.setTextDatum(TR_DATUM);
    gfx.setTextColor(TFT_CYAN, TFT_NAVY);
    gfx.drawString("Rainbow demo", gfx.width() - kUiMargin, 7, 1);
    gfx.setTextDatum(TL_DATUM);
}

template <typename Canvas>
void drawFooter(Canvas& gfx)
{
    const int16_t y = gfx.height() - kFooterHeight;
    gfx.fillRect(0, y, gfx.width(), kFooterHeight, TFT_DARKGREY);
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_WHITE, TFT_DARKGREY);
    gfx.drawString(footerText(), 6, y + 4, 1);
    drawBackButton(gfx, gFocus == FocusItem::Back);
}

template <typename Canvas>
void drawStatusPanel(Canvas& gfx)
{
    const uint16_t accent = statusAccentColor();
    const uint16_t fill = statusFillColor();

    gfx.fillRoundRect(kStatusX, kStatusY, kStatusW, kStatusH, 8, fill);
    gfx.drawRoundRect(kStatusX, kStatusY, kStatusW, kStatusH, 8, accent);

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(accent, fill);
    gfx.drawString(statusTitle(), kStatusX + 10, kStatusY + 6, 2);

    gfx.fillRoundRect(kStatusX + kStatusW - 56, kStatusY + 7, 44, 16, 6, accent);
    gfx.setTextDatum(MC_DATUM);
    gfx.setTextColor(TFT_BLACK, accent);
    gfx.drawString(statusTag(), kStatusX + kStatusW - 34, kStatusY + 15, 1);

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_LIGHTGREY, fill);
    gfx.drawString(statusDetail(), kStatusX + 10, kStatusY + 23, 1);
}

template <typename Canvas>
void drawPreviewPanel(Canvas& gfx)
{
    const bool selected = (gFocus == FocusItem::Preview);
    const uint16_t border = selected ? TFT_CYAN : kColorPanelEdge;

    gfx.fillRoundRect(kPreviewX, kPreviewY, kPreviewW, kPreviewH, 8, kColorPanel);
    gfx.drawRoundRect(kPreviewX, kPreviewY, kPreviewW, kPreviewH, 8, border);

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_LIGHTGREY, kColorPanel);
    gfx.drawString("LED PREVIEW", kPreviewX + 8, kPreviewY + 5, 1);

    if (!gInitOk || !gStrip) {
        gfx.setTextColor(TFT_RED, kColorPanel);
        gfx.drawString("No LED data available", kPreviewX + 8, kPreviewY + 20, 1);
        return;
    }

    const int16_t innerX = kPreviewX + 10;
    const int16_t innerY = kPreviewY + 17;
    const int16_t gap = 6;
    const int16_t tileCount = BOARD_WS2812_NUM_LEDS;
    const int16_t availableW = kPreviewW - 20 - gap * (tileCount - 1);
    const int16_t tileW = availableW / tileCount;
    const int16_t tileH = 15;

    int16_t x = innerX;
    for (uint8_t i = 0; i < BOARD_WS2812_NUM_LEDS; ++i) {
        const uint16_t c565 = color32To565(gStrip->getPixelColor(i));
        gfx.fillRoundRect(x, innerY, tileW, tileH, 4, c565);
        gfx.drawRoundRect(x, innerY, tileW, tileH, 4, TFT_DARKGREY);
        x += tileW + gap;
    }
}

template <typename Canvas>
void drawMetricCard(Canvas& gfx,
                    const int16_t x,
                    const char* label,
                    const String& value,
                    const uint16_t valueColor,
                    const uint16_t borderColor)
{
    gfx.fillRoundRect(x, kMetricY, kMetricW, kMetricH, 6, kColorCard);
    gfx.drawRoundRect(x, kMetricY, kMetricW, kMetricH, 6, borderColor);

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_CYAN, kColorCard);
    gfx.drawString(label, x + 6, kMetricY + 5, 1);

    gfx.setTextDatum(TR_DATUM);
    gfx.setTextColor(valueColor, kColorCard);
    gfx.drawString(value, x + kMetricW - 6, kMetricY + 5, 1);
    gfx.setTextDatum(TL_DATUM);
}

template <typename Canvas>
void drawMetricRow(Canvas& gfx)
{
    drawMetricCard(gfx, kUiMargin, "PIX", String(BOARD_WS2812_NUM_LEDS), TFT_WHITE, kColorPanelEdge);
    drawMetricCard(gfx, kUiMargin + (kMetricW + kMetricGap), "BRI", brightnessText(), TFT_YELLOW, kColorPanelEdge);
    drawMetricCard(gfx, kUiMargin + (kMetricW + kMetricGap) * 2, "FPS", fpsText(), TFT_GREEN, kColorPanelEdge);
    drawMetricCard(gfx, kUiMargin + (kMetricW + kMetricGap) * 3, "HUE", hueText(),
                   statusAccentColor(), statusAccentColor());
}

template <typename Canvas>
void drawUi(Canvas& gfx)
{
    gfx.fillRect(0, 0, gfx.width(), gfx.height(), kColorBg);
    drawHeader(gfx);
    drawStatusPanel(gfx);
    drawPreviewPanel(gfx);
    drawMetricRow(gfx);
    drawFooter(gfx);
}

void redrawScreen()
{
    if (gCanvasReady) {
        drawUi(gCanvas);
        gCanvas.pushSprite(0, 0);
    } else {
        drawUi(tft);
    }
    t_embed::board::deselectSharedSpiDevices();
}

void handleEncoder()
{
    const int32_t delta = (g.encRaw - gEncSnapshot) / 2;
    if (delta == 0) {
        return;
    }

    gEncSnapshot += delta * 2;
    int32_t next = static_cast<int32_t>(gFocus) + delta;
    next %= static_cast<int32_t>(FocusItem::kCount);
    if (next < 0) {
        next += static_cast<int32_t>(FocusItem::kCount);
    }

    const FocusItem newFocus = static_cast<FocusItem>(next);
    if (newFocus != gFocus) {
        gFocus = newFocus;
        markDirty();
    }
}

void cycleBrightness()
{
    if (!gInitOk || !gStrip) {
        return;
    }

    gBrightnessIndex = (gBrightnessIndex + 1) %
        (sizeof(kBrightnessLevels) / sizeof(kBrightnessLevels[0]));
    gBrightness = kBrightnessLevels[gBrightnessIndex];
    gStrip->setBrightness(gBrightness);
    applyRainbowFrame();
    markDirty();

    Serial.print(F("[WS2812] Brightness -> "));
    Serial.println(gBrightness);
}

void handleButtons()
{
    if (g.encBtn.event) {
        g.encBtn.event = false;
        if (gFocus == FocusItem::Back) {
            requestExitSubPage();
            return;
        }
    }

    if (g.usrBtn.event) {
        g.usrBtn.event = false;
        cycleBrightness();
    }
}

void updateAnimation()
{
    if (!gInitOk || !gStrip) {
        return;
    }

    const uint32_t now = millis();
    if (now - gLastAnimMs < kAnimIntervalMs) {
        return;
    }

    gLastAnimMs = now;
    applyRainbowFrame();
    gHue += 512;
    ++gFpsWindowCount;

    if (gFpsWindowMs == 0) {
        gFpsWindowMs = now;
    } else if (now - gFpsWindowMs >= 1000) {
        gAnimFps = static_cast<uint16_t>(
            (gFpsWindowCount * 1000UL) / (now - gFpsWindowMs));
        gFpsWindowMs = now;
        gFpsWindowCount = 0;
    }

    markDirty();
}

}  // namespace

void init()
{
    gFocus          = FocusItem::Preview;
    gCanvasReady    = false;
    gScreenDirty    = true;
    gInitOk         = false;
    gBrightnessIndex = kDefaultBrightnessIndex;
    gBrightness     = kBrightnessLevels[gBrightnessIndex];
    gHue            = 0;
    gAnimFps        = 0;
    gLastAnimMs     = 0;
    gLastUiDrawMs   = 0;
    gFpsWindowMs    = 0;
    gFpsWindowCount = 0;
    gEncSnapshot    = g.encRaw;

    g.encBtn.event = false;
    g.usrBtn.event = false;

    gCanvas.deleteSprite();
    gCanvas.setColorDepth(16);
    gCanvasReady = (gCanvas.createSprite(tft.width(), tft.height()) != nullptr);
    if (!gCanvasReady) {
        Serial.println(F("[WS2812] Sprite allocation failed, using direct TFT redraw."));
    }

    if (gStrip) {
        delete gStrip;
        gStrip = nullptr;
    }

    gStrip = new Adafruit_NeoPixel(BOARD_WS2812_NUM_LEDS,
                                   BOARD_WS2812_DATA_PIN,
                                   NEO_GRB + NEO_KHZ800);
    if (!gStrip) {
        Serial.println(F("[WS2812] Failed to allocate strip instance."));
        markDirty();
        return;
    }

    gStrip->begin();
    gStrip->setBrightness(gBrightness);
    applyRainbowFrame();
    gInitOk = true;
    markDirty();

    Serial.println(F("[WS2812] Page ready."));
}

void update()
{
    handleEncoder();
    handleButtons();
    if (g.subPageExitRequested) {
        return;
    }

    updateAnimation();
}

void render()
{
    if (!gScreenDirty) {
        return;
    }

    const uint32_t now = millis();
    if (gLastUiDrawMs != 0 && (now - gLastUiDrawMs) < kUiFrameIntervalMs) {
        return;
    }

    redrawScreen();
    gScreenDirty = false;
    gLastUiDrawMs = now;
}

void deinit()
{
    if (gStrip) {
        gStrip->clear();
        gStrip->show();
        delete gStrip;
        gStrip = nullptr;
    }

    gCanvas.deleteSprite();
    gCanvasReady = false;
    gScreenDirty = true;
    gInitOk = false;
    g.encBtn.event = false;
    g.usrBtn.event = false;
}

}  // namespace page_ws2812
