#pragma once
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <limits.h>

namespace page_wifi {

namespace {

constexpr uint32_t kUiFrameIntervalMs = 50;
constexpr uint32_t kConnTimeoutMs = 15000;
constexpr uint32_t kRssiRefreshMs = 1000;
constexpr uint32_t kAnimTickMs = 250;

constexpr int16_t kHeaderHeight = 24;
constexpr int16_t kFooterHeight = 18;
constexpr int16_t kBackBtnW = 58;
constexpr int16_t kBackBtnH = 14;

constexpr uint8_t kMaxScanNetworks = 24;
constexpr uint8_t kNetworksPerPage = 6;

constexpr char kWifiSsid[]     = "LilyGo-AABB";
constexpr char kWifiPassword[] = "xinyuandianzi";
constexpr char kWifiSsid2[]     = "xinyuandianzi";
constexpr char kWifiPassword2[] = "AA15994823428";

enum class WiFiState : uint8_t {
    Idle = 0,
    Scanning,
    Connecting,
    Connected,
    ScanOnly,
    Failed,
};

enum class FocusItem : uint8_t {
    Summary = 0,
    Back,
    kCount,
};

struct ScanNetwork {
    String ssid;
    int32_t rssi = INT32_MIN;
};

struct CandidateNet {
    const char* ssid = "";
    const char* password = "";
    int32_t rssi = INT32_MIN;
    bool found = false;

    CandidateNet() = default;
    CandidateNet(const char* networkName, const char* networkPassword)
        : ssid(networkName), password(networkPassword) {}
};

TFT_eSprite gCanvas(&tft);
WiFiState gState = WiFiState::Idle;
FocusItem gFocus = FocusItem::Summary;
bool gCanvasReady = false;
bool gScreenDirty = true;
bool gWorkflowQueued = false;
uint32_t gStateStartedMs = 0;
uint32_t gLastUiDrawMs = 0;
uint32_t gLastAnimMs = 0;
uint32_t gLastRssiPollMs = 0;
uint32_t gScanStartedMs = 0;
int32_t gEncSnapshot = 0;
int gRawScanCount = 0;
uint8_t gNetworkCount = 0;
uint8_t gListPage = 0;
uint8_t gCandidateCount = 0;
uint8_t gAttemptIndex = 0;
ScanNetwork gNetworks[kMaxScanNetworks];
CandidateNet gCandidates[2];
String gConnectedSsid;
String gConnectedIp;
int32_t gConnectedRssi = INT32_MIN;
String gFailReason;

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

uint8_t listPageCount()
{
    if (gNetworkCount == 0) {
        return 1;
    }
    return static_cast<uint8_t>(
        (gNetworkCount + kNetworksPerPage - 1U) / kNetworksPerPage);
}

String clipText(String text, const uint8_t maxChars)
{
    if (text.length() <= maxChars) {
        return text;
    }
    return text.substring(0, maxChars - 3U) + "...";
}

const char* signalLabel(const int32_t rssi)
{
    if (rssi >= -50) return "Excellent";
    if (rssi >= -60) return "Strong";
    if (rssi >= -70) return "Good";
    if (rssi >= -80) return "Fair";
    return "Weak";
}

void resetNetworks()
{
    gRawScanCount = 0;
    gNetworkCount = 0;
    gListPage = 0;
    for (auto& network : gNetworks) {
        network = ScanNetwork{};
    }
}

void resetCandidates()
{
    gCandidateCount = 0;
    gAttemptIndex = 0;
    gCandidates[0] = CandidateNet{kWifiSsid, kWifiPassword};
    gCandidates[1] = CandidateNet{kWifiSsid2, kWifiPassword2};
}

void resetRuntime()
{
    gState = WiFiState::Idle;
    gFocus = FocusItem::Summary;
    gWorkflowQueued = false;
    gStateStartedMs = 0;
    gLastUiDrawMs = 0;
    gLastAnimMs = 0;
    gLastRssiPollMs = 0;
    gScanStartedMs = 0;
    gConnectedSsid = "";
    gConnectedIp = "";
    gConnectedRssi = INT32_MIN;
    gFailReason = "";
    resetNetworks();
    resetCandidates();
}

void queueWorkflow()
{
    gWorkflowQueued = true;
    markDirty();
}

void addOrUpdateNetwork(const String& ssid, const int32_t rssi)
{
    String displayName = ssid;
    if (displayName.isEmpty()) {
        displayName = "<hidden>";
    }

    for (uint8_t index = 0; index < gNetworkCount; ++index) {
        if (gNetworks[index].ssid == displayName) {
            if (rssi > gNetworks[index].rssi) {
                gNetworks[index].rssi = rssi;
            }
            return;
        }
    }

    if (gNetworkCount < kMaxScanNetworks) {
        gNetworks[gNetworkCount].ssid = displayName;
        gNetworks[gNetworkCount].rssi = rssi;
        ++gNetworkCount;
        return;
    }

    uint8_t weakestIndex = 0;
    for (uint8_t index = 1; index < gNetworkCount; ++index) {
        if (gNetworks[index].rssi < gNetworks[weakestIndex].rssi) {
            weakestIndex = index;
        }
    }

    if (rssi > gNetworks[weakestIndex].rssi) {
        gNetworks[weakestIndex].ssid = displayName;
        gNetworks[weakestIndex].rssi = rssi;
    }
}

void sortNetworksByRssi()
{
    for (uint8_t index = 1; index < gNetworkCount; ++index) {
        ScanNetwork current = gNetworks[index];
        int insertAt = static_cast<int>(index) - 1;
        while (insertAt >= 0 && gNetworks[insertAt].rssi < current.rssi) {
            gNetworks[insertAt + 1] = gNetworks[insertAt];
            --insertAt;
        }
        gNetworks[insertAt + 1] = current;
    }
}

void collectKnownCandidate(const String& ssid, const int32_t rssi)
{
    for (auto& candidate : gCandidates) {
        if (ssid == candidate.ssid &&
            (!candidate.found || rssi > candidate.rssi)) {
            candidate.found = true;
            candidate.rssi = rssi;
        }
    }
}

void compactAndSortCandidates()
{
    CandidateNet foundCandidates[2];
    uint8_t foundCount = 0;

    for (const auto& candidate : gCandidates) {
        if (candidate.found) {
            foundCandidates[foundCount++] = candidate;
        }
    }

    if (foundCount == 2 && foundCandidates[1].rssi > foundCandidates[0].rssi) {
        const CandidateNet temp = foundCandidates[0];
        foundCandidates[0] = foundCandidates[1];
        foundCandidates[1] = temp;
    }

    gCandidateCount = foundCount;
    for (uint8_t index = 0; index < 2; ++index) {
        gCandidates[index] =
            index < foundCount ? foundCandidates[index] : CandidateNet{};
    }
}

void cacheScanResults(const int resultCount)
{
    resetNetworks();
    resetCandidates();
    gRawScanCount = resultCount;

    for (int index = 0; index < resultCount; ++index) {
        const String ssid = WiFi.SSID(index);
        const int32_t rssi = WiFi.RSSI(index);
        addOrUpdateNetwork(ssid, rssi);
        collectKnownCandidate(ssid, rssi);
    }

    sortNetworksByRssi();
    compactAndSortCandidates();

    Serial.printf("[WiFi] Scan found %d APs, cached %u unique SSIDs.\n",
                  gRawScanCount,
                  static_cast<unsigned>(gNetworkCount));
    for (uint8_t index = 0; index < gNetworkCount; ++index) {
        Serial.printf("[WiFi] %2u. %-24s %ld dBm\n",
                      static_cast<unsigned>(index + 1U),
                      gNetworks[index].ssid.c_str(),
                      static_cast<long>(gNetworks[index].rssi));
    }
}

void startScan()
{
    gConnectedSsid = "";
    gConnectedIp = "";
    gConnectedRssi = INT32_MIN;
    gFailReason = "";
    resetNetworks();
    resetCandidates();

    WiFi.scanDelete();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setSleep(false);
    delay(80);

    gScanStartedMs = millis();
    gLastAnimMs = 0;
    WiFi.scanNetworks(true);
    setState(WiFiState::Scanning);
    Serial.println(F("[WiFi] Scanning nearby networks..."));
}

bool startConnectAttempt(const uint8_t index)
{
    if (index >= gCandidateCount || !gCandidates[index].found) {
        return false;
    }

    gAttemptIndex = index;
    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(gCandidates[index].ssid, gCandidates[index].password);
    setState(WiFiState::Connecting);

    Serial.printf("[WiFi] Trying %s (%ld dBm), attempt %u/%u.\n",
                  gCandidates[index].ssid,
                  static_cast<long>(gCandidates[index].rssi),
                  static_cast<unsigned>(index + 1U),
                  static_cast<unsigned>(gCandidateCount));
    return true;
}

void advanceAttemptOrFail(const String& reason)
{
    const uint8_t next = static_cast<uint8_t>(gAttemptIndex + 1U);
    if (next < gCandidateCount && startConnectAttempt(next)) {
        return;
    }

    gFailReason = reason;
    setState(WiFiState::Failed);
    Serial.print(F("[WiFi] Connection failed: "));
    Serial.println(reason);
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
        gFailReason = "WiFi scan error";
        setState(WiFiState::Failed);
        return;
    }

    cacheScanResults(result);
    WiFi.scanDelete();

    if (gCandidateCount == 0) {
        setState(WiFiState::ScanOnly);
        Serial.println(F("[WiFi] LilyGo-AABB/xinyuandianzi not found; scan list only."));
        return;
    }

    if (!startConnectAttempt(0)) {
        gFailReason = "No valid target network";
        setState(WiFiState::Failed);
    }
}

void updateConnecting()
{
    const wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED) {
        gConnectedSsid = WiFi.SSID();
        gConnectedIp = WiFi.localIP().toString();
        gConnectedRssi = WiFi.RSSI();
        gLastRssiPollMs = millis();
        setState(WiFiState::Connected);
        Serial.printf("[WiFi] Connected: %s, IP=%s, RSSI=%ld dBm.\n",
                      gConnectedSsid.c_str(),
                      gConnectedIp.c_str(),
                      static_cast<long>(gConnectedRssi));
        return;
    }

    if ((status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) &&
        millis() - gStateStartedMs > 1000U) {
        advanceAttemptOrFail("Authentication failed or SSID unavailable");
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
        gFailReason = "Connection lost";
        setState(WiFiState::Failed);
        return;
    }

    if (millis() - gLastRssiPollMs >= kRssiRefreshMs) {
        gLastRssiPollMs = millis();
        const int32_t newRssi = WiFi.RSSI();
        const String newIp = WiFi.localIP().toString();
        if (newRssi != gConnectedRssi || newIp != gConnectedIp) {
            gConnectedRssi = newRssi;
            gConnectedIp = newIp;
            markDirty();
        }
    }
}

template <typename Canvas>
void drawSignalBars(Canvas& gfx,
                    const int16_t x,
                    const int16_t bottom,
                    const int32_t rssi,
                    const uint16_t color)
{
    uint8_t activeBars = 0;
    if (rssi > INT32_MIN) {
        if (rssi >= -50) activeBars = 5;
        else if (rssi >= -60) activeBars = 4;
        else if (rssi >= -70) activeBars = 3;
        else if (rssi >= -80) activeBars = 2;
        else activeBars = 1;
    }

    for (uint8_t index = 0; index < 5; ++index) {
        const int16_t barHeight = static_cast<int16_t>(5 + index * 4);
        const int16_t barX = static_cast<int16_t>(x + index * 10);
        const uint16_t barColor =
            index < activeBars ? color : static_cast<uint16_t>(0x3186);
        gfx.fillRoundRect(barX, bottom - barHeight, 7, barHeight, 2, barColor);
    }
}

uint16_t headerColor()
{
    switch (gState) {
        case WiFiState::Connected: return TFT_DARKGREEN;
        case WiFiState::Failed: return TFT_MAROON;
        case WiFiState::ScanOnly: return TFT_NAVY;
        case WiFiState::Scanning:
        case WiFiState::Connecting:
        case WiFiState::Idle:
        default: return TFT_DARKCYAN;
    }
}

const char* headerTitle()
{
    switch (gState) {
        case WiFiState::Scanning: return "WiFi Scanning";
        case WiFiState::Connecting: return "WiFi Connecting";
        case WiFiState::Connected: return "WiFi Connected";
        case WiFiState::ScanOnly: return "Nearby WiFi";
        case WiFiState::Failed: return "WiFi Status";
        case WiFiState::Idle:
        default: return "WiFi Test";
    }
}

String footerText()
{
    if (gFocus == FocusItem::Back) {
        return "BOOT=back";
    }

    if ((gState == WiFiState::ScanOnly || gState == WiFiState::Failed) &&
        listPageCount() > 1) {
        return "USER=next page  BOOT=rescan";
    }

    if (gState == WiFiState::Scanning) {
        return "Scanning...  USER=retest  turn=BACK";
    }

    return "USER=retest  BOOT=rescan  turn=BACK";
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
                      selected ? TFT_YELLOW : static_cast<uint16_t>(0x52AA));
    gfx.setTextColor(fg, bg);
    gfx.drawCentreString("BACK", x + kBackBtnW / 2, y + 3, 1);
}

template <typename Canvas>
void drawHeader(Canvas& gfx)
{
    const uint16_t color = headerColor();
    gfx.fillRect(0, 0, gfx.width(), kHeaderHeight, color);
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_WHITE, color);
    gfx.drawString(headerTitle(), 8, 5, 2);
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
void drawNetworkList(Canvas& gfx,
                     const int16_t headerY,
                     const uint8_t maxRows)
{
    const uint8_t pages = listPageCount();
    if (gListPage >= pages) {
        gListPage = 0;
    }

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_CYAN, TFT_BLACK);
    String listHeader = "Nearby WiFi: " + String(gNetworkCount);
    if (pages > 1) {
        listHeader += "  page " + String(gListPage + 1U) + "/" + String(pages);
    }
    gfx.drawString(listHeader, 8, headerY, 1);

    if (gNetworkCount == 0) {
        gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
        gfx.drawString("No WiFi networks found", 8, headerY + 18, 2);
        return;
    }

    const uint8_t first =
        static_cast<uint8_t>(gListPage * kNetworksPerPage);
    const uint8_t available =
        static_cast<uint8_t>(gNetworkCount - first);
    const uint8_t rows =
        available < maxRows ? available : maxRows;

    for (uint8_t row = 0; row < rows; ++row) {
        const uint8_t networkIndex = static_cast<uint8_t>(first + row);
        const int16_t y = static_cast<int16_t>(headerY + 16 + row * 16);
        const uint16_t rowBg =
            (row & 1U) ? static_cast<uint16_t>(0x0841) : TFT_BLACK;
        gfx.fillRect(4, y - 2, gfx.width() - 8, 15, rowBg);

        gfx.setTextColor(TFT_DARKGREY, rowBg);
        gfx.drawRightString(String(networkIndex + 1U), 23, y, 1);

        gfx.setTextColor(TFT_WHITE, rowBg);
        gfx.drawString(clipText(gNetworks[networkIndex].ssid, 27), 30, y, 1);

        gfx.setTextDatum(TR_DATUM);
        gfx.setTextColor(
            gNetworks[networkIndex].rssi >= -65 ? TFT_GREEN : TFT_YELLOW,
            rowBg);
        gfx.drawString(String(gNetworks[networkIndex].rssi) + " dBm",
                       gfx.width() - 8, y, 1);
        gfx.setTextDatum(TL_DATUM);
    }
}

template <typename Canvas>
void drawScanningBody(Canvas& gfx)
{
    const uint32_t elapsed = (millis() - gScanStartedMs) / 1000U;
    const uint8_t dots = static_cast<uint8_t>((millis() / 400U) % 4U);
    String text = "Scanning nearby WiFi";
    for (uint8_t index = 0; index < dots; ++index) {
        text += ".";
    }

    gfx.setTextColor(TFT_YELLOW, TFT_BLACK);
    gfx.drawCentreString(text, gfx.width() / 2, 47, 2);
    gfx.setTextColor(TFT_WHITE, TFT_BLACK);
    gfx.drawCentreString("Elapsed: " + String(elapsed) + "s",
                         gfx.width() / 2, 72, 1);
    gfx.setTextColor(TFT_CYAN, TFT_BLACK);
    gfx.drawCentreString("Results will be sorted by signal strength",
                         gfx.width() / 2, 94, 1);
    gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
    gfx.drawCentreString("Targets: LilyGo-AABB / xinyuandianzi",
                         gfx.width() / 2, 118, 1);
}

template <typename Canvas>
void drawConnectingBody(Canvas& gfx)
{
    const CandidateNet& candidate = gCandidates[gAttemptIndex];
    gfx.setTextColor(TFT_YELLOW, TFT_BLACK);
    gfx.drawString(
        "Connecting " + String(gAttemptIndex + 1U) + "/" +
        String(gCandidateCount),
        8, 30, 2);

    gfx.setTextColor(TFT_WHITE, TFT_BLACK);
    gfx.drawString("Target: " + clipText(candidate.ssid, 24), 8, 50, 2);
    gfx.drawString("Signal: " + String(candidate.rssi) + " dBm", 8, 69, 1);
    drawSignalBars(gfx, 260, 76, candidate.rssi, TFT_CYAN);

    drawNetworkList(gfx, 86, 3);
}

template <typename Canvas>
void drawConnectedBody(Canvas& gfx)
{
    constexpr uint16_t cardBg = 0x0861;
    constexpr uint16_t cardBorder = 0x2589;
    gfx.fillRoundRect(8, 31, gfx.width() - 16, 113, 8, cardBg);
    gfx.drawRoundRect(8, 31, gfx.width() - 16, 113, 8, cardBorder);

    gfx.setTextColor(TFT_DARKGREY, cardBg);
    gfx.drawString("TARGET WIFI", 18, 40, 1);
    gfx.setTextColor(TFT_CYAN, cardBg);
    gfx.drawString(clipText(gConnectedSsid, 25), 18, 54, 2);

    gfx.setTextColor(TFT_DARKGREY, cardBg);
    gfx.drawString("IP ADDRESS", 18, 78, 1);
    gfx.setTextColor(TFT_YELLOW, cardBg);
    gfx.drawString(gConnectedIp, 18, 92, 2);

    gfx.setTextColor(TFT_DARKGREY, cardBg);
    gfx.drawString("SIGNAL", 18, 116, 1);
    gfx.setTextColor(TFT_WHITE, cardBg);
    gfx.drawString(String(gConnectedRssi) + " dBm  " +
                       signalLabel(gConnectedRssi),
                   18, 129, 1);
    drawSignalBars(gfx, 246, 137, gConnectedRssi, TFT_GREEN);
}

template <typename Canvas>
void drawListBody(Canvas& gfx)
{
    if (gState == WiFiState::Failed && !gFailReason.isEmpty()) {
        gfx.setTextColor(TFT_ORANGE, TFT_BLACK);
        gfx.drawString("Connect: " + clipText(gFailReason, 36), 8, 29, 1);
        drawNetworkList(gfx, 43, 5);
        return;
    }

    drawNetworkList(gfx, 29, kNetworksPerPage);
}

template <typename Canvas>
void drawBody(Canvas& gfx)
{
    gfx.fillRect(0, kHeaderHeight,
                 gfx.width(),
                 gfx.height() - kHeaderHeight - kFooterHeight,
                 TFT_BLACK);
    gfx.setTextDatum(TL_DATUM);

    switch (gState) {
        case WiFiState::Scanning:
            drawScanningBody(gfx);
            break;
        case WiFiState::Connecting:
            drawConnectingBody(gfx);
            break;
        case WiFiState::Connected:
            drawConnectedBody(gfx);
            break;
        case WiFiState::ScanOnly:
        case WiFiState::Failed:
            drawListBody(gfx);
            break;
        case WiFiState::Idle:
        default:
            gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
            gfx.drawCentreString("Preparing WiFi scan...",
                                 gfx.width() / 2, 72, 2);
            break;
    }
}

template <typename Canvas>
void drawUi(Canvas& gfx)
{
    gfx.fillScreen(TFT_BLACK);
    drawHeader(gfx);
    drawBody(gfx);
    drawFooter(gfx);
}

void redrawScreen()
{
    if (!gCanvasReady) {
        return;
    }

    drawUi(gCanvas);
    t_embed::board::deselectSharedSpiDevices();
    gCanvas.pushSprite(0, 0);
    t_embed::board::deselectSharedSpiDevices();
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
    const FocusItem newFocus =
        delta > 0 ? FocusItem::Back : FocusItem::Summary;
    if (newFocus != gFocus) {
        gFocus = newFocus;
        markDirty();
    }
}

void nextListPage()
{
    const uint8_t pages = listPageCount();
    if (pages <= 1) {
        queueWorkflow();
        return;
    }

    gListPage = static_cast<uint8_t>((gListPage + 1U) % pages);
    markDirty();
}

void handleButtons()
{
    if (g.encBtn.event) {
        g.encBtn.event = false;
        if (gFocus == FocusItem::Back) {
            requestExitSubPage();
            return;
        }
        queueWorkflow();
    }

    if (g.usrBtn.event) {
        g.usrBtn.event = false;
        if (gState == WiFiState::ScanOnly ||
            gState == WiFiState::Failed) {
            nextListPage();
        } else {
            queueWorkflow();
        }
    }
}

}  // namespace

void init()
{
    resetRuntime();
    gEncSnapshot = g.encRaw;

    gCanvas.deleteSprite();
    gCanvas.setColorDepth(16);
    gCanvasReady =
        (gCanvas.createSprite(tft.width(), tft.height()) != nullptr);
    if (!gCanvasReady) {
        gCanvas.setColorDepth(8);
        gCanvasReady =
            (gCanvas.createSprite(tft.width(), tft.height()) != nullptr);
    }
    if (!gCanvasReady) {
        gCanvas.setColorDepth(4);
        gCanvasReady =
            (gCanvas.createSprite(tft.width(), tft.height()) != nullptr);
    }
    if (!gCanvasReady) {
        Serial.println(
            F("[WiFi] Sprite allocation failed; display disabled to prevent flicker."));
    }

    WiFi.scanDelete();
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(80);

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
        case WiFiState::ScanOnly:
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
    if (gLastUiDrawMs != 0 &&
        (now - gLastUiDrawMs) < kUiFrameIntervalMs) {
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
