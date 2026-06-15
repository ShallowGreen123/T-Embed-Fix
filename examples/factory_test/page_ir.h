#pragma once
#include <Adafruit_NeoPixel.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>

namespace page_ir {

namespace {

struct IrPreset {
    const char*   name;
    decode_type_t protocol;
    uint64_t      value;
    uint16_t      bits;
};

constexpr IrPreset kPreset = {"NEC A", NEC, 0x20DF10EFULL, 32};

constexpr uint16_t kIrCaptureBufSize   = 1024;
constexpr uint8_t  kIrTimeoutMs        = 50;
constexpr uint32_t kLoopbackIntervalMs = 1500;
constexpr uint32_t kLedFlashMs         = 120;
constexpr uint32_t kEchoWindowMs       = 250;
constexpr uint8_t  kLedBrightness      = 60;

constexpr uint16_t kBg      = TFT_BLACK;
constexpr uint16_t kHeader  = 0x04FF;
constexpr uint16_t kPanelTx = 0x18E3;
constexpr uint16_t kPanelRx = 0x12CB;
constexpr uint16_t kLabel   = TFT_DARKGREY;
constexpr uint16_t kLoopBg  = 0x6200;

constexpr int16_t kHeaderH   = 22;
constexpr int16_t kFooterH   = 18;
constexpr int16_t kBackBtnW  = 58;
constexpr int16_t kBackBtnH  = 14;
constexpr int16_t kPanelTxY  = 28;
constexpr int16_t kPanelRxY  = 86;
constexpr int16_t kPanelH    = 52;

enum class FocusItem : uint8_t {
    Controls = 0,
    Back,
    kCount,
};

struct RxInfo {
    bool     valid    = false;
    String   protocol = "--";
    String   valueHex = "--";
    uint16_t bits     = 0;
    uint32_t count    = 0;
    uint32_t lastMs   = 0;
};

struct TxInfo {
    bool     valid    = false;
    String   name     = "--";
    String   protocol = "--";
    String   valueHex = "--";
    uint16_t bits     = 0;
    uint32_t count    = 0;
    uint32_t lastMs   = 0;
};

struct DirtyState {
    bool chrome = true;
    bool header = true;
    bool tx     = true;
    bool rx     = true;
    bool txTime = true;
    bool rxTime = true;
    bool footer = true;
};

IRsend*            gIrSend = nullptr;
IRrecv*            gIrRecv = nullptr;
Adafruit_NeoPixel* gStrip  = nullptr;
decode_results     gIrResult;

RxInfo     gRxInfo;
TxInfo     gTxInfo;
DirtyState gDirty;

bool       gInitOk          = false;
bool       gLoopbackMode    = false;
uint32_t   gLastTxMs        = 0;
uint32_t   gLastLoopbackMs  = 0;
uint32_t   gLastTickMs      = 0;
String     gSerialLine;
FocusItem  gFocus           = FocusItem::Controls;
int32_t    gEncSnapshot     = 0;

struct LedFlash {
    bool     active = false;
    uint32_t endMs  = 0;
};

LedFlash gLedFlash;

void markAllDirty()
{
    gDirty.chrome = true;
    gDirty.header = true;
    gDirty.tx = true;
    gDirty.rx = true;
    gDirty.txTime = true;
    gDirty.rxTime = true;
    gDirty.footer = true;
}

String fmtElapsed(const uint32_t lastMs)
{
    if (lastMs == 0) {
        return "--";
    }
    const uint32_t sec = (millis() - lastMs) / 1000;
    if (sec < 60) {
        return String(sec) + "s ago";
    }
    if (sec < 3600) {
        return String(sec / 60) + "m ago";
    }
    return String(sec / 3600) + "h ago";
}

void setStripColor(const uint8_t r, const uint8_t g, const uint8_t b)
{
    if (!gStrip) {
        return;
    }
    for (int i = 0; i < BOARD_WS2812_NUM_LEDS; ++i) {
        gStrip->setPixelColor(i, gStrip->Color(r, g, b));
    }
    gStrip->show();
}

void initStrip()
{
    if (gStrip) {
        delete gStrip;
        gStrip = nullptr;
    }

    gStrip = new Adafruit_NeoPixel(BOARD_WS2812_NUM_LEDS, BOARD_WS2812_DATA_PIN,
                                   NEO_GRB + NEO_KHZ800);
    gStrip->begin();
    gStrip->setBrightness(kLedBrightness);
    setStripColor(0, 0, 0);
}

void startLedFlash(const uint8_t r, const uint8_t g, const uint8_t b)
{
    if (!gStrip) {
        return;
    }
    gLedFlash.active = true;
    gLedFlash.endMs = millis() + kLedFlashMs;
    setStripColor(r, g, b);
}

void pollLedFlash()
{
    if (!gLedFlash.active) {
        return;
    }
    if (millis() >= gLedFlash.endMs) {
        gLedFlash.active = false;
        setStripColor(0, 0, 0);
    }
}

void drawBackButton(const bool selected)
{
    const int16_t x = tft.width() - kBackBtnW - 6;
    const int16_t y = tft.height() - kFooterH + 2;
    const uint16_t bg = selected ? TFT_WHITE : 0x2104;
    const uint16_t fg = selected ? TFT_BLACK : TFT_LIGHTGREY;

    tft.fillRoundRect(x, y, kBackBtnW, kBackBtnH, 5, bg);
    tft.drawRoundRect(x, y, kBackBtnW, kBackBtnH, 5,
                      selected ? TFT_YELLOW : TFT_DARKGREY);
    tft.setTextColor(fg, bg);
    tft.drawCentreString("BACK", x + kBackBtnW / 2, y + 3, 1);
}

void drawChrome()
{
    const int16_t w = tft.width();
    const uint16_t border = gFocus == FocusItem::Controls ? TFT_YELLOW : TFT_DARKGREY;

    tft.fillRect(0, kHeaderH, w, tft.height() - kHeaderH - kFooterH, kBg);

    tft.fillRoundRect(6, kPanelTxY, w - 12, kPanelH, 6, kPanelTx);
    tft.drawRoundRect(6, kPanelTxY, w - 12, kPanelH, 6, border);
    tft.fillRoundRect(6, kPanelRxY, w - 12, kPanelH, 6, kPanelRx);
    tft.drawRoundRect(6, kPanelRxY, w - 12, kPanelH, 6, border);

    tft.setTextDatum(TL_DATUM);
    tft.setTextPadding(0);

    tft.setTextColor(TFT_ORANGE, kPanelTx);
    tft.drawString("TX", 12, kPanelTxY + 4, 2);
    tft.setTextColor(kLabel, kPanelTx);
    tft.drawString("val", 12, kPanelTxY + 25, 1);

    tft.setTextColor(TFT_GREENYELLOW, kPanelRx);
    tft.drawString("RX", 12, kPanelRxY + 4, 2);
    tft.setTextColor(kLabel, kPanelRx);
    tft.drawString("val", 12, kPanelRxY + 25, 1);
}

void drawHeader()
{
    const int16_t w = tft.width();
    tft.fillRect(0, 0, w, kHeaderH, kHeader);
    tft.setTextColor(TFT_WHITE, kHeader);
    tft.drawCentreString("IR TX / RX Test", w / 2, 4, 2);

    if (gLoopbackMode) {
        tft.fillRoundRect(w - 70, 3, 66, 16, 4, kLoopBg);
        tft.setTextColor(TFT_YELLOW, kLoopBg);
        tft.drawCentreString("LOOP", w - 37, 6, 1);
    }
}

void drawFooter()
{
    const int16_t y = tft.height() - kFooterH;
    tft.fillRect(0, y, tft.width(), kFooterH, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);

    const char* hint;
    if (!gInitOk) {
        hint = gFocus == FocusItem::Back
            ? "BOOT=back  IR init failed"
            : "Turn to BACK  IR init failed";
    } else if (gFocus == FocusItem::Back) {
        hint = "BOOT=back  USR=loop";
    } else {
        hint = "USR=loop  BOOT=send  turn=BACK";
    }

    tft.drawString(hint, 8, y + 3, 1);
    drawBackButton(gFocus == FocusItem::Back);
}

void drawTxValues()
{
    const int16_t w = tft.width();
    const int16_t pX = 6;
    const int16_t pW = w - 12;
    const int16_t pY = kPanelTxY;

    tft.setTextDatum(TL_DATUM);

    tft.setTextColor(TFT_WHITE, kPanelTx);
    tft.setTextPadding(70);
    tft.drawString(gTxInfo.valid ? gTxInfo.name.c_str() : "--", pX + 38, pY + 4, 2);

    tft.setTextColor(TFT_CYAN, kPanelTx);
    tft.setTextPadding(120);
    tft.drawString(gTxInfo.valid ? gTxInfo.protocol.c_str() : "", pX + 110, pY + 4, 2);

    tft.setTextColor(TFT_WHITE, kPanelTx);
    tft.setTextPadding(pW - 40);
    tft.drawString(gTxInfo.valueHex.c_str(), pX + 28, pY + 16, 2);

    char buf[24];
    snprintf(buf, sizeof(buf), "bits:%u", static_cast<unsigned>(gTxInfo.bits));
    tft.setTextColor(kLabel, kPanelTx);
    tft.setTextPadding(60);
    tft.drawString(buf, pX + 6, pY + 40, 1);

    snprintf(buf, sizeof(buf), "x%lu", static_cast<unsigned long>(gTxInfo.count));
    tft.setTextColor(TFT_YELLOW, kPanelTx);
    tft.setTextPadding(60);
    tft.drawString(buf, pX + 68, pY + 40, 1);

    tft.setTextPadding(0);
}

void drawTxTime()
{
    const int16_t w = tft.width();
    const int16_t pX = 6;
    const int16_t pW = w - 12;
    const int16_t pY = kPanelTxY;

    const String ts = fmtElapsed(gTxInfo.lastMs);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(kLabel, kPanelTx);
    tft.setTextPadding(70);
    tft.drawString(ts.c_str(), pX + pW - 4, pY + 34, 1);
    tft.setTextDatum(TL_DATUM);
    tft.setTextPadding(0);
}

void drawRxValues()
{
    const int16_t w = tft.width();
    const int16_t pX = 6;
    const int16_t pW = w - 12;
    const int16_t pY = kPanelRxY;

    tft.setTextDatum(TL_DATUM);

    tft.setTextColor(TFT_WHITE, kPanelRx);
    tft.setTextPadding(160);
    tft.drawString(gRxInfo.valid ? gRxInfo.protocol.c_str() : "waiting...",
                   pX + 38, pY + 4, 2);

    const int16_t bx = pX + pW - 38;
    const int16_t by = pY + 2;
    if (gLoopbackMode && gRxInfo.valid && gTxInfo.valid &&
        gRxInfo.valueHex == gTxInfo.valueHex) {
        tft.fillRoundRect(bx, by, 34, 14, 3, TFT_DARKGREEN);
        tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
        tft.drawCentreString("OK", bx + 17, by + 3, 1);
    } else {
        tft.fillRect(bx, by, 34, 14, kPanelRx);
    }

    tft.setTextColor(TFT_WHITE, kPanelRx);
    tft.setTextPadding(pW - 40);
    tft.drawString(gRxInfo.valid ? gRxInfo.valueHex.c_str() : "--", pX + 28, pY + 16, 2);

    char buf[24];
    snprintf(buf, sizeof(buf), "bits:%u", static_cast<unsigned>(gRxInfo.bits));
    tft.setTextColor(kLabel, kPanelRx);
    tft.setTextPadding(60);
    tft.drawString(buf, pX + 6, pY + 40, 1);

    snprintf(buf, sizeof(buf), "x%lu", static_cast<unsigned long>(gRxInfo.count));
    tft.setTextColor(TFT_YELLOW, kPanelRx);
    tft.setTextPadding(60);
    tft.drawString(buf, pX + 68, pY + 40, 1);

    tft.setTextPadding(0);
}

void drawRxTime()
{
    const int16_t w = tft.width();
    const int16_t pX = 6;
    const int16_t pW = w - 12;
    const int16_t pY = kPanelRxY;

    const String ts = fmtElapsed(gRxInfo.lastMs);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(kLabel, kPanelRx);
    tft.setTextPadding(70);
    tft.drawString(ts.c_str(), pX + pW - 4, pY + 34, 1);
    tft.setTextDatum(TL_DATUM);
    tft.setTextPadding(0);
}

void renderDirtyRegions()
{
    if (gDirty.chrome) {
        drawChrome();
        gDirty.chrome = false;
    }
    if (gDirty.header) {
        drawHeader();
        gDirty.header = false;
    }
    if (gDirty.tx) {
        drawTxValues();
        gDirty.tx = false;
    }
    if (gDirty.rx) {
        drawRxValues();
        gDirty.rx = false;
    }
    if (gDirty.txTime) {
        drawTxTime();
        gDirty.txTime = false;
    }
    if (gDirty.rxTime) {
        drawRxTime();
        gDirty.rxTime = false;
    }
    if (gDirty.footer) {
        drawFooter();
        gDirty.footer = false;
    }
}

bool sendPreset()
{
    if (!gIrSend) {
        return false;
    }

    switch (kPreset.protocol) {
        case NEC:
            gIrSend->sendNEC(kPreset.value, kPreset.bits);
            return true;
        default:
            return false;
    }
}

void doSendCurrentPreset()
{
    if (!gInitOk) {
        return;
    }

    Serial.print(F("[IR] TX -> "));
    Serial.print(kPreset.name);
    Serial.print(F("  proto="));
    Serial.print(typeToString(kPreset.protocol));
    Serial.print(F("  value=0x"));
    Serial.print(uint64ToString(kPreset.value, 16));
    Serial.print(F("  bits="));
    Serial.println(kPreset.bits);

    if (!sendPreset()) {
        Serial.println(F("[IR] Unsupported preset protocol."));
        return;
    }

    const uint32_t now = millis();
    gTxInfo.valid = true;
    gTxInfo.name = kPreset.name;
    gTxInfo.protocol = String(typeToString(kPreset.protocol));
    gTxInfo.valueHex = "0x" + String(uint64ToString(kPreset.value, 16));
    gTxInfo.bits = kPreset.bits;
    gTxInfo.lastMs = now;
    gTxInfo.count++;
    gLastTxMs = now;

    gDirty.tx = true;
    gDirty.txTime = true;

    startLedFlash(0, 0, 80);
}

void pollIrReceive()
{
    if (!gIrRecv || !gIrRecv->decode(&gIrResult)) {
        return;
    }

    const uint32_t now = millis();
    const bool isSelfEcho = (gLastTxMs != 0) && ((now - gLastTxMs) < kEchoWindowMs);

    if (isSelfEcho && !gLoopbackMode) {
        gIrRecv->resume();
        return;
    }

    gRxInfo.valid = (gIrResult.decode_type != UNKNOWN) && (gIrResult.bits > 0);
    gRxInfo.protocol = String(typeToString(gIrResult.decode_type, gIrResult.repeat));
    gRxInfo.valueHex = "0x" + String(uint64ToString(gIrResult.value, 16));
    gRxInfo.bits = gIrResult.bits;
    gRxInfo.lastMs = now;
    gRxInfo.count++;

    gDirty.rx = true;
    gDirty.rxTime = true;

    Serial.print(F("[IR] RX  proto="));
    Serial.print(gRxInfo.protocol);
    Serial.print(F("  value="));
    Serial.print(gRxInfo.valueHex);
    Serial.print(F("  bits="));
    Serial.print(gRxInfo.bits);
    if (isSelfEcho) {
        Serial.print(F("  (loopback echo)"));
    }
    Serial.println();

    if (isSelfEcho) {
        startLedFlash(80, 0, 80);
    } else {
        startLedFlash(0, 80, 0);
    }

    gIrRecv->resume();
}

void pollLoopback()
{
    if (!gLoopbackMode) {
        return;
    }

    const uint32_t now = millis();
    if (now - gLastLoopbackMs < kLoopbackIntervalMs) {
        return;
    }

    gLastLoopbackMs = now;
    doSendCurrentPreset();
}

void toggleLoopback()
{
    gLoopbackMode = !gLoopbackMode;
    if (gLoopbackMode) {
        gLastLoopbackMs = 0;
    }

    gDirty.header = true;
    gDirty.rx = true;
    gDirty.footer = true;

    Serial.print(F("[IR] Loopback mode: "));
    Serial.println(gLoopbackMode ? F("ON") : F("OFF"));
}

void printHelp()
{
    Serial.println();
    Serial.println(F("IR send/receive test commands:"));
    Serial.println(F("  help      - show this help"));
    Serial.println(F("  status    - show current counters"));
    Serial.println(F("  send      - transmit NEC A"));
    Serial.println(F("  loopback  - toggle self-loopback mode"));
    Serial.println();
    Serial.println(F("Hardware controls:"));
    Serial.println(F("  BOOT key  - send NEC A / back when BACK is selected"));
    Serial.println(F("  USR key   - toggle self-loopback mode"));
    Serial.println(F("  encoder   - select BACK"));
    Serial.println();
}

void printStatus()
{
    Serial.println();
    Serial.print(F("[IR] TX pin:     GPIO"));
    Serial.println(BOARD_IR_TX);
    Serial.print(F("[IR] RX pin:     GPIO"));
    Serial.println(BOARD_IR_RX);
    Serial.print(F("[IR] Preset:     "));
    Serial.println(kPreset.name);
    Serial.print(F("[IR] Protocol:   "));
    Serial.println(typeToString(kPreset.protocol));
    Serial.print(F("[IR] TX count:   "));
    Serial.println(gTxInfo.count);
    Serial.print(F("[IR] RX count:   "));
    Serial.println(gRxInfo.count);
    Serial.print(F("[IR] Loopback:   "));
    Serial.println(gLoopbackMode ? F("ON") : F("OFF"));
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

    if (!gInitOk) {
        Serial.println(F("[IR] IR page not ready."));
        return;
    }

    if (cmd == "send") {
        doSendCurrentPreset();
        return;
    }

    if (cmd == "loopback") {
        toggleLoopback();
        return;
    }

    Serial.print(F("[IR] Unknown command: "));
    Serial.println(line);
    printHelp();
}

void pollSerialCommands()
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

void handleEncoderFocus()
{
    const int32_t cur = g.encRaw;
    const int32_t delta = (cur - gEncSnapshot) / 2;
    if (delta == 0) {
        return;
    }

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
        gDirty.chrome = true;
        gDirty.footer = true;
        gDirty.tx = true;
        gDirty.rx = true;
        gDirty.txTime = true;
        gDirty.rxTime = true;
    }
}

void handleButtons()
{
    if (g.usrBtn.event) {
        g.usrBtn.event = false;
        if (gInitOk) {
            toggleLoopback();
        }
    }

    if (!g.encBtn.event) {
        return;
    }

    g.encBtn.event = false;
    if (gFocus == FocusItem::Back) {
        requestExitSubPage();
        return;
    }

    if (gInitOk) {
        doSendCurrentPreset();
    }
}

void updateTickRedraws()
{
    const uint32_t now = millis();
    if (now - gLastTickMs < 1000) {
        return;
    }

    gLastTickMs = now;
    gDirty.txTime = true;
    gDirty.rxTime = true;
}

}  // namespace

void init()
{
    gInitOk = false;
    gLoopbackMode = false;
    gLastTxMs = 0;
    gLastLoopbackMs = 0;
    gLastTickMs = 0;
    gSerialLine = "";
    gFocus = FocusItem::Controls;
    gEncSnapshot = g.encRaw;
    gLedFlash = LedFlash{};

    gRxInfo = RxInfo{};
    gTxInfo = TxInfo{};
    markAllDirty();

    if (gIrSend) {
        delete gIrSend;
        gIrSend = nullptr;
    }
    if (gIrRecv) {
        delete gIrRecv;
        gIrRecv = nullptr;
    }

    gIrSend = new IRsend(BOARD_IR_TX);
    gIrRecv = new IRrecv(BOARD_IR_RX, kIrCaptureBufSize, kIrTimeoutMs, true);
    initStrip();

    if (!gIrSend || !gIrRecv) {
        Serial.println(F("[IR] Failed to create IR objects."));
        return;
    }

    gIrSend->begin();
    gIrRecv->enableIRIn();
    gInitOk = true;

    printStatus();
    printHelp();
}

void update()
{
    pollSerialCommands();
    handleEncoderFocus();
    handleButtons();

    if (gInitOk) {
        pollIrReceive();
        pollLoopback();
    }

    pollLedFlash();
    updateTickRedraws();
}

void render()
{
    renderDirtyRegions();
    t_embed::board::deselectSharedSpiDevices();
}

void deinit()
{
    if (gIrRecv) {
        gIrRecv->disableIRIn();
    }

    if (gStrip) {
        setStripColor(0, 0, 0);
        delete gStrip;
        gStrip = nullptr;
    }

    delete gIrSend;
    gIrSend = nullptr;
    delete gIrRecv;
    gIrRecv = nullptr;

    g.encBtn.event = false;
    g.usrBtn.event = false;
    gInitOk = false;
    gLoopbackMode = false;
}

}  // namespace page_ir
