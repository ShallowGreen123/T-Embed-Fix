#pragma once
#include <WiFi.h>

namespace page_wifi {

namespace {
    enum class WState : uint8_t { Idle, Scanning, Connecting, Connected, Failed };

    WState   gState      = WState::Idle;
    bool     gDirty      = true;
    uint32_t gStateMs    = 0;
    int      gScanCount  = 0;

    // Top AP list (up to 5)
    constexpr uint8_t kMaxAp = 5;
    struct ApInfo { String ssid; int32_t rssi; };
    ApInfo gAps[kMaxAp];
    uint8_t gApCount = 0;

    String  gIp;
    String  gFailReason;

    void setState(WState s) {
        gState = s; gStateMs = millis(); gDirty = true;
    }

    void startScan() {
        gApCount = 0;
        WiFi.scanNetworks(/*async=*/true);
        setState(WState::Scanning);
    }

    void updateScan() {
        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_RUNNING) return;
        if (n < 0) { setState(WState::Failed); gFailReason = "Scan error"; return; }

        gScanCount = n;
        gApCount = (n > kMaxAp) ? kMaxAp : (uint8_t)n;
        for (uint8_t i = 0; i < gApCount; ++i) {
            gAps[i].ssid = WiFi.SSID(i);
            gAps[i].rssi = WiFi.RSSI(i);
        }
        WiFi.scanDelete();

        // Try to connect if credentials provided
        if (strlen(kWifiSsid) > 0) {
            WiFi.begin(kWifiSsid, kWifiPassword);
            setState(WState::Connecting);
        } else {
            // No creds — just show scan results
            setState(WState::Connected);  // show scan as "done"
        }
    }

    void updateConnect() {
        const wl_status_t s = WiFi.status();
        if (s == WL_CONNECTED) {
            gIp = WiFi.localIP().toString();
            setState(WState::Connected);
        } else if (s == WL_CONNECT_FAILED || s == WL_NO_SSID_AVAIL) {
            gFailReason = "Auth/SSID fail";
            setState(WState::Failed);
        } else if (millis() - gStateMs > 12000) {
            gFailReason = "Timeout";
            setState(WState::Failed);
        }
    }
}  // namespace

void init() {
    gState = WState::Idle;
    gApCount = 0;
    gIp = ""; gFailReason = "";

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(100);

    tft.fillRect(0, 0, tft.width(), 22, 0x000F);
    tft.setTextColor(TFT_WHITE, 0x000F);
    tft.drawCentreString("WiFi", tft.width() / 2, 4, 2);
    tft.fillRect(0, 22, tft.width(), tft.height() - 22, TFT_BLACK);

    startScan();
    gDirty = true;
}

void update() {
    switch (gState) {
        case WState::Scanning:   updateScan();    break;
        case WState::Connecting: updateConnect(); break;
        default: break;
    }
}

void render() {
    if (!gDirty) return;
    gDirty = false;

    const int16_t W = tft.width();
    tft.fillRect(0, 22, W, tft.height() - 22, TFT_BLACK);

    // State banner
    uint16_t stateColor;
    const char* stateLabel;
    switch (gState) {
        case WState::Scanning:   stateColor = TFT_YELLOW;   stateLabel = "SCANNING";   break;
        case WState::Connecting: stateColor = TFT_CYAN;     stateLabel = "CONNECTING"; break;
        case WState::Connected:  stateColor = TFT_GREEN;    stateLabel = "CONNECTED";  break;
        case WState::Failed:     stateColor = TFT_RED;      stateLabel = "FAILED";     break;
        default:                 stateColor = TFT_DARKGREY; stateLabel = "IDLE";       break;
    }
    tft.setTextColor(stateColor, TFT_BLACK);
    tft.drawString(stateLabel, 8, 26, 4);

    if (gState == WState::Connected && gIp.length() > 0) {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(gIp, 8, 66, 2);
    }
    if (gState == WState::Failed) {
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString(gFailReason, 8, 66, 1);
    }

    // AP list
    char buf[32];
    snprintf(buf, sizeof(buf), "APs found: %d", gScanCount);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString(buf, 8, 82, 1);

    for (uint8_t i = 0; i < gApCount; ++i) {
        const int16_t y = 94 + i * 12;
        // RSSI bar (100px wide)
        const int16_t barW = (int16_t)((gAps[i].rssi + 100) * 80 / 70);
        const int16_t bw = barW < 0 ? 0 : (barW > 80 ? 80 : barW);
        tft.fillRect(8, y + 2, 80, 8, 0x2104);
        if (bw > 0) tft.fillRect(8, y + 2, bw, 8, TFT_DARKCYAN);
        // SSID (truncated)
        String ssid = gAps[i].ssid;
        if (ssid.length() > 16) ssid = ssid.substring(0, 15) + "~";
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(ssid, 96, y, 1);
        snprintf(buf, sizeof(buf), "%ld", (long)gAps[i].rssi);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawRightString(buf, W - 6, y, 1);
    }

    // Footer
    tft.fillRect(0, tft.height() - 12, W, 12, 0x2104);
    tft.setTextColor(TFT_DARKGREY, 0x2104);
    tft.drawCentreString("USR=back", W / 2, tft.height() - 11, 1);
}

void deinit() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

}  // namespace page_wifi
