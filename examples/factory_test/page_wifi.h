#pragma once
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <limits.h>

namespace page_wifi {

namespace {

constexpr uint32_t kUiFrameIntervalMs = 33;
constexpr uint32_t kConnTimeoutMs     = 15000;
constexpr uint32_t kRssiRefreshMs     = 3000;
constexpr uint32_t kAnimTickMs        = 200;

constexpr int16_t  kHeaderHeight = 24;
constexpr int16_t  kFooterHeight = 18;
constexpr int16_t  kBackBtnW     = 58;
constexpr int16_t  kBackBtnH     = 14;

// Optional fallback credentials for the second attempt.
// ---- WiFi credentials (edit before flashing) ----
constexpr char kWifiSsid[]     = "LilyGo-AABB";
constexpr char kWifiPassword[] = "xinyuandianzi";
constexpr char kWifiSsid2[]     = "xinyuandianzi";
constexpr char kWifiPassword2[] = "AA15994823428";

enum class WiFiState : uint8_t {
    Idle = 0,
    Scanning,
    Connecting,
    Connected,
    Failed,
};

enum class FocusItem : uint8_t {
    Summary = 0,
    Back,
    kCount,
};

struct CandidateNet {
    const char* ssid     = "";
    const char* password = "";
    int32_t     rssi     = INT32_MIN;

    CandidateNet() = default;
    CandidateNet(const char* s, const char* p, const int32_t r)
        : ssid(s), password(p), rssi(r) {}
};

TFT_eSprite gCanvas(&tft);
WiFiState   gState           = WiFiState::Idle;
FocusItem   gFocus           = FocusItem::Summary;
bool        gCanvasReady     = false;
bool        gScreenDirty     = true;
bool        gWorkflowQueued  = false;
uint32_t    gStateStartedMs  = 0;
uint32_t    gLastUiDrawMs    = 0;
uint32_t    gLastAnimMs      = 0;
uint32_t    gLastRssiPollMs  = 0;
uint32_t    gScanStartedMs   = 0;
int32_t     gEncSnapshot     = 0;
int         gScanCount       = 0;
uint8_t     gCandidateCount  = 0;
uint8_t     gAttemptIndex    = 0;
CandidateNet gCandidates[2];
String      gConnectedSsid;
String      gConnectedIp;
int32_t     gConnectedRssi   = INT32_MIN;
String      gFailReason;

void markDirty()
{
    gScreenDirty = true;
}

void setState(const WiFiState next)
{
    gState = next;
    gStateStartedMs = millis();
    markDirty();
}

void resetRuntime()
{
    gState          = WiFiState::Idle;
    gFocus          = FocusItem::Summary;
    gWorkflowQueued = false;
    gStateStartedMs = 0;
    gLastUiDrawMs   = 0;
    gLastAnimMs     = 0;
    gLastRssiPollMs = 0;
    gScanStartedMs  = 0;
    gScanCount      = 0;
    gCandidateCount = 0;
    gAttemptIndex   = 0;
    gConnectedSsid  = "";
    gConnectedIp    = "";
    gConnectedRssi  = INT32_MIN;
    gFailReason     = "";
    for (auto& candidate : gCandidates) {
        candidate = CandidateNet{};
    }
}

void initCandidates()
{
    gCandidateCount = 0;

    if (strlen(kWifiSsid) > 0) {
        gCandidates[gCandidateCount++] = CandidateNet{kWifiSsid, kWifiPassword, INT32_MIN};
    }
    if (strlen(kWifiSsid2) > 0 && gCandidateCount < 2) {
        gCandidates[gCandidateCount++] = CandidateNet{kWifiSsid2, kWifiPassword2, INT32_MIN};
    }
}

void queueWorkflow()
{
    gWorkflowQueued = true;
    markDirty();
}

int16_t lineY(const uint8_t row)
{
    return static_cast<int16_t>(32 + row * 16);
}

void drawRssiBars(TFT_eSprite& gfx, const int16_t x, const int16_t y,
                  const int32_t rssi, const uint16_t color)
{
    int bars = 0;
    if (rssi > INT32_MIN) {
        if      (rssi >= -50) bars = 5;
        else if (rssi >= -60) bars = 4;
        else if (rssi >= -70) bars = 3;
        else if (rssi >= -80) bars = 2;
        else                  bars = 1;
    }

    for (int i = 0; i < 5; ++i) {
        const int16_t bx = x + i * 7;
        const int16_t bh = 4 + i * 3;
        const int16_t by = y + 15 - bh;
        const uint16_t c = (i < bars) ? color : TFT_DARKGREY;
        gfx.fillRect(bx, by, 5, bh, c);
    }
}

void drawRssiBars(TFT_eSPI& gfx, const int16_t x, const int16_t y,
                  const int32_t rssi, const uint16_t color)
{
    int bars = 0;
    if (rssi > INT32_MIN) {
        if      (rssi >= -50) bars = 5;
        else if (rssi >= -60) bars = 4;
        else if (rssi >= -70) bars = 3;
        else if (rssi >= -80) bars = 2;
        else                  bars = 1;
    }

    for (int i = 0; i < 5; ++i) {
        const int16_t bx = x + i * 7;
        const int16_t bh = 4 + i * 3;
        const int16_t by = y + 15 - bh;
        const uint16_t c = (i < bars) ? color : TFT_DARKGREY;
        gfx.fillRect(bx, by, 5, bh, c);
    }
}

uint16_t headerColor()
{
    switch (gState) {
        case WiFiState::Connected: return TFT_GREEN;
        case WiFiState::Failed:    return TFT_RED;
        case WiFiState::Scanning:
        case WiFiState::Connecting:
        case WiFiState::Idle:
        default:                   return TFT_CYAN;
    }
}

const char* headerTitle()
{
    switch (gState) {
        case WiFiState::Connected: return "WiFi Connected";
        case WiFiState::Failed:    return "WiFi Failed";
        case WiFiState::Scanning:
        case WiFiState::Connecting:
        case WiFiState::Idle:
        default:                   return "WiFi Test";
    }
}

String footerText()
{
    if (gFocus == FocusItem::Back) {
        return "BOOT returns to menu";
    }

    switch (gState) {
        case WiFiState::Scanning:   return "Scanning  USR=retest  turn=BACK";
        case WiFiState::Connecting: return "Connecting  USR=retest  turn=BACK";
        case WiFiState::Connected:  return "Connected  USR=retest  turn=BACK";
        case WiFiState::Failed:     return "Failed  USR=retest  turn=BACK";
        case WiFiState::Idle:
        default:                    return "USR=retest  turn=BACK";
    }
}

String rssiText(const int32_t rssi)
{
    if (rssi <= INT32_MIN) {
        return "N/A";
    }
    return String(rssi) + " dBm";
}

String configuredNamesText()
{
    if (gCandidateCount == 0) {
        return "No credentials configured";
    }
    if (gCandidateCount == 1) {
        return String(gCandidates[0].ssid);
    }
    return String(gCandidates[0].ssid) + " / " + gCandidates[1].ssid;
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
    const uint16_t color = headerColor();
    gfx.fillRect(0, 0, gfx.width(), kHeaderHeight, color);
    gfx.setTextColor(TFT_BLACK, color);
    gfx.setTextDatum(TL_DATUM);
    gfx.drawString(headerTitle(), 8, 5, 2);
}

template <typename Canvas>
void drawFooter(Canvas& gfx)
{
    const int16_t y = gfx.height() - kFooterHeight;
    gfx.fillRect(0, y, gfx.width(), kFooterHeight, TFT_DARKGREY);
    gfx.setTextColor(TFT_WHITE, TFT_DARKGREY);
    gfx.setTextDatum(TL_DATUM);
    gfx.drawString(footerText(), 6, y + 4, 1);
    drawBackButton(gfx, gFocus == FocusItem::Back);
}

template <typename Canvas>
void drawBody(Canvas& gfx)
{
    gfx.fillRect(0, kHeaderHeight, gfx.width(), gfx.height() - kHeaderHeight - kFooterHeight, TFT_BLACK);
    gfx.setTextDatum(TL_DATUM);

    switch (gState) {
        case WiFiState::Scanning: {
            const uint32_t elapsed = (millis() - gScanStartedMs) / 1000;
            gfx.setTextColor(TFT_YELLOW, TFT_BLACK);
            gfx.drawString("Scanning...", 8, lineY(0), 2);
            gfx.setTextColor(TFT_WHITE, TFT_BLACK);
            gfx.drawString(String("Elapsed: ") + elapsed + "s", 8, lineY(1), 1);
            gfx.drawString("Targets:", 8, lineY(2), 1);
            gfx.setTextColor(TFT_CYAN, TFT_BLACK);
            gfx.drawString(configuredNamesText(), 8, lineY(3), 1);
            gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
            gfx.drawString("Async scan in progress", 8, lineY(5), 1);
            break;
        }

        case WiFiState::Connecting: {
            const CandidateNet& current = gCandidates[gAttemptIndex];
            const uint32_t elapsed = millis() - gStateStartedMs;
            const uint8_t dots = static_cast<uint8_t>((elapsed / 500) % 4);
            String dotStr;
            for (uint8_t i = 0; i < dots; ++i) {
                dotStr += ".";
            }

            gfx.setTextColor(TFT_YELLOW, TFT_BLACK);
            gfx.drawString(String("Trying #") + (gAttemptIndex + 1) + "...", 8, lineY(0), 2);

            gfx.setTextColor(TFT_WHITE, TFT_BLACK);
            gfx.drawString(String("SSID: ") + current.ssid, 8, lineY(1), 2);
            gfx.drawString(String("RSSI: ") + rssiText(current.rssi), 8, lineY(2), 2);
            drawRssiBars(gfx, 8, lineY(3), current.rssi, TFT_GREEN);

            gfx.setTextColor(TFT_CYAN, TFT_BLACK);
            gfx.drawString(dotStr + "       ", 8, lineY(4) + 4, 4);

            gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
            gfx.drawString(String("APs found: ") + gScanCount, 8, lineY(6), 1);
            break;
        }

        case WiFiState::Connected: {
            gfx.setTextColor(TFT_GREEN, TFT_BLACK);
            gfx.drawString("Connected!", 8, lineY(0), 2);

            gfx.setTextColor(TFT_WHITE, TFT_BLACK);
            gfx.drawString("SSID:", 8, lineY(1), 2);
            gfx.setTextColor(TFT_CYAN, TFT_BLACK);
            gfx.drawString(gConnectedSsid, 8, lineY(2), 2);

            gfx.setTextColor(TFT_WHITE, TFT_BLACK);
            gfx.drawString("IP:", 8, lineY(3), 2);
            gfx.setTextColor(TFT_YELLOW, TFT_BLACK);
            gfx.drawString(gConnectedIp, 8, lineY(4), 2);

            gfx.setTextColor(TFT_WHITE, TFT_BLACK);
            gfx.drawString(String("Signal: ") + rssiText(gConnectedRssi), 8, lineY(5), 2);
            drawRssiBars(gfx, 8, lineY(6), gConnectedRssi, TFT_GREEN);
            break;
        }

        case WiFiState::Failed: {
            gfx.setTextColor(TFT_RED, TFT_BLACK);
            gfx.drawString("Connection failed", 8, lineY(0), 2);
            gfx.setTextColor(TFT_ORANGE, TFT_BLACK);
            gfx.drawString(gFailReason, 8, lineY(1), 1);
            gfx.setTextColor(TFT_WHITE, TFT_BLACK);
            gfx.drawString("Targets:", 8, lineY(2), 2);
            gfx.setTextColor(TFT_CYAN, TFT_BLACK);
            gfx.drawString(configuredNamesText(), 8, lineY(3), 1);
            gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
            gfx.drawString(String("APs found: ") + gScanCount, 8, lineY(5), 1);
            break;
        }

        case WiFiState::Idle:
        default:
            gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
            gfx.drawString("Waiting...", 8, lineY(0), 2);
            break;
    }
}

template <typename Canvas>
void drawUi(Canvas& gfx)
{
    gfx.fillRect(0, 0, gfx.width(), gfx.height(), TFT_BLACK);
    drawHeader(gfx);
    drawBody(gfx);
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
}

int32_t rssiFor(const char* ssid)
{
    if (!ssid || !ssid[0]) {
        return INT32_MIN;
    }

    const int n = WiFi.scanComplete();
    for (int i = 0; i < n; ++i) {
        if (WiFi.SSID(i) == ssid) {
            return WiFi.RSSI(i);
        }
    }
    return INT32_MIN;
}

void sortCandidatesByRssi()
{
    if (gCandidateCount < 2) {
        return;
    }

    if (gCandidates[1].rssi > gCandidates[0].rssi) {
        const CandidateNet temp = gCandidates[0];
        gCandidates[0] = gCandidates[1];
        gCandidates[1] = temp;
    }
}

void failWorkflow(const String& reason)
{
    gFailReason = reason;
    setState(WiFiState::Failed);
}

void startScan()
{
    initCandidates();
    gScanCount = 0;
    gAttemptIndex = 0;
    gConnectedSsid = "";
    gConnectedIp = "";
    gConnectedRssi = INT32_MIN;
    gFailReason = "";

    WiFi.scanDelete();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    delay(100);

    gScanStartedMs = millis();
    WiFi.scanNetworks(true);
    setState(WiFiState::Scanning);
}

bool startConnectAttempt(const uint8_t index)
{
    if (index >= gCandidateCount || !gCandidates[index].ssid[0]) {
        return false;
    }

    gAttemptIndex = index;
    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(gCandidates[index].ssid, gCandidates[index].password);
    setState(WiFiState::Connecting);
    return true;
}

void advanceAttemptOrFail(const String& reason)
{
    const uint8_t next = gAttemptIndex + 1;
    if (next < gCandidateCount && gCandidates[next].ssid[0]) {
        startConnectAttempt(next);
        return;
    }
    failWorkflow(reason);
}

void updateScanning()
{
    const int result = WiFi.scanComplete();
    if (result == WIFI_SCAN_RUNNING) {
        if (millis() - gLastAnimMs >= kAnimTickMs) {
            gLastAnimMs = millis();
            markDirty();
        }
        return;
    }

    if (result < 0) {
        failWorkflow("WiFi scan error");
        return;
    }

    gScanCount = result;
    for (uint8_t i = 0; i < gCandidateCount; ++i) {
        gCandidates[i].rssi = rssiFor(gCandidates[i].ssid);
    }
    sortCandidatesByRssi();
    WiFi.scanDelete();

    if (gCandidateCount == 0) {
        failWorkflow("No WiFi credentials configured");
        return;
    }

    if (!startConnectAttempt(0)) {
        failWorkflow("No valid target network");
    }
}

void updateConnecting()
{
    const wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED) {
        gConnectedSsid = gCandidates[gAttemptIndex].ssid;
        gConnectedIp = WiFi.localIP().toString();
        gConnectedRssi = WiFi.RSSI();
        gLastRssiPollMs = millis();
        setState(WiFiState::Connected);
        return;
    }

    if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
        advanceAttemptOrFail("Auth failed or SSID missing");
        return;
    }

    if (millis() - gStateStartedMs > kConnTimeoutMs) {
        advanceAttemptOrFail("Connection timeout");
        return;
    }

    if (millis() - gLastAnimMs >= kAnimTickMs) {
        gLastAnimMs = millis();
        markDirty();
    }
}

void updateConnected()
{
    if (WiFi.status() != WL_CONNECTED) {
        failWorkflow("Link lost");
        return;
    }

    if (millis() - gLastRssiPollMs >= kRssiRefreshMs) {
        gLastRssiPollMs = millis();
        gConnectedRssi = WiFi.RSSI();
        Serial.printf("[WiFi] RSSI: %ld dBm\n", static_cast<long>(gConnectedRssi));
        markDirty();
    }
}

void runQueuedWorkflow()
{
    if (!gWorkflowQueued) {
        return;
    }

    gWorkflowQueued = false;
    startScan();
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
        queueWorkflow();
    }
}

}  // namespace

void init()
{
    resetRuntime();
    gEncSnapshot = g.encRaw;

    gCanvas.deleteSprite();
    gCanvas.setColorDepth(16);
    gCanvasReady = (gCanvas.createSprite(tft.width(), tft.height()) != nullptr);

    WiFi.scanDelete();
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(100);

    queueWorkflow();
}

void update()
{
    handleEncoder();
    handleButtons();
    if (g.subPageExitRequested) {
        return;
    }

    runQueuedWorkflow();

    switch (gState) {
        case WiFiState::Scanning:
            updateScanning();
            break;
        case WiFiState::Connecting:
            updateConnecting();
            break;
        case WiFiState::Connected:
            updateConnected();
            break;
        case WiFiState::Idle:
        case WiFiState::Failed:
        default:
            break;
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
    gWorkflowQueued = false;
    WiFi.scanDelete();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

}  // namespace page_wifi
