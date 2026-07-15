#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace page_tft {

namespace {

constexpr uint32_t kAutoAdvanceMs    = 3000;
constexpr uint32_t kAnimationFrameMs = 20;
constexpr uint32_t kUiFrameIntervalMs = 20;

constexpr int16_t kHeaderHeight = 28;
constexpr int16_t kFooterHeight = 18;
constexpr int16_t kBackBtnW     = 58;
constexpr int16_t kBackBtnH     = 14;

enum class DemoPage : uint8_t {
    Summary = 0,
    ColorBars,
    Geometry,
    Text,
    Animation,
    Count,
};

enum class FocusItem : uint8_t {
    Demo = 0,
    Back,
    kCount,
};

struct BallState {
    int16_t x      = 40;
    int16_t y      = 64;
    int16_t vx     = 3;
    int16_t vy     = 2;
    int16_t radius = 10;
};

TFT_eSprite gCanvas(&tft);
DemoPage    gCurrentPage     = DemoPage::Summary;
FocusItem   gFocus           = FocusItem::Demo;
BallState   gBall;
bool        gCanvasReady     = false;
bool        gScreenDirty     = true;
bool        gAutoAdvance     = true;
uint32_t    gPageChangedAtMs = 0;
uint32_t    gLastUiDrawMs    = 0;
uint32_t    gLastAnimationMs = 0;
int32_t     gEncSnapshot     = 0;

void markDirty()
{
    gScreenDirty = true;
}

uint8_t pageCount()
{
    return static_cast<uint8_t>(DemoPage::Count);
}

DemoPage pageFromIndex(int index)
{
    const int count = pageCount();
    int wrapped = index % count;
    if (wrapped < 0) {
        wrapped += count;
    }
    return static_cast<DemoPage>(wrapped);
}

DemoPage nextPage(const DemoPage page, const int delta)
{
    return pageFromIndex(static_cast<int>(page) + delta);
}

const char* pageName(const DemoPage page)
{
    switch (page) {
        case DemoPage::Summary:   return "summary";
        case DemoPage::ColorBars: return "colors";
        case DemoPage::Geometry:  return "geometry";
        case DemoPage::Text:      return "text";
        case DemoPage::Animation: return "animation";
        case DemoPage::Count:
        default:                  return "?";
    }
}

String pageIndexText()
{
    return String(static_cast<uint8_t>(gCurrentPage) + 1) + "/" +
           String(pageCount());
}

String actionHintText()
{
    if (gFocus == FocusItem::Back) {
        return "BOOT returns to menu";
    }
    return "USR=retest  turn=BACK";
}

void resetBall()
{
    gBall = BallState{};
}

void showPage(const DemoPage page)
{
    gCurrentPage = page;
    gPageChangedAtMs = millis();
    gLastAnimationMs = 0;
    if (gCurrentPage == DemoPage::Animation) {
        resetBall();
    }
    Serial.print(F("[TFT] Demo page -> "));
    Serial.println(pageName(gCurrentPage));
    markDirty();
}

void restartDemo()
{
    gAutoAdvance = true;
    showPage(DemoPage::Summary);
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
void drawHeader(Canvas& gfx, const char* title, const uint16_t color)
{
    gfx.fillRect(0, 0, gfx.width(), kHeaderHeight, color);
    gfx.setTextColor(TFT_BLACK, color);
    gfx.setTextDatum(TL_DATUM);
    gfx.drawString(title, 8, 6, 2);

    gfx.setTextDatum(TR_DATUM);
    gfx.drawString(pageIndexText(), gfx.width() - 8, 8, 1);
    gfx.setTextDatum(TL_DATUM);
}

template <typename Canvas>
void drawFooter(Canvas& gfx)
{
    const int16_t y = gfx.height() - kFooterHeight;
    gfx.fillRect(0, y, gfx.width(), kFooterHeight, TFT_DARKGREY);
    gfx.setTextColor(TFT_WHITE, TFT_DARKGREY);
    gfx.setTextDatum(TL_DATUM);
    gfx.drawString(actionHintText(), 6, y + 4, 1);
    drawBackButton(gfx, gFocus == FocusItem::Back);
}

template <typename Canvas>
void drawSummaryPage(Canvas& gfx)
{
    gfx.fillRect(0, 0, gfx.width(), gfx.height(), TFT_BLACK);
    drawHeader(gfx, "T-Embed TFT Test", TFT_CYAN);

    const int16_t footerY = gfx.height() - kFooterHeight;
    gfx.setTextColor(TFT_WHITE, TFT_BLACK);
    gfx.drawString("Board: T-Embed PN532", 10, 40, 2);
    gfx.drawString("Panel: ST7789 170x320", 10, 58, 2);
    gfx.drawString("Reset: XL9555", 10, 76, 2);
    gfx.drawString(String("Rot: ") + String(gSettings.rotation) + "  Auto: on", 10, 94, 2);

    const uint16_t swatchColors[] = {
        TFT_RED, TFT_ORANGE, TFT_YELLOW, TFT_GREEN, TFT_CYAN, TFT_BLUE
    };
    int16_t swatchY = footerY - 42;
    if (swatchY > 114) {
        swatchY = 114;
    }
    if (swatchY < 106) {
        swatchY = 106;
    }

    const int16_t swatchW = (gfx.width() - 26) / 6;
    for (uint8_t i = 0; i < 6; ++i) {
        gfx.fillRoundRect(10 + i * swatchW, swatchY, swatchW - 4, 20, 6, swatchColors[i]);
    }

    gfx.setTextColor(TFT_GREEN, TFT_BLACK);
    gfx.drawString("Auto cycle | USER retest", 10, swatchY + 24, 1);

    drawFooter(gfx);
}

template <typename Canvas>
void drawColorBarsPage(Canvas& gfx)
{
    static const uint16_t colors[] = {
        TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW, TFT_CYAN, TFT_MAGENTA, TFT_WHITE
    };
    static const char* labels[] = {
        "RED", "GREEN", "BLUE", "YELLOW", "CYAN", "MAGENTA", "WHITE"
    };

    gfx.fillRect(0, 0, gfx.width(), gfx.height(), TFT_BLACK);
    drawHeader(gfx, "Color Bars", TFT_GREEN);

    const int16_t top = 34;
    const int16_t barHeight = (gfx.height() - top - 20) / 7;
    for (uint8_t i = 0; i < 7; ++i) {
        const int16_t y = top + i * barHeight;
        gfx.fillRect(0, y, gfx.width(), barHeight, colors[i]);
        gfx.setTextColor((colors[i] == TFT_WHITE || colors[i] == TFT_YELLOW)
                             ? TFT_BLACK
                             : TFT_WHITE,
                         colors[i]);
        gfx.drawString(labels[i], 10, y + 4, 2);
    }

    drawFooter(gfx);
}

template <typename Canvas>
void drawGeometryPage(Canvas& gfx)
{
    gfx.fillRect(0, 0, gfx.width(), gfx.height(), TFT_NAVY);
    drawHeader(gfx, "Geometry", TFT_YELLOW);

    const int16_t top = 34;
    for (int16_t x = 0; x < gfx.width(); x += 20) {
        gfx.drawFastVLine(x, top, gfx.height() - top - 18, TFT_DARKGREY);
    }
    for (int16_t y = top; y < gfx.height() - 18; y += 20) {
        gfx.drawFastHLine(0, y, gfx.width(), TFT_DARKGREY);
    }

    gfx.drawRect(8, top + 8, gfx.width() - 16, gfx.height() - top - 34, TFT_WHITE);
    gfx.drawLine(8, top + 8, gfx.width() - 9, gfx.height() - 27, TFT_RED);
    gfx.drawLine(gfx.width() - 9, top + 8, 8, gfx.height() - 27, TFT_CYAN);
    gfx.drawCircle(gfx.width() / 2, top + 42, 28, TFT_GREEN);
    gfx.fillCircle(gfx.width() / 2, top + 42, 8, TFT_GREEN);
    gfx.drawRoundRect(18, gfx.height() - 72, 88, 34, 8, TFT_ORANGE);
    gfx.fillRoundRect(gfx.width() - 110, gfx.height() - 72, 92, 34, 10, TFT_MAGENTA);
    gfx.setTextColor(TFT_WHITE, TFT_NAVY);
    gfx.drawString("grid / lines / circles", 16, gfx.height() - 98, 2);

    drawFooter(gfx);
}

template <typename Canvas>
void drawTextPage(Canvas& gfx)
{
    gfx.fillRect(0, 0, gfx.width(), gfx.height(), TFT_BLACK);
    drawHeader(gfx, "Text Rendering", TFT_MAGENTA);

    gfx.setTextColor(TFT_WHITE, TFT_BLACK);
    gfx.drawString("Font 2: quick status text", 10, 40, 2);

    gfx.setTextColor(TFT_CYAN, TFT_BLACK);
    gfx.drawString("T-Embed", 10, 60, 4);

    gfx.setTextColor(TFT_YELLOW, TFT_BLACK);
    gfx.drawString("170x320 ST7789", 10, 94, 2);

    gfx.setTextColor(TFT_GREEN, TFT_BLACK);
    gfx.drawString("RGB565 palette check", 10, 112, 2);

    gfx.setTextColor(TFT_WHITE, TFT_BLACK);
    gfx.drawString("0123456789  +-.", 10, 130, 2);

    drawFooter(gfx);
}

template <typename Canvas>
void drawBall(Canvas& gfx, const uint16_t color)
{
    gfx.fillCircle(gBall.x, gBall.y, gBall.radius, color);
}

template <typename Canvas>
void drawAnimationPage(Canvas& gfx)
{
    gfx.fillRect(0, 0, gfx.width(), gfx.height(), TFT_BLACK);
    drawHeader(gfx, "Animation", TFT_ORANGE);
    gfx.drawRoundRect(12, 40, gfx.width() - 24, gfx.height() - 64, 12, TFT_WHITE);
    drawBall(gfx, TFT_YELLOW);
    gfx.setTextColor(TFT_WHITE, TFT_BLACK);
    gfx.drawString("Ball checks refresh/fill.", 18, gfx.height() - 42, 1);

    drawFooter(gfx);
}

template <typename Canvas>
void drawCurrentPage(Canvas& gfx)
{
    switch (gCurrentPage) {
        case DemoPage::Summary:
            drawSummaryPage(gfx);
            break;
        case DemoPage::ColorBars:
            drawColorBarsPage(gfx);
            break;
        case DemoPage::Geometry:
            drawGeometryPage(gfx);
            break;
        case DemoPage::Text:
            drawTextPage(gfx);
            break;
        case DemoPage::Animation:
            drawAnimationPage(gfx);
            break;
        case DemoPage::Count:
        default:
            break;
    }
}

void redrawScreen()
{
    if (gCanvasReady) {
        drawCurrentPage(gCanvas);
        gCanvas.pushSprite(0, 0);
    } else {
        drawCurrentPage(tft);
    }
}

void maybeAdvancePage()
{
    if (!gAutoAdvance) {
        return;
    }
    if (millis() - gPageChangedAtMs < kAutoAdvanceMs) {
        return;
    }
    showPage(nextPage(gCurrentPage, 1));
}

void animateBall()
{
    if (gCurrentPage != DemoPage::Animation) {
        return;
    }

    const uint32_t now = millis();
    if (gLastAnimationMs != 0U && (now - gLastAnimationMs) < kAnimationFrameMs) {
        return;
    }
    gLastAnimationMs = now;

    const int16_t left   = 24 + gBall.radius;
    const int16_t right  = tft.width() - 24 - gBall.radius;
    const int16_t top    = 52 + gBall.radius;
    const int16_t bottom = tft.height() - 34 - gBall.radius;

    gBall.x += gBall.vx;
    gBall.y += gBall.vy;

    if (gBall.x <= left || gBall.x >= right) {
        gBall.vx = -gBall.vx;
        gBall.x += gBall.vx;
    }
    if (gBall.y <= top || gBall.y >= bottom) {
        gBall.vy = -gBall.vy;
        gBall.y += gBall.vy;
    }

    markDirty();
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
        restartDemo();
    }
}

}  // namespace

void init()
{
    gFocus = FocusItem::Demo;
    gAutoAdvance = true;
    gEncSnapshot = g.encRaw;
    gLastUiDrawMs = 0;
    gLastAnimationMs = 0;
    gPageChangedAtMs = 0;

    gCanvas.deleteSprite();
    gCanvas.setColorDepth(16);
    gCanvasReady = (gCanvas.createSprite(tft.width(), tft.height()) != nullptr);

    restartDemo();
}

void update()
{
    handleEncoder();
    handleButtons();
    if (g.subPageExitRequested) {
        return;
    }

    maybeAdvancePage();
    animateBall();
}

void render()
{
    if (!gScreenDirty) {
        return;
    }

    const uint32_t now = millis();
    if (gLastUiDrawMs != 0U && (now - gLastUiDrawMs) < kUiFrameIntervalMs) {
        return;
    }

    redrawScreen();
    gScreenDirty = false;
    gLastUiDrawMs = now;
}

void deinit()
{
    gCanvas.deleteSprite();
    gCanvasReady = false;
    gScreenDirty = true;
}

}  // namespace page_tft
