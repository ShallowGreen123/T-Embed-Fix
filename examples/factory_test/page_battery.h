#pragma once
#define XPOWERS_CHIP_SY6970
#include <XPowersLib.h>
#include <GaugeBQ27220.hpp>

namespace page_battery {

namespace {
    GaugeBQ27220 gauge;
    XPowersPPM   pmu;
    bool         gGaugeOk  = false;
    bool         gPmuOk    = false;
    bool         gInitOk   = false;
    bool         gDirty    = true;
    uint32_t     gLastMs   = 0;

    // cached readings
    int     gSoc      = 0;
    float   gVolt     = 0;
    int16_t gCurrent  = 0;
    int16_t gRemain   = 0;
    int16_t gFull     = 0;
    float   gTemp     = 0;

    float   gVbus     = 0;
    float   gVbat     = 0;
    float   gSysVolt  = 0;
    uint8_t gBusStatus  = 0;
    uint8_t gChgStatus  = 0;

    void refresh() {
        if (gGaugeOk && gauge.refresh()) {
            gSoc     = gauge.getStateOfCharge();
            gVolt    = gauge.getVoltage() / 1000.0f;
            gCurrent = gauge.getCurrent();
            gRemain  = gauge.getRemainingCapacity();
            gFull    = gauge.getFullChargeCapacity();
            gTemp    = gauge.getTemperature();
        }

        if (gPmuOk) {
            gVbus      = pmu.getVbusVoltage();
            gVbat      = pmu.getBattVoltage();
            gSysVolt   = pmu.getSystemVoltage();
            gBusStatus = (uint8_t)pmu.getBusStatus();
            gChgStatus = (uint8_t)pmu.chargeStatus();
        }

        gDirty = true;
    }

    void drawRow(int16_t y, const char* label, const char* val, uint16_t valColor = TFT_WHITE) {
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString(label, 8, y, 1);
        tft.setTextColor(valColor, TFT_BLACK);
        tft.drawString(val, 100, y, 1);
    }
}  // namespace

void init() {
    gInitOk = false;
    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);

    gGaugeOk = gauge.begin(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL);
    gPmuOk   = pmu.init(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL, BOARD_I2C_SY6970);

    if (!gGaugeOk) Serial.println(F("[BAT] BQ27220 init failed."));
    if (!gPmuOk)   Serial.println(F("[BAT] SY6970 init failed."));
    gInitOk = gGaugeOk || gPmuOk;

    // Header
    tft.fillRect(0, 0, tft.width(), 22, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.drawCentreString("Battery / PMU", tft.width() / 2, 4, 2);
    tft.fillRect(0, 22, tft.width(), tft.height() - 22, TFT_BLACK);

    if (gInitOk) refresh();
    gDirty = true;
    gLastMs = millis();
}

void update() {
    if (!gInitOk) return;
    if (millis() - gLastMs >= 1000) {
        gLastMs = millis();
        refresh();
    }
}

void render() {
    if (!gDirty) return;
    gDirty = false;

    const int16_t W = tft.width();
    tft.fillRect(0, 22, W, tft.height() - 22, TFT_BLACK);

    if (!gInitOk) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawCentreString("INIT FAILED", W / 2, 80, 4);
        return;
    }

    // SOC big display
    char buf[24];
    if (gGaugeOk) {
        snprintf(buf, sizeof(buf), "%d%%", gSoc);
        uint16_t socColor = (gSoc > 40) ? TFT_GREEN : (gSoc > 15 ? TFT_YELLOW : TFT_RED);
        tft.setTextColor(socColor, TFT_BLACK);
        tft.drawCentreString(buf, W / 2, 28, 4);

        // BQ27220 rows  (y start at 70)
        snprintf(buf, sizeof(buf), "%.3fV", gVolt);
        drawRow(70, "Voltage", buf);
        snprintf(buf, sizeof(buf), "%dmA", gCurrent);
        drawRow(82, "Current", buf, gCurrent < 0 ? TFT_YELLOW : TFT_GREEN);
        snprintf(buf, sizeof(buf), "%d/%d mAh", gRemain, gFull);
        drawRow(94, "Capacity", buf);
        snprintf(buf, sizeof(buf), "%.1fC", gTemp);
        drawRow(106, "Temp", buf);
    } else {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawCentreString("BQ27220 unavailable", W / 2, 40, 2);
    }

    if (gPmuOk) {
        // SY6970 rows
        snprintf(buf, sizeof(buf), "%.3fV", gVbus / 1000.0f);
        drawRow(120, "VBUS", buf);
        snprintf(buf, sizeof(buf), "%.3fV", gVbat / 1000.0f);
        drawRow(132, "VBAT", buf);
        snprintf(buf, sizeof(buf), "%.3fV", gSysVolt / 1000.0f);
        drawRow(144, "VSYS", buf);

        // Status labels
        const char* busLabels[] = {"No input","USB SDP","USB CDP","USB DCP","HVDCP","Unknown","Non-std","OTG"};
        const char* chgLabels[] = {"Not chg","Pre-chg","Fast chg","Done","?"};
        uint8_t bi = gBusStatus < 8 ? gBusStatus : 7;
        uint8_t ci = gChgStatus < 4 ? gChgStatus : 4;
        drawRow(156, "Bus", busLabels[bi], TFT_CYAN);
        drawRow(168, "Chg", chgLabels[ci], TFT_CYAN);
    } else {
        drawRow(132, "SY6970", "Unavailable", TFT_DARKGREY);
    }

    // Footer hint
    tft.fillRect(0, tft.height() - 12, W, 12, 0x2104);
    tft.setTextColor(TFT_DARKGREY, 0x2104);
    tft.drawCentreString("USR=back", W / 2, tft.height() - 11, 1);
}

void deinit() {
    // I2C stays active — nothing to tear down
}

}  // namespace page_battery
