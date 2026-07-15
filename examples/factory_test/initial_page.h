#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>

#include <TEmbedPins.h>

// These defaults keep the sketch buildable outside PlatformIO. The PlatformIO
// pre-build script replaces FACTORY_COMMIT_HASH with the current Git hash.
#ifndef FACTORY_SOFTWARE_VERSION
#define FACTORY_SOFTWARE_VERSION "1.0.0"
#endif

#ifndef FACTORY_COMMIT_HASH
#define FACTORY_COMMIT_HASH unknown
#endif

#define FACTORY_STRINGIFY_IMPL(value) #value
#define FACTORY_STRINGIFY(value) FACTORY_STRINGIFY_IMPL(value)

namespace factory_initial_page {

namespace {

constexpr uint32_t kRefreshIntervalMs = 250;

constexpr int16_t kHeaderH = 24;
constexpr int16_t kFooterH = 18;
constexpr int16_t kMargin = 8;

constexpr uint16_t kUiBg    = 0x0841;
constexpr uint16_t kUiCard  = 0x18C3;
constexpr uint16_t kUiEdge  = 0x31A6;
constexpr uint16_t kAccent  = 0x55FF;
constexpr uint16_t kPassBg  = 0x0A41;
constexpr uint16_t kFailBg  = 0x3006;
constexpr uint16_t kWarnBg  = 0x6320;
constexpr uint16_t kUiMuted = 0x9CD3;

struct I2cDevice {
    const char* label;
    uint8_t address;
    bool detected;
};

I2cDevice gI2cDevices[] = {
    {"XL9555",  BOARD_I2C_XL9555, false},
    {"SY6970",  BOARD_I2C_SY6970, false},
    {"BQ27220", BOARD_I2C_BQ27220, false},
};

bool gActive = false;
bool gHardwareOk = false;
bool gExpanderReady = false;
bool gLowPowerReady = false;
int32_t gEncoderStart = 0;
bool gDirty = true;
uint32_t gLastDrawMs = 0;

String clipText(const String& text, const uint8_t maxChars)
{
    if (text.length() <= maxChars || maxChars < 4) {
        return text;
    }
    return text.substring(0, maxChars - 3) + "...";
}

bool probeI2cAddress(TwoWire& wire, const uint8_t address)
{
    // An ACK is the only generic I2C probe that works for all three devices;
    // their register maps and read protocols are different.
    wire.beginTransmission(address);
    return wire.endTransmission(true) == 0;
}

void scanI2cDevices()
{
    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
    delay(2);

    for (I2cDevice& device : gI2cDevices) {
        device.detected = probeI2cAddress(Wire, device.address);
    }
}

bool allRequiredI2cDetected()
{
    for (const I2cDevice& device : gI2cDevices) {
        if (!device.detected) {
            return false;
        }
    }
    return true;
}

bool anyRequiredI2cDetected()
{
    for (const I2cDevice& device : gI2cDevices) {
        if (device.detected) {
            return true;
        }
    }
    return false;
}

bool controlReady()
{
    return gExpanderReady && gLowPowerReady;
}

const char* statusText(const bool ok)
{
    return ok ? "OK" : "FAIL";
}

uint16_t statusColor(const bool ok)
{
    return ok ? TFT_GREEN : TFT_RED;
}

String hardwareVersionText()
{
    if (allRequiredI2cDetected()) {
        return "T-Embed-CC1101 V1.1";
    }
    if (anyRequiredI2cDetected()) {
        return "I2C mismatch";
    }
    return "Not identified";
}

String buildStampText()
{
    return String(__DATE__) + " " + __TIME__;
}

const char* badgeText()
{
    if (gHardwareOk) {
        return "V1.1 OK";
    }
    if (!anyRequiredI2cDetected()) {
        return "NO I2C";
    }
    if (!allRequiredI2cDetected()) {
        return "CHECK I2C";
    }
    return "CTRL FAIL";
}

uint16_t badgeFg()
{
    if (gHardwareOk) {
        return TFT_GREEN;
    }
    if (!allRequiredI2cDetected() && anyRequiredI2cDetected()) {
        return TFT_YELLOW;
    }
    return TFT_RED;
}

uint16_t badgeBg()
{
    if (gHardwareOk) {
        return kPassBg;
    }
    if (!allRequiredI2cDetected() && anyRequiredI2cDetected()) {
        return kWarnBg;
    }
    return kFailBg;
}

String supportText()
{
    if (gHardwareOk) {
        return "Hardware check passed.";
    }
    if (!anyRequiredI2cDetected()) {
        return "No I2C devices found. Main menu is blocked.";
    }
    if (!allRequiredI2cDetected()) {
        return "Required I2C device missing. Main menu is blocked.";
    }
    return "Board control init failed. Main menu is blocked.";
}

String footerText()
{
    if (gHardwareOk) {
        return "USER/BOOT/ENC to enter main menu";
    }
    return "TEST HARDWARE ERROR - main menu blocked";
}

template <typename Canvas>
void drawHeader(Canvas& gfx)
{
    const uint16_t bg = gHardwareOk ? TFT_NAVY : 0x8000;

    gfx.fillRect(0, 0, gfx.width(), kHeaderH, bg);
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_WHITE, bg);
    gfx.drawString("Factory Startup", kMargin, 5, 2);
    gfx.setTextDatum(TR_DATUM);
    gfx.setTextColor(gHardwareOk ? kAccent : TFT_YELLOW, bg);
    gfx.drawString(gHardwareOk ? "T-Embed CC1101" : "HARDWARE ERROR",
                   gfx.width() - kMargin, 7, 1);
    gfx.setTextDatum(TL_DATUM);
}

template <typename Canvas>
void drawFooter(Canvas& gfx)
{
    const int16_t y = gfx.height() - kFooterH;
    gfx.fillRect(0, y, gfx.width(), kFooterH, TFT_DARKGREY);
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(gHardwareOk ? TFT_WHITE : TFT_YELLOW, TFT_DARKGREY);
    gfx.drawString(clipText(footerText(), 42), kMargin, y + 5, 1);
}

template <typename Canvas>
void drawStatusBadge(Canvas& gfx)
{
    constexpr int16_t x = 8;
    constexpr int16_t y = 34;
    constexpr int16_t w = 92;
    constexpr int16_t h = 20;
    const uint16_t bg = badgeBg();
    const uint16_t fg = badgeFg();

    gfx.fillRoundRect(x, y, w, h, 6, bg);
    gfx.drawRoundRect(x, y, w, h, 6, fg);
    gfx.setTextDatum(TC_DATUM);
    gfx.setTextColor(fg, bg);
    gfx.drawString(badgeText(), x + w / 2, y + 5, 1);
    gfx.setTextDatum(TL_DATUM);
}

template <typename Canvas>
void drawInfoCard(Canvas& gfx)
{
    const int16_t x = 8;
    const int16_t y = 58;
    const int16_t w = gfx.width() - 16;
    constexpr int16_t h = 80;
    const int16_t labelX = x + 10;
    const int16_t valueX = x + 50;
    const int16_t row1Y = y + 34;
    const int16_t row2Y = y + 48;
    const int16_t row3Y = y + 62;

    gfx.fillRoundRect(x, y, w, h, 6, kUiCard);
    gfx.drawRoundRect(x, y, w, h, 6, kUiEdge);

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(gHardwareOk ? TFT_WHITE : TFT_YELLOW, kUiCard);
    gfx.drawString(String("HW: ") + hardwareVersionText(), x + 10, y + 8, 2);
    gfx.drawFastHLine(x + 10, y + 24, w - 20, kUiEdge);

    gfx.setTextColor(kUiMuted, kUiCard);
    gfx.drawString("SW:", labelX, row1Y, 1);
    gfx.drawString("Build:", labelX, row2Y, 1);
    gfx.drawString("Git:", labelX, row3Y, 1);

    gfx.setTextColor(TFT_WHITE, kUiCard);
    gfx.drawString(clipText(FACTORY_SOFTWARE_VERSION, 28), valueX, row1Y, 1);
    gfx.drawString(clipText(buildStampText(), 28), valueX, row2Y, 1);
    gfx.drawString(clipText(FACTORY_STRINGIFY(FACTORY_COMMIT_HASH), 28),
                   valueX, row3Y, 1);
}

template <typename Canvas>
void drawProbeLines(Canvas& gfx)
{
    char probeLine[80];
    snprintf(probeLine, sizeof(probeLine), "I2C: 22[%c] 6A[%c] 55[%c] CTRL[%c]",
             gI2cDevices[0].detected ? 'Y' : '-',
             gI2cDevices[1].detected ? 'Y' : '-',
             gI2cDevices[2].detected ? 'Y' : '-',
             controlReady() ? 'Y' : '-');

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(kUiMuted, kUiBg);
    gfx.drawString(probeLine, kMargin, 142, 1);

    gfx.setTextColor(gHardwareOk ? TFT_GREEN : TFT_RED, kUiBg);
    gfx.drawString(clipText(supportText(), 32), 106, 38, 1);
}

template <typename Canvas>
void drawUi(Canvas& gfx)
{
    gfx.fillScreen(kUiBg);
    drawHeader(gfx);
    drawStatusBadge(gfx);
    drawInfoCard(gfx);
    drawProbeLines(gfx);
    drawFooter(gfx);
}

void drawPage()
{
    drawUi(tft);
}

}  // namespace

void begin(const bool expanderReady, const bool lowPowerReady)
{
    gExpanderReady = expanderReady;
    gLowPowerReady = lowPowerReady;
    scanI2cDevices();

    gHardwareOk = gExpanderReady && gLowPowerReady &&
                  gI2cDevices[0].detected &&
                  gI2cDevices[1].detected &&
                  gI2cDevices[2].detected;
    gEncoderStart = g.encRaw;
    gActive = true;
    gDirty = true;
    gLastDrawMs = 0;

    Serial.printf("[INIT] XL9555(0x%02X)=%s, SY6970(0x%02X)=%s, "
                  "BQ27220(0x%02X)=%s\n",
                  BOARD_I2C_XL9555, statusText(gI2cDevices[0].detected),
                  BOARD_I2C_SY6970, statusText(gI2cDevices[1].detected),
                  BOARD_I2C_BQ27220, statusText(gI2cDevices[2].detected));
    if (!gHardwareOk) {
        Serial.println(F("[INIT] Hardware check failed; main menu is locked."));
    }
}

bool isActive()
{
    return gActive;
}

bool hardwareOk()
{
    return gHardwareOk;
}

void update()
{
    if (!gActive || !gHardwareOk) {
        return;
    }

    const bool buttonPressed = g.encBtn.event || g.usrBtn.event;
    const bool encoderMoved = g.encRaw != gEncoderStart;
    if (!buttonPressed && !encoderMoved) {
        return;
    }

    g.encBtn.event = false;
    g.usrBtn.event = false;
    g.encLast = g.encRaw;
    g.encActivitySnapshot = g.encRaw;
    gActive = false;
    Serial.println(F("[INIT] Hardware check passed; entering main menu."));
}

void render()
{
    if (!gActive) {
        return;
    }

    const uint32_t now = millis();
    if (!gDirty && (now - gLastDrawMs) < kRefreshIntervalMs) {
        return;
    }

    drawPage();
    t_embed::board::deselectSharedSpiDevices();
    gDirty = false;
    gLastDrawMs = now;
}

}  // namespace factory_initial_page
