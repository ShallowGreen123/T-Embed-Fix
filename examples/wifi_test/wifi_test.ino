#include <Arduino.h>
#include <TFT_eSPI.h>
#include <TEmbedBoard.h>
#include <WiFi.h>

// ---- Configure your two candidate networks here ----
static const char* kSsid1     = "YourSSID_1";
static const char* kPassword1 = "YourPassword1";
static const char* kSsid2     = "YourSSID_2";
static const char* kPassword2 = "YourPassword2";
// ----------------------------------------------------

static constexpr uint8_t  kRotation      = 1;
static constexpr uint32_t kConnTimeoutMs = 15000;

static TEmbedXL9555 ioExpander;
static TFT_eSPI     tft;

struct NetChoice {
    const char* ssid;
    const char* password;
    int32_t     rssi;
};

// ---- layout helpers ----
static int16_t lineY(uint8_t row) {
    return 36 + row * 18;
}

static void drawHeader(const char* title, uint16_t color) {
    tft.fillRect(0, 0, tft.width(), 28, color);
    tft.setTextColor(TFT_BLACK, color);
    tft.drawString(title, 8, 7, 2);
}

static void clearBody() {
    tft.fillRect(0, 28, tft.width(), tft.height() - 28, TFT_BLACK);
}

// ---- board init ----
static bool initBoardForDisplay() {
    t_embed::board::deselectSharedSpiDevices();

    if (!t_embed::board::beginExpander(ioExpander)) {
        Serial.println(F("[WiFi] XL9555 init failed."));
        return false;
    }
    if (!t_embed::board::setLowPowerEnabled(ioExpander, true)) {
        Serial.println(F("[WiFi] 3V3 rail enable failed."));
        return false;
    }

    pinMode(BOARD_LCD_BL, OUTPUT);
    digitalWrite(BOARD_LCD_BL, HIGH);

    t_embed::board::setLcdReset(ioExpander, true);  delay(5);
    t_embed::board::setLcdReset(ioExpander, false); delay(20);
    t_embed::board::setLcdReset(ioExpander, true);  delay(120);
    return true;
}

// ---- scan helpers ----
static int32_t rssiFor(const char* ssid) {
    int n = WiFi.scanComplete();
    for (int i = 0; i < n; ++i) {
        if (WiFi.SSID(i) == ssid) {
            return WiFi.RSSI(i);
        }
    }
    return INT32_MIN;
}

static void drawRssiBars(int16_t x, int16_t y, int32_t rssi, uint16_t color) {
    int bars = 0;
    if (rssi > INT32_MIN) {
        if      (rssi >= -50) bars = 5;
        else if (rssi >= -60) bars = 4;
        else if (rssi >= -70) bars = 3;
        else if (rssi >= -80) bars = 2;
        else                  bars = 1;
    }
    for (int i = 0; i < 5; ++i) {
        int16_t  bx = x + i * 7;
        int16_t  bh = 4 + i * 3;
        int16_t  by = y + 15 - bh;
        uint16_t c  = (i < bars) ? color : TFT_DARKGREY;
        tft.fillRect(bx, by, 5, bh, c);
    }
}

// ---- screen pages ----
static void drawScanPage(const char* msg) {
    tft.fillScreen(TFT_BLACK);
    drawHeader("WiFi Test", TFT_CYAN);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Scanning...", 8, lineY(0), 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(msg, 8, lineY(1), 1);
}


static void drawSuccess(const char* ssid, IPAddress ip, int32_t rssi) {
    tft.fillScreen(TFT_BLACK);
    drawHeader("WiFi  Connected", TFT_GREEN);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("Connected!", 8, lineY(0), 2);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("SSID:", 8, lineY(1), 2);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(String(ssid), 8, lineY(2), 2);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("IP:", 8, lineY(3), 2);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(ip.toString(), 8, lineY(4), 2);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String("Signal: ") + rssi + " dBm", 8, lineY(5), 2);
    drawRssiBars(8, lineY(6), rssi, TFT_GREEN);
}

static void drawTrying(const char* ssid, int32_t rssi, uint8_t attempt, uint32_t elapsed_ms) {
    clearBody();
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(String("Trying #") + attempt + "...", 8, lineY(0), 2);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String("SSID: ") + ssid, 8, lineY(1), 2);

    String rssiStr = (rssi > INT32_MIN) ? (String(rssi) + " dBm") : "N/A";
    tft.drawString(String("RSSI: ") + rssiStr, 8, lineY(2), 2);
    drawRssiBars(8, lineY(3), rssi, TFT_GREEN);

    uint8_t dots = (elapsed_ms / 500) % 4;
    String  dotStr = "";
    for (uint8_t i = 0; i < dots; ++i) dotStr += ".";
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(dotStr + "       ", 8, lineY(4) + 6, 4);
}

static void drawFailed(const char* ssid1, const char* ssid2) {
    tft.fillScreen(TFT_BLACK);
    drawHeader("WiFi  Failed", TFT_RED);

    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Both networks failed!", 8, lineY(0), 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String(ssid1), 8, lineY(1), 2);
    tft.drawString(String(ssid2), 8, lineY(2), 2);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Retrying in 10s...", 8, lineY(3), 1);
}

// ---- try connecting to one specific network ----
static bool tryConnect(const char* ssid, const char* password, int32_t rssi, uint8_t attempt) {
    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(ssid, password);

    clearBody();
    drawHeader("WiFi Test", TFT_CYAN);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        uint32_t elapsed = millis() - start;
        if (elapsed > kConnTimeoutMs) {
            Serial.printf("[WiFi] Timeout: %s\n", ssid);
            return false;
        }
        drawTrying(ssid, rssi, attempt, elapsed);
        delay(200);
    }
    return true;
}

// ---- one full scan+connect cycle ----
static bool runConnect() {
    // 1. Scan
    drawScanPage("Starting scan...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(100);

    WiFi.scanNetworks(/*async=*/true);

    uint32_t scanStart = millis();
    while (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
        String msg = String("Scanning... ") + ((millis() - scanStart) / 1000) + "s";
        drawScanPage(msg.c_str());
        delay(300);
    }

    Serial.printf("[WiFi] Scan done: %d networks\n", WiFi.scanComplete());

    // 2. Sort by signal: try stronger first, weaker second
    int32_t r1 = rssiFor(kSsid1);
    int32_t r2 = rssiFor(kSsid2);
    Serial.printf("[WiFi] %s RSSI=%d  |  %s RSSI=%d\n", kSsid1, r1, kSsid2, r2);

    const char* firstSsid;  const char* firstPass;  int32_t firstRssi;
    const char* secondSsid; const char* secondPass; int32_t secondRssi;

    if (r2 > r1) {
        firstSsid  = kSsid2;  firstPass  = kPassword2;  firstRssi  = r2;
        secondSsid = kSsid1;  secondPass = kPassword1;  secondRssi = r1;
    } else {
        firstSsid  = kSsid1;  firstPass  = kPassword1;  firstRssi  = r1;
        secondSsid = kSsid2;  secondPass = kPassword2;  secondRssi = r2;
    }

    // 3. Try first (stronger)
    Serial.printf("[WiFi] Trying first: %s\n", firstSsid);
    if (tryConnect(firstSsid, firstPass, firstRssi, 1)) {
        int32_t   connRssi = WiFi.RSSI();
        IPAddress ip       = WiFi.localIP();
        Serial.printf("[WiFi] Connected! IP=%s  RSSI=%d\n", ip.toString().c_str(), connRssi);
        drawSuccess(firstSsid, ip, connRssi);
        WiFi.scanDelete();
        return true;
    }

    // 4. Try second (weaker / fallback)
    Serial.printf("[WiFi] First failed, trying: %s\n", secondSsid);
    if (tryConnect(secondSsid, secondPass, secondRssi, 2)) {
        int32_t   connRssi = WiFi.RSSI();
        IPAddress ip       = WiFi.localIP();
        Serial.printf("[WiFi] Connected! IP=%s  RSSI=%d\n", ip.toString().c_str(), connRssi);
        drawSuccess(secondSsid, ip, connRssi);
        WiFi.scanDelete();
        return true;
    }

    // 5. Both failed
    Serial.println(F("[WiFi] Both networks failed."));
    drawFailed(kSsid1, kSsid2);
    WiFi.scanDelete();
    return false;
}

// ================================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println(F("\nT-Embed WiFi test"));

    if (!initBoardForDisplay()) {
        Serial.println(F("[WiFi] Display init failed. Halting."));
        while (true) delay(1000);
    }

    tft.init();
    tft.setRotation(kRotation);
    tft.fillScreen(TFT_BLACK);
}

void loop() {
    bool ok = runConnect();

    if (ok) {
        for (int i = 0; i < 200; ++i) {
            delay(3000);
            int32_t rssi = WiFi.RSSI();
            Serial.printf("[WiFi] RSSI: %d dBm\n", rssi);
            tft.fillRect(8, lineY(6), 50, 20, TFT_BLACK);
            drawRssiBars(8, lineY(6), rssi, TFT_GREEN);
            tft.fillRect(68, lineY(5), 90, 18, TFT_BLACK);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString(String(rssi) + " dBm ", 68, lineY(5), 2);
        }
        WiFi.disconnect(true);
    } else {
        delay(10000);
    }
}
