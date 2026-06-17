#pragma once
#include <TFT_eSPI.h>

namespace page_setting {

namespace {

constexpr uint32_t kUiFrameIntervalMs = 33;
constexpr uint32_t kInfoRefreshMs     = 1500;

constexpr int16_t kUiMargin     = 8;
constexpr int16_t kHeaderHeight = 24;
constexpr int16_t kFooterHeight = 18;
constexpr int16_t kBackBtnW     = 58;
constexpr int16_t kBackBtnH     = 14;

constexpr int16_t kMenuX     = 8;
constexpr int16_t kMenuY     = 34;
constexpr int16_t kMenuW     = 104;
constexpr int16_t kMenuH     = 20;
constexpr int16_t kMenuGap   = 6;

constexpr int16_t kDetailX   = 120;
constexpr int16_t kDetailY   = 34;
constexpr int16_t kDetailW   = 192;
constexpr int16_t kDetailH   = 114;

constexpr uint16_t kColorBg        = 0x0841;
constexpr uint16_t kColorPanel     = 0x1082;
constexpr uint16_t kColorPanelEdge = 0x31A6;
constexpr uint16_t kColorCard      = 0x18C3;
constexpr uint16_t kColorPassBg    = 0x0A41;
constexpr uint16_t kColorWarnBg    = 0x6300;

enum class FocusItem : uint8_t {
    DeviceInfo = 0,
    Rotation,
    SleepNow,
    AutoOff,
    Back,
    kCount,
};

TFT_eSprite gCanvas(&tft);
FocusItem   gFocus         = FocusItem::DeviceInfo;
bool        gCanvasReady   = false;
bool        gScreenDirty   = true;
uint32_t    gLastUiDrawMs  = 0;
uint32_t    gLastInfoPollMs = 0;
int32_t     gEncSnapshot   = 0;

String      gChipModel;
uint32_t    gCpuMhz        = 0;
uint32_t    gFlashMB       = 0;
uint32_t    gPsramKB       = 0;
String      gBuildDate;
String      gBuildTime;
uint32_t    gFreeHeap      = 0;
uint32_t    gMinHeap       = 0;

void markDirty()
{
    gScreenDirty = true;
}

void refreshInfo()
{
    gFreeHeap = ESP.getFreeHeap();
    gMinHeap = ESP.getMinFreeHeap();
    gLastInfoPollMs = millis();
    markDirty();
}

const char* itemLabel(const FocusItem item)
{
    switch (item) {
        case FocusItem::DeviceInfo: return "Device Info";
        case FocusItem::Rotation:   return "Rotation";
        case FocusItem::SleepNow:   return "Sleep";
        case FocusItem::AutoOff:    return "Auto Power";
        case FocusItem::Back:       return "Back";
        default:                    return "";
    }
}

uint16_t itemAccent(const FocusItem item)
{
    switch (item) {
        case FocusItem::DeviceInfo: return TFT_CYAN;
        case FocusItem::Rotation:   return TFT_YELLOW;
        case FocusItem::SleepNow:   return TFT_ORANGE;
        case FocusItem::AutoOff:    return TFT_GREEN;
        case FocusItem::Back:       return TFT_WHITE;
        default:                    return TFT_LIGHTGREY;
    }
}

const char* footerText()
{
    switch (gFocus) {
        case FocusItem::DeviceInfo: return "Info view";
        case FocusItem::Rotation:   return "BOOT=rotate";
        case FocusItem::SleepNow:   return "BOOT=sleep";
        case FocusItem::AutoOff:    return "BOOT=cycle timer";
        case FocusItem::Back:       return "BOOT=back";
        default:                    return "";
    }
}

String fitText(const String& text, const uint8_t maxChars)
{
    if (text.length() <= maxChars) {
        return text;
    }
    if (maxChars <= 3) {
        return text.substring(0, maxChars);
    }
    return text.substring(0, maxChars - 3) + "...";
}

String heapText(const uint32_t bytes)
{
    const uint32_t kb = (bytes + 512UL) / 1024UL;
    return String(kb) + " KB";
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
    gfx.drawString("Settings", kUiMargin, 5, 2);

    gfx.setTextDatum(TR_DATUM);
    gfx.setTextColor(TFT_CYAN, TFT_NAVY);
    gfx.drawString("Factory tools", gfx.width() - kUiMargin, 7, 1);
    gfx.setTextDatum(TL_DATUM);
}

template <typename Canvas>
void drawFooter(Canvas& gfx)
{
    const int16_t y = gfx.height() - kFooterHeight;
    gfx.fillRect(0, y, gfx.width(), kFooterHeight, TFT_DARKGREY);
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_WHITE, TFT_DARKGREY);
    gfx.drawString(footerText(), 8, y + 4, 1);
    drawBackButton(gfx, gFocus == FocusItem::Back);
}

template <typename Canvas>
void drawMenuCard(Canvas& gfx, const FocusItem item, const int16_t y)
{
    const bool selected = (gFocus == item);
    const uint16_t accent = itemAccent(item);
    const uint16_t fill = selected ? accent : kColorCard;
    const uint16_t textColor = selected ? TFT_BLACK : TFT_WHITE;

    gfx.fillRoundRect(kMenuX, y, kMenuW, kMenuH, 6, fill);
    gfx.drawRoundRect(kMenuX, y, kMenuW, kMenuH, 6,
                      selected ? accent : kColorPanelEdge);

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(textColor, fill);
    gfx.drawString(itemLabel(item), kMenuX + 8, y + 6, 1);
}

template <typename Canvas>
void drawMenu(Canvas& gfx)
{
    drawMenuCard(gfx, FocusItem::DeviceInfo, kMenuY);
    drawMenuCard(gfx, FocusItem::Rotation,   kMenuY + (kMenuH + kMenuGap));
    drawMenuCard(gfx, FocusItem::SleepNow,   kMenuY + (kMenuH + kMenuGap) * 2);
    drawMenuCard(gfx, FocusItem::AutoOff,    kMenuY + (kMenuH + kMenuGap) * 3);
}

template <typename Canvas>
void drawDetailRow(Canvas& gfx, const int16_t y,
                   const char* label, const String& value, const uint16_t valueColor)
{
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_CYAN, kColorPanel);
    gfx.drawString(label, kDetailX + 10, y, 1);
    gfx.setTextColor(valueColor, kColorPanel);
    gfx.drawString(value, kDetailX + 62, y, 1);
}

template <typename Canvas>
void drawDeviceInfoDetail(Canvas& gfx)
{
    char buf[24];
    int16_t y = kDetailY + 8;
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_CYAN, kColorPanel);
    gfx.drawString("Device Snapshot", kDetailX + 10, y, 2);
    y += 16;

    drawDetailRow(gfx, y, "Chip", fitText(gChipModel, 14), TFT_WHITE); y += 11;
    snprintf(buf, sizeof(buf), "%lu MHz", static_cast<unsigned long>(gCpuMhz));
    drawDetailRow(gfx, y, "CPU", String(buf), TFT_WHITE); y += 11;
    snprintf(buf, sizeof(buf), "%lu MB", static_cast<unsigned long>(gFlashMB));
    drawDetailRow(gfx, y, "Flash", String(buf), TFT_WHITE); y += 11;
    if (gPsramKB > 0) {
        snprintf(buf, sizeof(buf), "%lu KB", static_cast<unsigned long>(gPsramKB));
        drawDetailRow(gfx, y, "PSRAM", String(buf), TFT_WHITE); y += 11;
    }
    drawDetailRow(gfx, y, "Heap", heapText(gFreeHeap),
                  gFreeHeap > 50000 ? TFT_GREEN : TFT_YELLOW); y += 11;
    drawDetailRow(gfx, y, "Min", heapText(gMinHeap), TFT_LIGHTGREY); y += 11;
    drawDetailRow(gfx, y, "Date", fitText(gBuildDate, 14), TFT_LIGHTGREY); y += 11;
    drawDetailRow(gfx, y, "Time", gBuildTime, TFT_LIGHTGREY);
}

template <typename Canvas>
void drawRotationDetail(Canvas& gfx)
{
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_YELLOW, kColorPanel);
    gfx.drawString("Screen Rotation", kDetailX + 10, kDetailY + 10, 2);

    gfx.setTextDatum(MC_DATUM);
    gfx.setTextColor(TFT_WHITE, kColorPanel);
    gfx.drawString(currentRotationLabel(), kDetailX + kDetailW / 2, kDetailY + 48, 4);

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_LIGHTGREY, kColorPanel);
    gfx.drawString("Wide layout only.", kDetailX + 10, kDetailY + 76, 1);
    gfx.drawString("BOOT toggles 1 / 3.", kDetailX + 10, kDetailY + 88, 1);
}

template <typename Canvas>
void drawSleepDetail(Canvas& gfx)
{
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_ORANGE, kColorPanel);
    gfx.drawString("Deep Sleep", kDetailX + 10, kDetailY + 10, 2);

    gfx.setTextDatum(MC_DATUM);
    gfx.setTextColor(TFT_WHITE, kColorPanel);
    gfx.drawString("SLEEP", kDetailX + kDetailW / 2, kDetailY + 48, 4);

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_LIGHTGREY, kColorPanel);
    gfx.drawString("Turns off LCD / audio /", kDetailX + 10, kDetailY + 74, 1);
    gfx.drawString("peripheral 3V3 rail.", kDetailX + 10, kDetailY + 86, 1);
    gfx.drawString("Wake with USER key.", kDetailX + 10, kDetailY + 98, 1);
}

template <typename Canvas>
void drawAutoOffDetail(Canvas& gfx)
{
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_GREEN, kColorPanel);
    gfx.drawString("Auto Power", kDetailX + 10, kDetailY + 10, 2);

    gfx.setTextDatum(MC_DATUM);
    gfx.setTextColor(TFT_WHITE, kColorPanel);
    gfx.drawString(autoSleepPresetLabel(), kDetailX + kDetailW / 2, kDetailY + 44, 4);

    gfx.setTextDatum(TL_DATUM);
    drawDetailRow(gfx, kDetailY + 68, "Dim", autoDimTimeoutLabel(), TFT_YELLOW);
    drawDetailRow(gfx, kDetailY + 82, "Sleep", autoSleepPresetLabel(), TFT_GREEN);
    gfx.setTextColor(TFT_LIGHTGREY, kColorPanel);
    gfx.drawString("Global idle timer.", kDetailX + 10, kDetailY + 96, 1);
}

template <typename Canvas>
void drawBackDetail(Canvas& gfx)
{
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_WHITE, kColorPanel);
    gfx.drawString("Return to Menu", kDetailX + 10, kDetailY + 10, 2);

    gfx.setTextDatum(MC_DATUM);
    gfx.setTextColor(TFT_CYAN, kColorPanel);
    gfx.drawString("BACK", kDetailX + kDetailW / 2, kDetailY + 48, 4);

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_LIGHTGREY, kColorPanel);
    gfx.drawString("Turn to BACK,", kDetailX + 10, kDetailY + 80, 1);
    gfx.drawString("then press BOOT.", kDetailX + 10, kDetailY + 92, 1);
}

template <typename Canvas>
void drawDetailPanel(Canvas& gfx)
{
    const uint16_t accent = itemAccent(gFocus);
    gfx.fillRoundRect(kDetailX, kDetailY, kDetailW, kDetailH, 8, kColorPanel);
    gfx.drawRoundRect(kDetailX, kDetailY, kDetailW, kDetailH, 8, accent);

    switch (gFocus) {
        case FocusItem::DeviceInfo:
            drawDeviceInfoDetail(gfx);
            break;
        case FocusItem::Rotation:
            drawRotationDetail(gfx);
            break;
        case FocusItem::SleepNow:
            drawSleepDetail(gfx);
            break;
        case FocusItem::AutoOff:
            drawAutoOffDetail(gfx);
            break;
        case FocusItem::Back:
            drawBackDetail(gfx);
            break;
        default:
            break;
    }
}

template <typename Canvas>
void drawUi(Canvas& gfx)
{
    gfx.fillRect(0, 0, gfx.width(), gfx.height(), kColorBg);
    drawHeader(gfx);
    drawMenu(gfx);
    drawDetailPanel(gfx);
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

void handleEncoderFocus()
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
    if (g.usrBtn.event) {
        g.usrBtn.event = false;
    }

    if (!g.encBtn.event) {
        return;
    }

    g.encBtn.event = false;
    switch (gFocus) {
        case FocusItem::Rotation:
            cycleDisplayRotation();
            markDirty();
            break;
        case FocusItem::SleepNow:
            markDirty();
            requestSystemSleep();
            break;
        case FocusItem::AutoOff:
            cycleAutoSleepPreset();
            markDirty();
            break;
        case FocusItem::Back:
            requestExitSubPage();
            break;
        case FocusItem::DeviceInfo:
        default:
            refreshInfo();
            break;
    }
}

}  // namespace

void init()
{
    gFocus = FocusItem::DeviceInfo;
    gCanvasReady = false;
    gScreenDirty = true;
    gLastUiDrawMs = 0;
    gLastInfoPollMs = 0;
    gEncSnapshot = g.encRaw;

    g.encBtn.event = false;
    g.usrBtn.event = false;

    gChipModel = String(ESP.getChipModel());
    gCpuMhz = ESP.getCpuFreqMHz();
    gFlashMB = ESP.getFlashChipSize() / (1024UL * 1024UL);
    gPsramKB = ESP.getPsramSize() / 1024UL;
    gBuildDate = String(__DATE__);
    gBuildTime = String(__TIME__);
    gFreeHeap = ESP.getFreeHeap();
    gMinHeap = ESP.getMinFreeHeap();

    gCanvas.deleteSprite();
    gCanvas.setColorDepth(16);
    gCanvasReady = (gCanvas.createSprite(tft.width(), tft.height()) != nullptr);
    if (!gCanvasReady) {
        Serial.println(F("[SET] Sprite allocation failed, using direct TFT redraw."));
    }
}

void update()
{
    handleEncoderFocus();
    handleButtons();

    const uint32_t now = millis();
    if ((now - gLastInfoPollMs) >= kInfoRefreshMs) {
        refreshInfo();
    }
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
    gCanvas.deleteSprite();
    gCanvasReady = false;
    gScreenDirty = true;
    g.encBtn.event = false;
    g.usrBtn.event = false;
}

}  // namespace page_setting
