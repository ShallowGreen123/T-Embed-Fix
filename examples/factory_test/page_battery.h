#pragma once
#include <GaugeBQ27220.hpp>
#include <bq27220.h>

#define XPOWERS_CHIP_SY6970
#include <XPowersLib.h>

namespace page_battery {

namespace {

constexpr uint32_t kPollIntervalMs = 250;
constexpr uint32_t kUiStateDebounceMs = 400;
constexpr uint32_t kChargingStateHoldMs = 1800;
constexpr uint32_t kExternalPowerHoldMs = 1800;
constexpr uint32_t kChargeTopOffRetryMs = 60000;
constexpr uint32_t kShutdownHoldMs = 2000;
constexpr uint32_t kTransientDetailMs = 2500;

constexpr uint16_t kBatteryCapacityMah = 1300;
constexpr uint16_t kChargeTargetVoltageMv = 4208;
constexpr uint16_t kPrechargeCurrentMa = 128;
constexpr uint16_t kFastChargeCurrentMa = 512;
constexpr uint16_t kTerminationCurrentMa = 128;
constexpr uint16_t kSysPowerDownVoltageMv = 3300;
constexpr uint16_t kInputCurrentSdpMa = 500;
constexpr uint16_t kInputCurrentAdapterMa = 1000;
constexpr uint16_t kVbusPresentThresholdMv = 3900;
constexpr uint16_t kChargeDoneSocThreshold = 99;
constexpr uint16_t kRechargeThresholdOffsetMv = 100;
constexpr uint16_t kChargeTopOffRestartMarginMv = 32;
constexpr uint16_t kBqRequestedChargeCurrentMa = kFastChargeCurrentMa;
constexpr uint16_t kBqRequestedChargeVoltageMv = kChargeTargetVoltageMv;
constexpr uint16_t kBqTaperCurrentMa = kTerminationCurrentMa;
constexpr uint16_t kBqChargeTerminationVoltageMv = 100;
constexpr uint16_t kBqChargeDetectThresholdMa = 75;
constexpr uint16_t kBqQuitCurrentMa = 40;
constexpr int16_t kChargeCurrentIntoBatteryThresholdMa = 30;
constexpr int16_t kDischargeCurrentThresholdMa = -30;

constexpr int16_t kHeaderHeight = 24;
constexpr int16_t kFooterHeight = 18;
constexpr int16_t kLeftX = 8;
constexpr int16_t kLeftValueX = 62;
constexpr int16_t kLeftValueW = 96;
constexpr int16_t kRightX = 162;
constexpr int16_t kRightValueX = 222;
constexpr int16_t kRightValueW = 94;
constexpr int16_t kRow0 = 80;
constexpr int16_t kRowGap = 14;
constexpr int16_t kStateX = 8;
constexpr int16_t kStateY = 30;
constexpr int16_t kStateW = 220;
constexpr int16_t kStateH = 26;
constexpr int16_t kDetailX = 8;
constexpr int16_t kDetailY = 64;
constexpr int16_t kDetailW = 304;
constexpr int16_t kDetailH = 10;
constexpr int16_t kValueFieldH = 10;
constexpr int16_t kBackBtnW = 58;
constexpr int16_t kBackBtnH = 14;

enum class UiState : uint8_t {
    Init = 0,
    Charging,
    ChargeDone,
    Discharging,
    Idle,
    BqConfigWarn,
    Error,
};

enum class FocusItem : uint8_t {
    Metrics = 0,
    Back,
    kCount,
};

struct BatteryMetrics {
    uint16_t soc = 0;
    uint16_t bqVoltageMv = 0;
    int16_t ibatMa = 0;
    uint16_t remainMah = 0;
    uint16_t fullMah = 0;
    uint16_t designMah = 0;
    uint16_t soh = 0;
    int16_t tempDeciC = 0;
    uint16_t tteMin = 0;
    uint16_t ttfMin = 0;
    uint16_t syBattVoltageMv = 0;
    uint16_t vbusMv = 0;
    uint16_t vsysMv = 0;
    uint16_t chargeCurrentMa = 0;
    uint8_t busStatus = 0;
    uint8_t chargeStatus = 0;
    uint8_t faultStatus = 0;
    bool chargeEnabled = false;
    bool powerGood = false;
    bool hizMode = false;
    bool watchdogFault = false;
    bool boostFault = false;
    bool chargeFault = false;
    bool batteryFault = false;
    bool ntcFault = false;
    bool bqFullChargeDetected = false;
    bool isDischarging = false;
    bool vbusPresent = false;
};

GaugeBQ27220 gauge;
BQ27220 gaugeDm;
XPowersPPM pmu;

UiState gUiState = UiState::Init;
UiState gPendingUiState = UiState::Init;
BatteryMetrics gMetrics;
BatteryMetrics gLastDrawnMetrics;
bool gHasMetrics = false;
bool gHasDrawnMetrics = false;
bool gGaugeOk = false;
bool gPmuOk = false;
bool gReady = false;
bool gScreenDirty = true;
bool gBqConfigWarning = false;
bool gFrameDrawn = false;
bool gFrameShowsError = false;
bool gShutdownTracking = false;
bool gShutdownHandled = false;
String gBqConfigDetail = "BQ cfg pending";
String gTransientDetail;
String gErrorDetail;
String gLastDrawnDetail;
UiState gLastDrawnUiState = UiState::Init;
FocusItem gFocus = FocusItem::Metrics;
FocusItem gLastDrawnFocus = FocusItem::Metrics;
String gSerialLine;
unsigned long gDetailExpiresAtMs = 0;
unsigned long gLastPollAtMs = 0;
unsigned long gPendingUiStateSinceMs = 0;
unsigned long gLastChargingEvidenceAtMs = 0;
unsigned long gLastExternalPowerSeenAtMs = 0;
unsigned long gLastChargeTopOffAttemptMs = 0;
unsigned long gUsrPressedAtMs = 0;
int16_t gAppliedInputCurrentMa = -1;
uint8_t gLatchedInputBusStatus = static_cast<uint8_t>(XPowersPPM::BUS_STATE_NOINPUT);
int32_t gEncSnapshot = 0;

BQ27220DMData makeDmU16Entry(const uint16_t address, const uint16_t value)
{
    BQ27220DMData entry = {};
    entry.type = BQ27220DMTypeU16;
    entry.address = address;
    entry.value.u16 = value;
    return entry;
}

BQ27220DMData makeDmEndEntry()
{
    BQ27220DMData entry = {};
    entry.type = BQ27220DMTypeEnd;
    return entry;
}

void printBqChargeConfigTargets()
{
    Serial.print(kBqRequestedChargeCurrentMa);
    Serial.print(F("/"));
    Serial.print(kBqRequestedChargeVoltageMv);
    Serial.print(F("/"));
    Serial.print(kBqTaperCurrentMa);
    Serial.print(F("/"));
    Serial.print(kBqChargeTerminationVoltageMv);
    Serial.print(F("/"));
    Serial.print(kBqChargeDetectThresholdMa);
    Serial.print(F("/"));
    Serial.println(kBqQuitCurrentMa);
}

const char* stateLabel(const UiState state)
{
    switch (state) {
        case UiState::Init:         return "INIT";
        case UiState::Charging:     return "CHARGING";
        case UiState::ChargeDone:   return "CHARGE DONE";
        case UiState::Discharging:  return "DISCHARGING";
        case UiState::Idle:         return "IDLE";
        case UiState::BqConfigWarn: return "BQ CFG WARN";
        case UiState::Error:        return "ERROR";
    }
    return "?";
}

uint16_t stateColor(const UiState state)
{
    switch (state) {
        case UiState::Init:         return TFT_CYAN;
        case UiState::Charging:     return TFT_GREEN;
        case UiState::ChargeDone:   return TFT_YELLOW;
        case UiState::Discharging:  return TFT_ORANGE;
        case UiState::Idle:         return TFT_LIGHTGREY;
        case UiState::BqConfigWarn: return TFT_RED;
        case UiState::Error:        return TFT_RED;
    }
    return TFT_WHITE;
}

const char* busStatusLabel(const uint8_t status)
{
    switch (static_cast<XPowersPPM::BusStatus>(status)) {
        case XPowersPPM::BUS_STATE_NOINPUT:             return "No input";
        case XPowersPPM::BUS_STATE_USB_SDP:             return "USB SDP";
        case XPowersPPM::BUS_STATE_USB_CDP:             return "USB CDP";
        case XPowersPPM::BUS_STATE_USB_DCP:             return "USB DCP";
        case XPowersPPM::BUS_STATE_HVDCP:               return "HVDCP";
        case XPowersPPM::BUS_STATE_ADAPTER:             return "Adapter";
        case XPowersPPM::BUS_STATE_NO_STANDARD_ADAPTER: return "Adapter";
        case XPowersPPM::BUS_STATE_OTG:                 return "OTG";
    }
    return "Unknown";
}

const char* chargeStatusLabel(const uint8_t status)
{
    switch (static_cast<XPowersPPM::ChargeStatus>(status)) {
        case XPowersPPM::CHARGE_STATE_NO_CHARGE:   return "Not Charging";
        case XPowersPPM::CHARGE_STATE_PRE_CHARGE:  return "Pre-charge";
        case XPowersPPM::CHARGE_STATE_FAST_CHARGE: return "Fast Charging";
        case XPowersPPM::CHARGE_STATE_DONE:        return "Done";
        case XPowersPPM::CHARGE_STATE_UNKOWN:      return "Unknown";
    }
    return "Unknown";
}

String currentDetail()
{
    if (gDetailExpiresAtMs && millis() < gDetailExpiresAtMs) {
        return gTransientDetail;
    }
    if (gUiState == UiState::Error) {
        return gErrorDetail;
    }
    if (gHasMetrics) {
        if (gMetrics.hizMode) {
            return "SY6970 HIZ active";
        }
        if (gMetrics.watchdogFault ||
            gMetrics.boostFault ||
            gMetrics.chargeFault ||
            gMetrics.batteryFault ||
            gMetrics.ntcFault) {
            String detail = "PMU fault:";
            if (gMetrics.watchdogFault) {
                detail += " WDT";
            }
            if (gMetrics.boostFault) {
                detail += " BOOST";
            }
            if (gMetrics.chargeFault) {
                detail += " CHG";
            }
            if (gMetrics.batteryFault) {
                detail += " BAT";
            }
            if (gMetrics.ntcFault) {
                detail += " NTC";
            }
            return detail;
        }
    }
    return gBqConfigDetail;
}

void setTransientDetail(const String& detail, const uint32_t durationMs = kTransientDetailMs)
{
    gTransientDetail = detail;
    gDetailExpiresAtMs = millis() + durationMs;
    gScreenDirty = true;
}

String formatSignedMilliamp(const int16_t value)
{
    return String(value) + " mA";
}

String formatVoltage(const uint16_t value)
{
    if (!value) {
        return "-";
    }
    return String(value) + " mV";
}

String formatTimePair(const uint16_t tteMin, const uint16_t ttfMin)
{
    auto part = [](const uint16_t value) -> String {
        if (!value || value == 65535U) {
            return "-";
        }
        return String(value);
    };
    return part(tteMin) + " / " + part(ttfMin) + " min";
}

String formatTemp(const int16_t deciC)
{
    const bool negative = deciC < 0;
    const int16_t magnitude = negative ? -deciC : deciC;
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%s%d.%d C", negative ? "-" : "", magnitude / 10, magnitude % 10);
    return String(buffer);
}

String formatCapacityTriplet(const BatteryMetrics& metrics)
{
    return String(metrics.remainMah) + "/" + String(metrics.fullMah) + "/" + String(metrics.designMah) + " mAh";
}

uint16_t effectiveBatteryVoltageMv(const BatteryMetrics& metrics)
{
    return max(metrics.bqVoltageMv, metrics.syBattVoltageMv);
}

bool hasExternalPower()
{
    const auto bus = static_cast<XPowersPPM::BusStatus>(gMetrics.busStatus);
    return gMetrics.vbusPresent && bus != XPowersPPM::BUS_STATE_OTG;
}

bool isUsableInputBusStatus(const uint8_t status)
{
    const auto bus = static_cast<XPowersPPM::BusStatus>(status);
    return bus != XPowersPPM::BUS_STATE_NOINPUT &&
           bus != XPowersPPM::BUS_STATE_OTG;
}

bool hasExternalPower(const BatteryMetrics& metrics)
{
    return metrics.vbusPresent &&
           static_cast<XPowersPPM::BusStatus>(metrics.busStatus) != XPowersPPM::BUS_STATE_OTG;
}

bool hasChargingEvidence(const BatteryMetrics& metrics)
{
    const auto chargeStatus = static_cast<XPowersPPM::ChargeStatus>(metrics.chargeStatus);
    return chargeStatus == XPowersPPM::CHARGE_STATE_FAST_CHARGE ||
           chargeStatus == XPowersPPM::CHARGE_STATE_PRE_CHARGE ||
           metrics.chargeCurrentMa > kChargeCurrentIntoBatteryThresholdMa;
}

uint16_t readTerminationCurrentMa()
{
    const int reg05 = pmu.readRegister(POWERS_PPM_REG_05H);
    if (reg05 < 0) {
        return 0;
    }
    return 64U + static_cast<uint16_t>(reg05 & 0x0F) * 64U;
}

uint16_t readRechargeThresholdOffsetMv()
{
    const int reg06 = pmu.readRegister(POWERS_PPM_REG_06H);
    if (reg06 < 0) {
        return 0;
    }
    return (reg06 & 0x01) ? 200U : 100U;
}

void setInitError(const String& detail)
{
    gReady = false;
    gUiState = UiState::Error;
    gPendingUiState = UiState::Error;
    gPendingUiStateSinceMs = 0;
    gErrorDetail = detail;
    gBqConfigDetail = detail;
    gTransientDetail = "";
    gDetailExpiresAtMs = 0;
    gScreenDirty = true;
    Serial.print(F("[BAT] "));
    Serial.println(detail);
}

void forceUiState(const UiState state)
{
    gPendingUiState = state;
    gPendingUiStateSinceMs = 0;
    if (gUiState != state) {
        gUiState = state;
        gScreenDirty = true;
    }
}

void requestImmediatePoll()
{
    const unsigned long now = millis();
    gLastPollAtMs = (now > kPollIntervalMs) ? (now - kPollIntervalMs) : 0;
}

void armUsbSourceDetection()
{
    // Re-arm source detection so the charger updates VBUS type/current limits
    // quickly after page entry or a fresh cable insertion.
    pmu.enableAutoDetectionDPDM();
    pmu.enableDetectionDPDM();
}

void drawHeader()
{
    tft.fillRect(0, 0, tft.width(), kHeaderHeight, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.drawString("Battery Charge/Discharge Test", 8, 6, 2);
}

void clearField(const int16_t x, const int16_t y, const int16_t w, const int16_t h)
{
    tft.fillRect(x, y, w, h, TFT_BLACK);
}

void drawValue(const String& value, const int16_t x, const int16_t y, const uint16_t color = TFT_WHITE)
{
    tft.setTextColor(color, TFT_BLACK);
    tft.drawString(value, x, y, 1);
}

void clearValueField(const int16_t x, const int16_t y, const int16_t w)
{
    clearField(x, y, w, kValueFieldH);
}

void updateTextField(const bool force,
                     const String& value,
                     const String& lastValue,
                     const int16_t x,
                     const int16_t y,
                     const int16_t w,
                     const uint16_t color = TFT_WHITE)
{
    if (!force && value == lastValue) {
        return;
    }
    clearValueField(x, y, w);
    drawValue(value, x, y, color);
}

void drawMetricFrame()
{
    tft.fillScreen(TFT_BLACK);
    drawHeader();

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("SOC", kLeftX, kRow0, 1);
    tft.drawString("VBAT", kLeftX, kRow0 + kRowGap, 1);
    tft.drawString("IBAT", kLeftX, kRow0 + kRowGap * 2, 1);
    tft.drawString("R/F/D", kLeftX, kRow0 + kRowGap * 3, 1);
    tft.drawString("SOH/T", kLeftX, kRow0 + kRowGap * 4, 1);

    tft.drawString("VBUS/VSYS", kRightX, kRow0, 1);
    tft.drawString("SY Bus", kRightX, kRow0 + kRowGap, 1);
    tft.drawString("Charge", kRightX, kRow0 + kRowGap * 2, 1);
    tft.drawString("Cfg V/P/F", kRightX, kRow0 + kRowGap * 3, 1);
    tft.drawString("TTE/TTF", kRightX, kRow0 + kRowGap * 4, 1);

    gFrameDrawn = true;
    gFrameShowsError = false;
}

void drawErrorFrame()
{
    tft.fillScreen(TFT_BLACK);
    drawHeader();
    gFrameDrawn = true;
    gFrameShowsError = true;
}

void drawBackButton(const bool selected)
{
    const int16_t x = tft.width() - kBackBtnW - 6;
    const int16_t y = tft.height() - kFooterHeight + 2;
    const uint16_t bg = selected ? TFT_WHITE : 0x2104;
    const uint16_t fg = selected ? TFT_BLACK : TFT_LIGHTGREY;

    tft.fillRoundRect(x, y, kBackBtnW, kBackBtnH, 5, bg);
    tft.drawRoundRect(x, y, kBackBtnW, kBackBtnH, 5, selected ? TFT_YELLOW : TFT_DARKGREY);
    tft.setTextColor(fg, bg);
    tft.drawCentreString("BACK", x + kBackBtnW / 2, y + 3, 1);
}

void drawFooterArea(const bool force)
{
    if (!force && gFocus == gLastDrawnFocus) {
        return;
    }

    const int16_t footerY = tft.height() - kFooterHeight;
    tft.fillRect(0, footerY, tft.width(), kFooterHeight, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    const char* hint = gFocus == FocusItem::Back
        ? "BOOT=back  USER 2s=shutdown"
        : "Turn to BACK  USER 2s=shutdown";
    tft.drawString(hint, 4, footerY + 3, 1);
    drawBackButton(gFocus == FocusItem::Back);
}

void drawStatusArea(const bool force)
{
    if (force || gUiState != gLastDrawnUiState) {
        clearField(kStateX, kStateY, kStateW, kStateH);
        tft.setTextColor(stateColor(gUiState), TFT_BLACK);
        tft.drawString(stateLabel(gUiState), kStateX, kStateY, 4);
        gLastDrawnUiState = gUiState;
    }

    const String detail = currentDetail();
    if (force || detail != gLastDrawnDetail) {
        clearField(kDetailX, kDetailY, kDetailW, kDetailH);
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        tft.drawString(detail, kDetailX, kDetailY, 1);
        gLastDrawnDetail = detail;
    }
}

void drawMetricValues(const bool force)
{
    updateTextField(force,
                    String(gMetrics.soc) + "%",
                    String(gLastDrawnMetrics.soc) + "%",
                    kLeftValueX,
                    kRow0,
                    kLeftValueW);
    updateTextField(force,
                    formatVoltage(gMetrics.bqVoltageMv),
                    formatVoltage(gLastDrawnMetrics.bqVoltageMv),
                    kLeftValueX,
                    kRow0 + kRowGap,
                    kLeftValueW);
    updateTextField(force,
                    formatSignedMilliamp(gMetrics.ibatMa),
                    formatSignedMilliamp(gLastDrawnMetrics.ibatMa),
                    kLeftValueX,
                    kRow0 + kRowGap * 2,
                    kLeftValueW);
    updateTextField(force,
                    formatCapacityTriplet(gMetrics),
                    formatCapacityTriplet(gLastDrawnMetrics),
                    kLeftValueX,
                    kRow0 + kRowGap * 3,
                    kLeftValueW);
    updateTextField(force,
                    String(gMetrics.soh) + "% / " + formatTemp(gMetrics.tempDeciC),
                    String(gLastDrawnMetrics.soh) + "% / " + formatTemp(gLastDrawnMetrics.tempDeciC),
                    kLeftValueX,
                    kRow0 + kRowGap * 4,
                    kLeftValueW);

    updateTextField(force,
                    formatVoltage(gMetrics.vbusMv) + " / " + formatVoltage(gMetrics.vsysMv),
                    formatVoltage(gLastDrawnMetrics.vbusMv) + " / " + formatVoltage(gLastDrawnMetrics.vsysMv),
                    kRightValueX,
                    kRow0,
                    kRightValueW);
    updateTextField(force,
                    String(busStatusLabel(gMetrics.busStatus)),
                    String(busStatusLabel(gLastDrawnMetrics.busStatus)),
                    kRightValueX,
                    kRow0 + kRowGap,
                    kRightValueW);
    updateTextField(force,
                    String(chargeStatusLabel(gMetrics.chargeStatus)) + " " + String(gMetrics.chargeCurrentMa) + "mA",
                    String(chargeStatusLabel(gLastDrawnMetrics.chargeStatus)) + " " + String(gLastDrawnMetrics.chargeCurrentMa) + "mA",
                    kRightValueX,
                    kRow0 + kRowGap * 2,
                    kRightValueW);
    updateTextField(force,
                    String(kChargeTargetVoltageMv) + "/" + String(kPrechargeCurrentMa) + "/" + String(kFastChargeCurrentMa),
                    String(kChargeTargetVoltageMv) + "/" + String(kPrechargeCurrentMa) + "/" + String(kFastChargeCurrentMa),
                    kRightValueX,
                    kRow0 + kRowGap * 3,
                    kRightValueW);
    updateTextField(force,
                    formatTimePair(gMetrics.tteMin, gMetrics.ttfMin),
                    formatTimePair(gLastDrawnMetrics.tteMin, gLastDrawnMetrics.ttfMin),
                    kRightValueX,
                    kRow0 + kRowGap * 4,
                    kRightValueW);
}

void drawErrorBody(const bool force)
{
    if (!force) {
        return;
    }

    clearField(0, kHeaderHeight, tft.width(), tft.height() - kHeaderHeight - kFooterHeight);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawCentreString("INIT FAILED", tft.width() / 2, 86, 4);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawCentreString(gErrorDetail.c_str(), tft.width() / 2, 124, 1);
}

void redrawScreen()
{
    const bool wantErrorFrame = (gUiState == UiState::Error && !gHasMetrics);
    const bool frameChanged = !gFrameDrawn || gFrameShowsError != wantErrorFrame;
    if (frameChanged) {
        if (wantErrorFrame) {
            drawErrorFrame();
        } else {
            drawMetricFrame();
        }
    }

    const bool force = frameChanged || (!wantErrorFrame && !gHasDrawnMetrics);

    tft.startWrite();
    drawStatusArea(force);
    if (wantErrorFrame) {
        drawErrorBody(force);
    } else {
        drawMetricValues(force);
    }
    drawFooterArea(force || gFocus != gLastDrawnFocus);
    tft.endWrite();

    t_embed::board::deselectSharedSpiDevices();

    if (!wantErrorFrame) {
        gLastDrawnMetrics = gMetrics;
        gHasDrawnMetrics = gHasMetrics;
    } else {
        gHasDrawnMetrics = false;
    }
    gLastDrawnFocus = gFocus;
}

void printHelp()
{
    Serial.println();
    Serial.println(F("Battery test commands:"));
    Serial.println(F("  help       - show this help"));
    Serial.println(F("  status     - print current battery / charger status"));
    Serial.println(F("  charge on  - enable SY6970 charging"));
    Serial.println(F("  charge off - disable SY6970 charging"));
    Serial.println(F("  shutdown   - power off via SY6970 (battery-only)"));
    Serial.println();
}

void printStatus()
{
    if (!gHasMetrics) {
        Serial.println(F("[BAT] Metrics not ready yet."));
        return;
    }

    Serial.println();
    Serial.print(F("[BAT] UI State:        "));
    Serial.println(stateLabel(gUiState));
    Serial.print(F("[BAT] Detail:          "));
    Serial.println(currentDetail());
    Serial.print(F("[BAT] BQ Voltage:      "));
    Serial.print(gMetrics.bqVoltageMv);
    Serial.println(F(" mV"));
    Serial.print(F("[BAT] BQ Current:      "));
    Serial.print(gMetrics.ibatMa);
    Serial.println(F(" mA"));
    Serial.print(F("[BAT] SOC / SOH:       "));
    Serial.print(gMetrics.soc);
    Serial.print(F("% / "));
    Serial.print(gMetrics.soh);
    Serial.println(F("%"));
    Serial.print(F("[BAT] Rem/Full/Design: "));
    Serial.print(gMetrics.remainMah);
    Serial.print(F(" / "));
    Serial.print(gMetrics.fullMah);
    Serial.print(F(" / "));
    Serial.print(gMetrics.designMah);
    Serial.println(F(" mAh"));
    Serial.print(F("[BAT] Temp:            "));
    Serial.print(gMetrics.tempDeciC / 10);
    Serial.print('.');
    Serial.print(abs(gMetrics.tempDeciC % 10));
    Serial.println(F(" C"));
    Serial.print(F("[BAT] VBUS / VSYS:     "));
    Serial.print(gMetrics.vbusMv);
    Serial.print(F(" / "));
    Serial.print(gMetrics.vsysMv);
    Serial.println(F(" mV"));
    Serial.print(F("[BAT] SY VBAT:         "));
    Serial.print(gMetrics.syBattVoltageMv);
    Serial.println(F(" mV"));
    Serial.print(F("[BAT] SY Bus:          "));
    Serial.println(busStatusLabel(gMetrics.busStatus));
    Serial.print(F("[BAT] Power Good:      "));
    Serial.println(gMetrics.powerGood ? F("yes") : F("no"));
    Serial.print(F("[BAT] HIZ Mode:        "));
    Serial.println(gMetrics.hizMode ? F("yes") : F("no"));
    Serial.print(F("[BAT] Charge State:    "));
    Serial.println(chargeStatusLabel(gMetrics.chargeStatus));
    Serial.print(F("[BAT] Charge Enabled:  "));
    Serial.println(gMetrics.chargeEnabled ? F("yes") : F("no"));
    Serial.print(F("[BAT] BQ FC Flag:      "));
    Serial.println(gMetrics.bqFullChargeDetected ? F("yes") : F("no"));
    Serial.print(F("[BAT] Charge Current:  "));
    Serial.print(gMetrics.chargeCurrentMa);
    Serial.println(F(" mA"));
    Serial.print(F("[BAT] Term Current:    "));
    Serial.print(kTerminationCurrentMa);
    Serial.println(F(" mA (pack spec)"));
    Serial.print(F("[BAT] TTE / TTF:       "));
    Serial.print(gMetrics.tteMin);
    Serial.print(F(" / "));
    Serial.print(gMetrics.ttfMin);
    Serial.println(F(" min"));
    Serial.print(F("[BAT] BQ Req V/I:      "));
    Serial.print(gauge.getRequestChargingVoltage());
    Serial.print(F(" / "));
    Serial.print(gauge.getRequestChargingCurrent());
    Serial.println(F(" mV/mA"));
    Serial.print(F("[BAT] Input Limit:     "));
    if (gAppliedInputCurrentMa < 0) {
        Serial.println(F("n/a"));
    } else {
        Serial.print(gAppliedInputCurrentMa);
        Serial.println(F(" mA"));
    }
    Serial.print(F("[BAT] PMU Target I/V:  "));
    Serial.print(pmu.getChargerConstantCurr());
    Serial.print(F(" mA / "));
    Serial.print(pmu.getChargeTargetVoltage());
    Serial.println(F(" mV"));
    Serial.print(F("[BAT] PMU Pre/Term:    "));
    Serial.print(pmu.getPrechargeCurr());
    Serial.print(F(" / "));
    Serial.print(readTerminationCurrentMa());
    Serial.println(F(" mA"));
    Serial.print(F("[BAT] PMU Recharge:    "));
    Serial.print(readRechargeThresholdOffsetMv());
    Serial.println(F(" mV"));
    Serial.print(F("[BAT] PMU Timer:       "));
    Serial.print(pmu.isEnableChargingSafetyTimer() ? F("on") : F("off"));
    Serial.print(F(" / "));
    Serial.println(static_cast<int>(pmu.getFastChargeTimer()));
    Serial.print(F("[BAT] PMU Fault Bits:  0x"));
    Serial.println(gMetrics.faultStatus, HEX);
}

bool initGauge()
{
    if (!gauge.begin(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL)) {
        Serial.println(F("[BAT] BQ27220 init failed."));
        return false;
    }

    Serial.print(F("[BAT] BQ27220 Chip ID: 0x"));
    Serial.println(gauge.getChipID(), HEX);
    Serial.print(F("[BAT] BQ27220 HW Ver: 0x"));
    Serial.println(gauge.getHardwareVersion(), HEX);

    if (!gauge.refresh()) {
        Serial.println(F("[BAT] BQ27220 refresh failed."));
        return false;
    }

    Serial.print(F("[BAT] BQ DesignCapacity: "));
    Serial.println(gauge.getDesignCapacity());
    Serial.print(F("[BAT] BQ FullChargeCapacity: "));
    Serial.println(gauge.getFullChargeCapacity());
    Serial.print(F("[BAT] BQ Req Charge V/I: "));
    Serial.print(gauge.getRequestChargingVoltage());
    Serial.print(F(" mV / "));
    Serial.print(gauge.getRequestChargingCurrent());
    Serial.println(F(" mA"));
    return true;
}

bool configureGaugeCapacity()
{
    const uint16_t designCapacity = gauge.getDesignCapacity();
    const uint16_t fullCapacity = gauge.getFullChargeCapacity();

    if (designCapacity == kBatteryCapacityMah && fullCapacity == kBatteryCapacityMah) {
        gBqConfigWarning = false;
        gBqConfigDetail = "BQ CFG OK";
        Serial.println(F("[BAT] BQ27220 capacity already 1300/1300 mAh."));
        return true;
    }

    Serial.print(F("[BAT] Updating BQ27220 capacity from "));
    Serial.print(designCapacity);
    Serial.print(F("/"));
    Serial.print(fullCapacity);
    Serial.println(F(" to 1300/1300 mAh."));

    if (!gauge.setNewCapacity(kBatteryCapacityMah, kBatteryCapacityMah)) {
        gBqConfigWarning = true;
        gBqConfigDetail = "BQ write failed";
        Serial.println(F("[BAT] BQ27220 capacity update failed."));
        return false;
    }

    delay(50);

    if (!gauge.refresh()) {
        gBqConfigWarning = true;
        gBqConfigDetail = "BQ verify refresh failed";
        Serial.println(F("[BAT] BQ27220 refresh failed after update."));
        return false;
    }

    if (gauge.getDesignCapacity() != kBatteryCapacityMah || gauge.getFullChargeCapacity() != kBatteryCapacityMah) {
        gBqConfigWarning = true;
        gBqConfigDetail = "BQ verify mismatch";
        Serial.print(F("[BAT] BQ27220 verify mismatch: "));
        Serial.print(gauge.getDesignCapacity());
        Serial.print(F("/"));
        Serial.println(gauge.getFullChargeCapacity());
        return false;
    }

    gBqConfigWarning = false;
    gBqConfigDetail = "BQ CFG OK";
    Serial.println(F("[BAT] BQ27220 capacity update verified."));
    return true;
}

bool configureGaugeChargeParameters()
{
    BQ27220DMData chargeConfig[] = {
        makeDmU16Entry(BQ27220DMAddressChargingChargingCurrent, kBqRequestedChargeCurrentMa),
        makeDmU16Entry(BQ27220DMAddressChargingChargingVoltage, kBqRequestedChargeVoltageMv),
        makeDmU16Entry(BQ27220DMAddressChargingTaperCurrent, kBqTaperCurrentMa),
        makeDmU16Entry(BQ27220DMAddressGasGaugingCEDVProfile1ChargeTerminationVoltage,
                       kBqChargeTerminationVoltageMv),
        makeDmU16Entry(BQ27220DMAddressConfigurationCurrentThresholdsChargeDetectThreshold,
                       kBqChargeDetectThresholdMa),
        makeDmU16Entry(BQ27220DMAddressConfigurationCurrentThresholdsQuitCurrent, kBqQuitCurrentMa),
        makeDmEndEntry(),
    };

    bool success = false;
    bool reseal = false;

    do {
        BQ27220OperationStatus operationStatus = {};
        gaugeDm.getOperationStatus(&operationStatus);
        reseal = operationStatus.reg.SEC == Bq27220OperationStatusSecSealed;

        if (!gaugeDm.unsealAccess()) {
            gBqConfigWarning = true;
            gBqConfigDetail = "BQ unseal failed";
            Serial.println(F("[BAT] Failed to unseal BQ27220."));
            break;
        }

        if (!gaugeDm.fullAccess()) {
            gBqConfigWarning = true;
            gBqConfigDetail = "BQ full access failed";
            Serial.println(F("[BAT] Failed to enter BQ27220 full access mode."));
            break;
        }

        if (gaugeDm.dateMemoryCheck(chargeConfig, false)) {
            Serial.println(F("[BAT] BQ27220 charge parameters already match target pack."));
            success = true;
            break;
        }

        Serial.print(F("[BAT] Updating BQ charge cfg I/V/Taper/TermV/Detect/Quit: "));
        printBqChargeConfigTargets();

        if (!gaugeDm.dateMemoryCheck(chargeConfig, true)) {
            gBqConfigWarning = true;
            gBqConfigDetail = "BQ write cfg failed";
            Serial.println(F("[BAT] Failed to update BQ27220 charge parameters."));
            break;
        }

        if (!gaugeDm.dateMemoryCheck(chargeConfig, false)) {
            gBqConfigWarning = true;
            gBqConfigDetail = "BQ cfg verify mismatch";
            Serial.println(F("[BAT] BQ27220 charge parameter verify mismatch."));
            break;
        }

        delay(100);
        if (!gauge.refresh()) {
            gBqConfigWarning = true;
            gBqConfigDetail = "BQ refresh after cfg failed";
            Serial.println(F("[BAT] BQ27220 refresh failed after charge parameter update."));
            break;
        }

        success = true;
    } while (false);

    if (reseal && !gaugeDm.sealAccess()) {
        gBqConfigWarning = true;
        gBqConfigDetail = "BQ reseal failed";
        Serial.println(F("[BAT] Failed to reseal BQ27220."));
        return false;
    }

    if (!success) {
        return false;
    }

    gBqConfigWarning = false;
    gBqConfigDetail = "BQ CFG OK";
    Serial.print(F("[BAT] BQ charge cfg verified I/V/Taper/TermV/Detect/Quit: "));
    printBqChargeConfigTargets();
    return true;
}

bool configureTerminationAndRecharge()
{
    if (kTerminationCurrentMa < 64 || kTerminationCurrentMa > 1024 || (kTerminationCurrentMa % 64) != 0) {
        Serial.println(F("[BAT] Invalid SY6970 termination current setting."));
        return false;
    }

    int reg05 = pmu.readRegister(POWERS_PPM_REG_05H);
    if (reg05 < 0) {
        Serial.println(F("[BAT] Failed to read SY6970 REG05."));
        return false;
    }
    reg05 &= 0xF0;
    reg05 |= ((kTerminationCurrentMa - 64) / 64) & 0x0F;
    if (pmu.writeRegister(POWERS_PPM_REG_05H, static_cast<uint8_t>(reg05)) != 0) {
        Serial.println(F("[BAT] Failed to write SY6970 termination current."));
        return false;
    }

    int reg06 = pmu.readRegister(POWERS_PPM_REG_06H);
    if (reg06 < 0) {
        Serial.println(F("[BAT] Failed to read SY6970 REG06."));
        return false;
    }
    if (kRechargeThresholdOffsetMv >= 200) {
        reg06 |= 0x01;
    } else {
        reg06 &= ~0x01;
    }
    if (pmu.writeRegister(POWERS_PPM_REG_06H, static_cast<uint8_t>(reg06)) != 0) {
        Serial.println(F("[BAT] Failed to write SY6970 recharge threshold."));
        return false;
    }

    return true;
}

bool configurePmu()
{
    if (!pmu.init(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL, BOARD_I2C_SY6970)) {
        Serial.println(F("[BAT] SY6970 init failed."));
        return false;
    }

    pmu.exitHizMode();
    if (pmu.isHizMode()) {
        Serial.println(F("[BAT] SY6970 failed to exit HIZ mode."));
        return false;
    }

    // Start from a conservative 500mA input limit until DPDM finishes.
    if (!pmu.setInputCurrentLimit(kInputCurrentSdpMa)) {
        Serial.println(F("[BAT] Failed to set initial input current limit."));
        return false;
    }
    gAppliedInputCurrentMa = kInputCurrentSdpMa;

    if (!pmu.setSysPowerDownVoltage(kSysPowerDownVoltageMv)) {
        Serial.println(F("[BAT] Failed to set SYS power-down voltage."));
        return false;
    }

    pmu.disableCurrentLimitPin();

    if (!pmu.setChargeTargetVoltage(kChargeTargetVoltageMv)) {
        Serial.println(F("[BAT] Failed to set charge target voltage."));
        return false;
    }

    if (!pmu.setPrechargeCurr(kPrechargeCurrentMa)) {
        Serial.println(F("[BAT] Failed to set precharge current."));
        return false;
    }

    if (!pmu.setChargerConstantCurr(kFastChargeCurrentMa)) {
        Serial.println(F("[BAT] Failed to set fast charge current."));
        return false;
    }

    if (!configureTerminationAndRecharge()) {
        return false;
    }

    pmu.enableChargingTermination();
    if (!pmu.isEnableChargingTermination()) {
        Serial.println(F("[BAT] Failed to enable charging termination."));
        return false;
    }
    pmu.setFastChargeTimer(XPowersPPM::FAST_CHARGE_TIMER_12H);
    if (pmu.getFastChargeTimer() != XPowersPPM::FAST_CHARGE_TIMER_12H) {
        Serial.println(F("[BAT] Failed to set fast-charge safety timer."));
        return false;
    }
    pmu.enableChargingSafetyTimer();
    if (!pmu.isEnableChargingSafetyTimer()) {
        Serial.println(F("[BAT] Failed to enable charging safety timer."));
        return false;
    }

    if (!pmu.enableMeasure()) {
        Serial.println(F("[BAT] Failed to enable SY6970 ADC measurement."));
        return false;
    }

    armUsbSourceDetection();
    pmu.enableCharge();
    pmu.getFaultStatus();

    if (pmu.getChargeTargetVoltage() != kChargeTargetVoltageMv ||
        pmu.getPrechargeCurr() != kPrechargeCurrentMa ||
        pmu.getChargerConstantCurr() != kFastChargeCurrentMa ||
        readTerminationCurrentMa() != kTerminationCurrentMa ||
        readRechargeThresholdOffsetMv() != kRechargeThresholdOffsetMv ||
        pmu.getInputCurrentLimit() != kInputCurrentSdpMa ||
        !pmu.isEnableChargingSafetyTimer()) {
        Serial.println(F("[BAT] SY6970 parameter verify mismatch."));
        return false;
    }

    Serial.print(F("[BAT] SY6970 target/pre/fast/term/rechg: "));
    Serial.print(kChargeTargetVoltageMv);
    Serial.print(F("mV / "));
    Serial.print(kPrechargeCurrentMa);
    Serial.print(F("mA / "));
    Serial.print(kFastChargeCurrentMa);
    Serial.print(F("mA / "));
    Serial.print(kTerminationCurrentMa);
    Serial.print(F("mA / "));
    Serial.print(kRechargeThresholdOffsetMv);
    Serial.println(F("mV"));
    return true;
}

bool metricsChanged(const BatteryMetrics& a, const BatteryMetrics& b)
{
    return a.soc != b.soc ||
           a.bqVoltageMv != b.bqVoltageMv ||
           a.ibatMa != b.ibatMa ||
           a.remainMah != b.remainMah ||
           a.fullMah != b.fullMah ||
           a.designMah != b.designMah ||
           a.soh != b.soh ||
           a.tempDeciC != b.tempDeciC ||
           a.tteMin != b.tteMin ||
           a.ttfMin != b.ttfMin ||
           a.syBattVoltageMv != b.syBattVoltageMv ||
           a.vbusMv != b.vbusMv ||
           a.vsysMv != b.vsysMv ||
           a.chargeCurrentMa != b.chargeCurrentMa ||
           a.busStatus != b.busStatus ||
           a.chargeStatus != b.chargeStatus ||
           a.faultStatus != b.faultStatus ||
           a.chargeEnabled != b.chargeEnabled ||
           a.powerGood != b.powerGood ||
           a.hizMode != b.hizMode ||
           a.watchdogFault != b.watchdogFault ||
           a.boostFault != b.boostFault ||
           a.chargeFault != b.chargeFault ||
           a.batteryFault != b.batteryFault ||
           a.ntcFault != b.ntcFault ||
           a.bqFullChargeDetected != b.bqFullChargeDetected ||
           a.isDischarging != b.isDischarging ||
           a.vbusPresent != b.vbusPresent;
}

UiState evaluateObservedUiState(const BatteryMetrics& metrics)
{
    if (gBqConfigWarning) {
        return UiState::BqConfigWarn;
    }

    const bool externalPower = hasExternalPower(metrics);
    const bool bqChargeDone = metrics.bqFullChargeDetected || metrics.soc >= kChargeDoneSocThreshold;
    const auto chargeStatus = static_cast<XPowersPPM::ChargeStatus>(metrics.chargeStatus);
    const bool activelyCharging = hasChargingEvidence(metrics);
    const bool chargeDoneLikely = metrics.chargeEnabled &&
                                  bqChargeDone &&
                                  !metrics.isDischarging;
    const bool keepCharging = externalPower &&
                              metrics.chargeEnabled &&
                              !bqChargeDone &&
                              gLastChargingEvidenceAtMs != 0 &&
                              (millis() - gLastChargingEvidenceAtMs) < kChargingStateHoldMs;

    if (chargeStatus == XPowersPPM::CHARGE_STATE_DONE && bqChargeDone) {
        return UiState::ChargeDone;
    }

    if (externalPower) {
        if (activelyCharging) {
            return UiState::Charging;
        }
        if (keepCharging) {
            return UiState::Charging;
        }
        if (chargeDoneLikely) {
            return UiState::ChargeDone;
        }
        return UiState::Idle;
    }

    if (metrics.ibatMa <= kDischargeCurrentThresholdMa || metrics.isDischarging) {
        return UiState::Discharging;
    }
    return UiState::Idle;
}

void updateUiState(const UiState observedState)
{
    const unsigned long now = millis();

    if (gUiState == UiState::Init || observedState == UiState::BqConfigWarn || observedState == UiState::Error) {
        gPendingUiState = observedState;
        gPendingUiStateSinceMs = 0;
        if (gUiState != observedState) {
            gUiState = observedState;
            gScreenDirty = true;
        }
        return;
    }

    if (observedState == gUiState) {
        gPendingUiState = observedState;
        gPendingUiStateSinceMs = 0;
        return;
    }

    if (observedState != gPendingUiState) {
        gPendingUiState = observedState;
        gPendingUiStateSinceMs = now;
        return;
    }

    if (gPendingUiStateSinceMs && (now - gPendingUiStateSinceMs) >= kUiStateDebounceMs) {
        gUiState = observedState;
        gPendingUiStateSinceMs = 0;
        gScreenDirty = true;
    }
}

bool maybeRestartChargeTopOff()
{
    if (!gMetrics.vbusPresent || !gMetrics.chargeEnabled) {
        return false;
    }

    const auto chargeStatus = static_cast<XPowersPPM::ChargeStatus>(gMetrics.chargeStatus);
    if (chargeStatus != XPowersPPM::CHARGE_STATE_DONE &&
        chargeStatus != XPowersPPM::CHARGE_STATE_NO_CHARGE) {
        return false;
    }

    if (gMetrics.bqFullChargeDetected || gMetrics.soc >= kChargeDoneSocThreshold) {
        return false;
    }

    if (gMetrics.chargeCurrentMa > kChargeCurrentIntoBatteryThresholdMa) {
        return false;
    }

    if (gLastChargeTopOffAttemptMs != 0 &&
        (millis() - gLastChargeTopOffAttemptMs) < kChargeTopOffRetryMs) {
        return false;
    }

    if (effectiveBatteryVoltageMv(gMetrics) + kChargeTopOffRestartMarginMv >= kChargeTargetVoltageMv) {
        return false;
    }

    gLastChargeTopOffAttemptMs = millis();
    Serial.print(F("[BAT] Restarting charge top-off at "));
    Serial.print(effectiveBatteryVoltageMv(gMetrics));
    Serial.println(F(" mV because BQ is not full yet."));
    pmu.disableCharge();
    delay(20);
    pmu.enableCharge();
    setTransientDetail("Charge top-off restart");
    return true;
}

void handleVbusTransition(const BatteryMetrics& previous, BatteryMetrics& current)
{
    if (current.vbusPresent == previous.vbusPresent) {
        return;
    }

    gAppliedInputCurrentMa = -1;
    gLastExternalPowerSeenAtMs = 0;
    gLastChargeTopOffAttemptMs = 0;

    if (current.vbusPresent) {
        pmu.exitHizMode();
        current.hizMode = pmu.isHizMode();
        armUsbSourceDetection();
        pmu.enableCharge();
        current.chargeEnabled = pmu.isEnableCharge();
        setTransientDetail("USB connected");
    } else {
        gLatchedInputBusStatus = static_cast<uint8_t>(XPowersPPM::BUS_STATE_NOINPUT);
        gLastChargingEvidenceAtMs = 0;
        setTransientDetail("USB disconnected");
    }

    requestImmediatePoll();
}

bool applyDynamicInputCurrentLimit()
{
    const auto bus = static_cast<XPowersPPM::BusStatus>(gMetrics.busStatus);

    int16_t desired = -1;
    switch (bus) {
        case XPowersPPM::BUS_STATE_USB_SDP:
            desired = kInputCurrentSdpMa;
            break;
        case XPowersPPM::BUS_STATE_USB_CDP:
        case XPowersPPM::BUS_STATE_USB_DCP:
        case XPowersPPM::BUS_STATE_HVDCP:
        case XPowersPPM::BUS_STATE_ADAPTER:
        case XPowersPPM::BUS_STATE_NO_STANDARD_ADAPTER:
            desired = kInputCurrentAdapterMa;
            break;
        case XPowersPPM::BUS_STATE_NOINPUT:
        case XPowersPPM::BUS_STATE_OTG:
            desired = -1;
            break;
    }

    if (desired < 0) {
        if (gAppliedInputCurrentMa != -1) {
            gAppliedInputCurrentMa = -1;
            Serial.println(F("[BAT] No VBUS input current limit applied."));
        }
        return true;
    }

    if (desired == gAppliedInputCurrentMa) {
        return true;
    }

    if (!pmu.setInputCurrentLimit(desired)) {
        Serial.print(F("[BAT] Failed to set input current limit to "));
        Serial.println(desired);
        return false;
    }

    gAppliedInputCurrentMa = desired;
    Serial.print(F("[BAT] Input current limit set to "));
    Serial.print(desired);
    Serial.print(F(" mA for "));
    Serial.println(busStatusLabel(gMetrics.busStatus));
    return true;
}

bool refreshMetrics()
{
    if (!gauge.refresh()) {
        Serial.println(F("[BAT] BQ27220 refresh failed."));
        setTransientDetail("BQ refresh failed");
        return false;
    }

    const bool hadMetrics = gHasMetrics;
    const BatteryMetrics previous = gMetrics;
    BatteryMetrics next = {};
    const unsigned long now = millis();
    const BatteryStatus batteryStatus = gauge.getBatteryStatus();

    next.soc = gauge.getStateOfCharge();
    next.bqVoltageMv = gauge.getVoltage();
    next.ibatMa = gauge.getCurrent();
    next.remainMah = gauge.getRemainingCapacity();
    next.fullMah = gauge.getFullChargeCapacity();
    next.designMah = gauge.getDesignCapacity();
    next.soh = gauge.getStateOfHealth();
    next.tempDeciC = static_cast<int16_t>(lroundf(gauge.getTemperature() * 10.0f));
    next.tteMin = gauge.getTimeToEmpty();
    next.ttfMin = gauge.getTimeToFull();
    next.syBattVoltageMv = pmu.getBattVoltage();
    next.vbusMv = pmu.getVbusVoltage();
    next.vsysMv = pmu.getSystemVoltage();
    next.chargeCurrentMa = pmu.getChargeCurrent();
    const uint8_t rawBusStatus = static_cast<uint8_t>(pmu.getBusStatus());
    next.busStatus = rawBusStatus;
    next.chargeStatus = static_cast<uint8_t>(pmu.chargeStatus());
    next.chargeEnabled = pmu.isEnableCharge();
    next.hizMode = pmu.isHizMode();
    next.faultStatus = pmu.getFaultStatus();
    next.watchdogFault = pmu.isWatchdogFault();
    next.boostFault = pmu.isBoostFault();
    next.chargeFault = pmu.isChargeFault();
    next.batteryFault = pmu.isBatteryFault();
    next.ntcFault = pmu.isNTCFault();
    next.bqFullChargeDetected = batteryStatus.isFullChargeDetected();
    next.isDischarging = batteryStatus.isInDischargeMode();
    const bool rawBusHasPower = isUsableInputBusStatus(rawBusStatus);
    const bool vbusMeasuredPresent = next.vbusMv >= kVbusPresentThresholdMv;
    next.powerGood = pmu.isPowerGood() && vbusMeasuredPresent;
    const bool rawExternalPower = rawBusHasPower || vbusMeasuredPresent;
    const bool pmuChargingEvidence = hasChargingEvidence(next);
    if (rawExternalPower) {
        gLastExternalPowerSeenAtMs = now;
    }
    const bool recentExternalPower = gLastExternalPowerSeenAtMs != 0 &&
                                     (now - gLastExternalPowerSeenAtMs) < kExternalPowerHoldMs;
    next.vbusPresent = rawExternalPower ||
                       recentExternalPower ||
                       pmuChargingEvidence;

    if (rawBusHasPower) {
        gLatchedInputBusStatus = rawBusStatus;
    } else if (!next.vbusPresent) {
        gLatchedInputBusStatus = static_cast<uint8_t>(XPowersPPM::BUS_STATE_NOINPUT);
        gLastExternalPowerSeenAtMs = 0;
    }

    if (!rawBusHasPower &&
        next.vbusPresent &&
        isUsableInputBusStatus(gLatchedInputBusStatus)) {
        next.busStatus = gLatchedInputBusStatus;
    }

    if (next.vbusPresent && next.hizMode) {
        pmu.exitHizMode();
        requestImmediatePoll();
        setTransientDetail("SY6970 HIZ cleared");
        next.hizMode = pmu.isHizMode();
    }

    gMetrics = next;
    gHasMetrics = true;

    if (!hasExternalPower(gMetrics)) {
        gLastChargingEvidenceAtMs = 0;
    } else if (hasChargingEvidence(gMetrics)) {
        gLastChargingEvidenceAtMs = millis();
    }

    const bool vbusTransition = hadMetrics && gMetrics.vbusPresent != previous.vbusPresent;
    if (vbusTransition) {
        handleVbusTransition(previous, gMetrics);
    }

    if (!applyDynamicInputCurrentLimit()) {
        setTransientDetail("SY6970 input-limit update failed");
    }

    (void)maybeRestartChargeTopOff();
    const UiState observedState = evaluateObservedUiState(gMetrics);
    if (vbusTransition) {
        forceUiState(observedState);
    } else {
        updateUiState(observedState);
    }

    if (!hadMetrics || !gHasDrawnMetrics || metricsChanged(gMetrics, gLastDrawnMetrics)) {
        gScreenDirty = true;
    }

    return true;
}

void attemptShutdown(const __FlashStringHelper* source)
{
    Serial.print(F("[BAT] Shutdown requested by "));
    Serial.println(source);

    if (!gPmuOk) {
        Serial.println(F("[BAT] SY6970 unavailable, cannot shutdown."));
        setTransientDetail("SY6970 unavailable");
        return;
    }

    if (hasExternalPower()) {
        Serial.println(F("[BAT] VBUS present, refusing shutdown. Unplug USB first."));
        setTransientDetail("VBUS present; unplug USB first");
        return;
    }

    setTransientDetail("Shutting down...");
    redrawScreen();
    delay(100);
    Serial.println(F("[BAT] SY6970 shutdown now."));
    pmu.shutdown();
}

void handleEncoder()
{
    const int32_t cur = g.encRaw;
    const int32_t delta = (cur - gEncSnapshot) / 2;
    if (delta != 0) {
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
            gScreenDirty = true;
        }
    }

    if (g.encBtn.event) {
        g.encBtn.event = false;
        if (gFocus == FocusItem::Back) {
            requestExitSubPage();
        }
    }
}

void handleUserButton()
{
    if (g.usrBtn.event) {
        g.usrBtn.event = false;
    }

    if (!g.usrBtn.pressed) {
        gShutdownTracking = false;
        gShutdownHandled = false;
        return;
    }

    const unsigned long now = millis();
    if (!gShutdownTracking) {
        gShutdownTracking = true;
        gShutdownHandled = false;
        gUsrPressedAtMs = now;
        return;
    }

    if (!gShutdownHandled && (now - gUsrPressedAtMs) >= kShutdownHoldMs) {
        gShutdownHandled = true;
        attemptShutdown(F("USER KEY"));
    }
}

void handleCommand(const String& line)
{
    String cmd = line;
    cmd.trim();
    cmd.toLowerCase();

    if (cmd.isEmpty()) {
        return;
    }

    if (cmd == "help") {
        printHelp();
        return;
    }

    if (cmd == "status") {
        printStatus();
        return;
    }

    if (!gReady) {
        Serial.println(F("[BAT] Battery page not ready."));
        return;
    }

    if (cmd == "charge on") {
        pmu.enableCharge();
        Serial.println(F("[BAT] Charging enabled."));
        setTransientDetail("Charging enabled");
        refreshMetrics();
        return;
    }

    if (cmd == "charge off") {
        pmu.disableCharge();
        Serial.println(F("[BAT] Charging disabled."));
        setTransientDetail("Charging disabled");
        refreshMetrics();
        return;
    }

    if (cmd == "shutdown") {
        attemptShutdown(F("serial"));
        return;
    }

    Serial.print(F("[BAT] Unknown command: "));
    Serial.println(line);
    printHelp();
}

void pollSerial()
{
    while (Serial.available() > 0) {
        const char ch = static_cast<char>(Serial.read());
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            handleCommand(gSerialLine);
            gSerialLine = "";
            continue;
        }
        gSerialLine += ch;
    }
}

}  // namespace

void init()
{
    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);

    gUiState = UiState::Init;
    gPendingUiState = UiState::Init;
    gMetrics = BatteryMetrics{};
    gLastDrawnMetrics = BatteryMetrics{};
    gHasMetrics = false;
    gHasDrawnMetrics = false;
    gGaugeOk = false;
    gPmuOk = false;
    gReady = false;
    gScreenDirty = true;
    gBqConfigWarning = false;
    gFrameDrawn = false;
    gFrameShowsError = false;
    gShutdownTracking = false;
    gShutdownHandled = false;
    gBqConfigDetail = "Initializing BQ27220 / SY6970";
    gTransientDetail = "";
    gErrorDetail = "";
    gLastDrawnDetail = "";
    gLastDrawnUiState = UiState::Init;
    gFocus = FocusItem::Metrics;
    gLastDrawnFocus = FocusItem::Metrics;
    gSerialLine = "";
    gDetailExpiresAtMs = 0;
    gLastPollAtMs = 0;
    gPendingUiStateSinceMs = 0;
    gLastChargingEvidenceAtMs = 0;
    gLastExternalPowerSeenAtMs = 0;
    gLastChargeTopOffAttemptMs = 0;
    gUsrPressedAtMs = 0;
    gAppliedInputCurrentMa = -1;
    gLatchedInputBusStatus = static_cast<uint8_t>(XPowersPPM::BUS_STATE_NOINPUT);
    gEncSnapshot = g.encRaw;

    redrawScreen();
    gScreenDirty = false;

    gGaugeOk = initGauge();
    if (!gGaugeOk) {
        setInitError("BQ27220 init failed");
        return;
    }

    (void)configureGaugeCapacity();
    (void)configureGaugeChargeParameters();

    gPmuOk = configurePmu();
    if (!gPmuOk) {
        setInitError("SY6970 config failed");
        return;
    }

    gReady = true;
    if (!refreshMetrics()) {
        setInitError("Initial battery refresh failed");
        return;
    }

    printHelp();
    printStatus();
}

void update()
{
    pollSerial();
    handleEncoder();
    handleUserButton();

    if (gDetailExpiresAtMs && millis() >= gDetailExpiresAtMs) {
        gDetailExpiresAtMs = 0;
        gTransientDetail = "";
        gScreenDirty = true;
    }

    if (currentDetail() != gLastDrawnDetail) {
        gScreenDirty = true;
    }

    if (!gReady) {
        return;
    }

    const unsigned long now = millis();
    if (now - gLastPollAtMs >= kPollIntervalMs) {
        gLastPollAtMs = now;
        (void)refreshMetrics();
    }
}

void render()
{
    if (!gScreenDirty) {
        return;
    }
    gScreenDirty = false;
    redrawScreen();
}

void deinit()
{
    g.encBtn.event = false;
    g.usrBtn.event = false;
    gShutdownTracking = false;
    gShutdownHandled = false;
    gSerialLine = "";
}

}  // namespace page_battery
