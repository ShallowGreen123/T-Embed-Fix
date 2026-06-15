#pragma once
#include <Adafruit_NeoPixel.h>
#include <RadioLib.h>

namespace page_cc1101 {

namespace {

struct FreqChoice {
    float       mhz;
    const char* label;
};

constexpr FreqChoice kFreqChoices[] = {
    {315.0f, "315 MHz"},
    {433.92f, "433 MHz"},
    {868.0f, "868 MHz"},
};
constexpr uint8_t kFreqChoiceCount = sizeof(kFreqChoices) / sizeof(kFreqChoices[0]);

constexpr float    kBitRateKbps            = 1.2f;
constexpr float    kRxBandwidthKHz         = 58.0f;
constexpr float    kFrequencyDeviationKHz  = 5.2f;
constexpr int8_t   kOutputPowerDbm         = 10;
constexpr bool     kUseOok                 = true;
constexpr uint8_t  kSyncWordHigh           = 0x01;
constexpr uint8_t  kSyncWordLow            = 0x23;
constexpr uint32_t kBurstIntervalMs        = 1000;
constexpr char     kDefaultTxPrefix[]      = "T-Embed CC1101";

constexpr uint8_t  kMarginLeft   = 8;
constexpr int16_t  kHeaderHeight = 24;
constexpr int16_t  kFooterHeight = 18;
constexpr int16_t  kRowFreqValue = 30;
constexpr int16_t  kRowMode      = 64;
constexpr int16_t  kRowDivider   = 86;
constexpr int16_t  kRowRxLabel   = 92;
constexpr int16_t  kRowRxData    = 106;
constexpr int16_t  kRowRxMeta    = 122;
constexpr int16_t  kRowTxCount   = 138;
constexpr int16_t  kBackBtnW     = 58;
constexpr int16_t  kBackBtnH     = 14;

enum class RadioMode : uint8_t {
    Receive = 0,
    BurstTransmit,
    SniffOok,
};

enum class FocusItem : uint8_t {
    Controls = 0,
    Back,
    kCount,
};

enum class LedEffect : uint8_t {
    None = 0,
    RxFlash,
    TxFlash,
    SniffFlash,
};

constexpr uint8_t kCcRegIocfg2           = 0x00;
constexpr uint8_t kCcRegIocfg0           = 0x02;
constexpr uint8_t kCcRegPktCtrl0         = 0x08;
constexpr uint8_t kCcRegMdmCfg2          = 0x12;
constexpr uint8_t kCcCmdSidle            = 0x36;
constexpr uint8_t kCcCmdSfrx             = 0x3A;
constexpr uint8_t kCcCmdSrx              = 0x34;
constexpr uint8_t kCcGdoSerialDataAsync  = 0x0D;
constexpr uint8_t kCcPktCtrl0AsyncSerial = 0x30;
constexpr float   kSniffRxBwKHz          = 270.0f;
constexpr float   kSniffBitRateKbps      = 50.0f;

constexpr uint8_t  kLedBrightness = 10;
constexpr uint32_t kLedFlashMs    = 200;

struct PulseEvent {
    uint32_t durationUs;
    uint8_t  level;
};

constexpr size_t   kPulseRingSize          = 256;
constexpr uint32_t kBurstSilenceUs         = 5000;
constexpr uint32_t kMinValidPulseUs        = 80;
constexpr uint32_t kMaxValidPulseUs        = 20000;
constexpr uint32_t kSniffScreenIntervalMs  = 250;

Module*  gModule = nullptr;
CC1101*  gRadio = nullptr;
Adafruit_NeoPixel* gStrip = nullptr;

volatile bool gPacketReceived = false;
volatile PulseEvent gPulseRing[kPulseRingSize];
volatile uint16_t gPulseHead = 0;
volatile uint16_t gPulseTail = 0;
volatile uint32_t gLastEdgeUs = 0;
volatile uint32_t gIsrEdgeCount = 0;

RadioMode gCurrentMode = RadioMode::Receive;
FocusItem gFocus = FocusItem::Controls;
FocusItem gLastDrawnFocus = FocusItem::Controls;
uint8_t   gCurrentFreqIndex = 1;
unsigned long gLastBurstAtMs = 0;
uint32_t  gBurstCounter = 0;
uint32_t  gRxCounter = 0;
String    gBurstPrefix = kDefaultTxPrefix;

String    gLastRxPayload;
float     gLastRxRssi = 0.0f;
uint8_t   gLastRxLqi = 0;
bool      gHasLastRx = false;

bool      gScreenDirty = true;
bool      gNeedFullRedraw = true;
bool      gNeedSniffStatsRedraw = false;
bool      gNeedFooterRedraw = true;
bool      gInitOk = false;
bool      gRadioReady = false;
String    gSerialLine;
int32_t   gEncSnapshot = 0;

uint32_t  gLedEffectUntilMs = 0;
LedEffect gLedEffect = LedEffect::None;
bool      gLedDirty = true;

uint32_t gTotalEdges = 0;
uint32_t gLastPulseUs = 0;
uint32_t gBurstCount = 0;
uint16_t gCurrentBurstPulses = 0;
uint32_t gCurrentBurstMinUs = 0;
uint32_t gCurrentBurstMaxUs = 0;
uint16_t gLastBurstPulses = 0;
uint32_t gLastBurstMinUs = 0;
uint32_t gLastBurstMaxUs = 0;
uint32_t gLastBurstShortUs = 0;
uint32_t gLastBurstLongUs = 0;
uint32_t gLastEdgeAtMs = 0;
uint32_t gLastSniffDrawMs = 0;
bool     gInBurst = false;

inline float currentFrequencyMHz()
{
    return kFreqChoices[gCurrentFreqIndex].mhz;
}

inline const char* currentFrequencyLabel()
{
    return kFreqChoices[gCurrentFreqIndex].label;
}

SPIClass& sharedSpi()
{
    // TFT owns the shared HSPI bus on this board, so CC1101 must reuse it.
    return tft.getSPIinstance();
}

const __FlashStringHelper* modeLabel(const RadioMode mode)
{
    switch (mode) {
        case RadioMode::Receive:       return F("RX");
        case RadioMode::BurstTransmit: return F("TX-BURST");
        case RadioMode::SniffOok:      return F("SNIFF-OOK");
    }
    return F("?");
}

const char* modeUiLabel(const RadioMode mode)
{
    switch (mode) {
        case RadioMode::Receive:       return "RX";
        case RadioMode::BurstTransmit: return "TX BURST";
        case RadioMode::SniffOok:      return "SNIFF OOK";
    }
    return "?";
}

uint16_t modeColor(const RadioMode mode)
{
    switch (mode) {
        case RadioMode::Receive:       return TFT_GREEN;
        case RadioMode::BurstTransmit: return TFT_ORANGE;
        case RadioMode::SniffOok:      return TFT_MAGENTA;
    }
    return TFT_WHITE;
}

void IRAM_ATTR onPacketReceived()
{
    gPacketReceived = true;
}

void IRAM_ATTR onSniffEdge()
{
    const uint32_t now = micros();
    const uint32_t dur = now - gLastEdgeUs;
    gLastEdgeUs = now;

    const uint8_t level = static_cast<uint8_t>(digitalRead(BOARD_CC1101_GDO2));

    const uint16_t next = (gPulseHead + 1) % kPulseRingSize;
    if (next != gPulseTail) {
        gPulseRing[gPulseHead].durationUs = dur;
        gPulseRing[gPulseHead].level = level;
        gPulseHead = next;
    }
    gIsrEdgeCount++;
}

void triggerLedEffect(const LedEffect effect)
{
    gLedEffect = effect;
    gLedEffectUntilMs = millis() + kLedFlashMs;
    gLedDirty = true;
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

void updateLeds()
{
    if (!gStrip || !gLedDirty) {
        return;
    }
    gLedDirty = false;

    if (gLedEffect == LedEffect::None || millis() > gLedEffectUntilMs) {
        gLedEffect = LedEffect::None;
        setStripColor(0, 0, 0);
        return;
    }

    switch (gLedEffect) {
        case LedEffect::RxFlash:
            setStripColor(0, kLedBrightness, 0);
            break;
        case LedEffect::TxFlash:
            setStripColor(0, 0, kLedBrightness);
            break;
        case LedEffect::SniffFlash:
            setStripColor(kLedBrightness, 0, kLedBrightness);
            break;
        default:
            setStripColor(0, 0, 0);
            break;
    }
}

void checkLedTimeout()
{
    if (gLedEffect != LedEffect::None && millis() > gLedEffectUntilMs) {
        gLedDirty = true;
    }
}

void drawHeader()
{
    tft.fillRect(0, 0, tft.width(), kHeaderHeight, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.drawString("CC1101 Send / Recv", kMarginLeft, 6, 2);
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

void drawFooter(const bool force)
{
    if (!force && gFocus == gLastDrawnFocus && !gNeedFooterRedraw) {
        return;
    }

    const int16_t y = tft.height() - kFooterHeight;
    tft.fillRect(0, y, tft.width(), kFooterHeight, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);

    const char* msg;
    if (!gInitOk || !gRadioReady) {
        msg = gFocus == FocusItem::Back ? "BOOT=back  Radio init failed" : "Turn to BACK  Radio init failed";
    } else if (gFocus == FocusItem::Back) {
        msg = "BOOT=back  USR=freq";
    } else {
        msg = "USR=freq  BOOT=mode  turn=BACK";
    }
    tft.drawString(msg, kMarginLeft, y + 3, 1);
    drawBackButton(gFocus == FocusItem::Back);

    gLastDrawnFocus = gFocus;
    gNeedFooterRedraw = false;
}

void drawFreqRow()
{
    tft.fillRect(0, kRowFreqValue, tft.width(), 32, TFT_BLACK);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("FREQ:", kMarginLeft, kRowFreqValue + 8, 1);

    uint16_t color = TFT_ORANGE;
    if (gCurrentFreqIndex == 0) {
        color = TFT_GREEN;
    } else if (gCurrentFreqIndex == 1) {
        color = TFT_YELLOW;
    }
    tft.setTextColor(color, TFT_BLACK);
    tft.drawString(currentFrequencyLabel(), kMarginLeft + 50, kRowFreqValue, 4);
}

void drawModeRow()
{
    tft.fillRect(0, kRowMode, tft.width(), 18, TFT_BLACK);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("MODE:", kMarginLeft, kRowMode + 2, 1);
    tft.setTextColor(modeColor(gCurrentMode), TFT_BLACK);
    tft.drawString(modeUiLabel(gCurrentMode), kMarginLeft + 50, kRowMode, 2);
}

void drawDivider()
{
    tft.drawFastHLine(0, kRowDivider, tft.width(), TFT_DARKGREY);
}

void drawRxRow()
{
    tft.fillRect(0, kRowRxLabel, tft.width(), kRowTxCount - kRowRxLabel, TFT_BLACK);

    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("Last RX:", kMarginLeft, kRowRxLabel, 1);

    if (!gHasLastRx) {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("(none)", kMarginLeft + 60, kRowRxLabel, 1);
        return;
    }

    String payload = gLastRxPayload;
    if (payload.length() > 36) {
        payload = payload.substring(0, 33) + "...";
    }
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(payload, kMarginLeft, kRowRxData, 1);

    char meta[40];
    snprintf(meta, sizeof(meta), "RSSI:%.0f dBm  LQI:%u  #%lu",
             gLastRxRssi, static_cast<unsigned>(gLastRxLqi), static_cast<unsigned long>(gRxCounter));
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(meta, kMarginLeft, kRowRxMeta, 1);
}

void drawTxRow()
{
    tft.fillRect(0, kRowTxCount, tft.width(),
                 tft.height() - kFooterHeight - kRowTxCount, TFT_BLACK);
    char buf[32];
    snprintf(buf, sizeof(buf), "TX count: %lu", static_cast<unsigned long>(gBurstCounter));
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(buf, kMarginLeft, kRowTxCount, 1);
}

void drawSniffRows()
{
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.fillRect(0, kRowRxLabel, tft.width(), 14, TFT_BLACK);
    tft.drawString("OOK sniff:", kMarginLeft, kRowRxLabel, 1);

    char l1[48];
    snprintf(l1, sizeof(l1), "Edges: %lu  Bursts: %lu",
             static_cast<unsigned long>(gTotalEdges),
             static_cast<unsigned long>(gBurstCount));
    tft.fillRect(0, kRowRxData, tft.width(), 14, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(l1, kMarginLeft, kRowRxData, 1);

    char l2[64];
    tft.fillRect(0, kRowRxMeta, tft.width(), 14, TFT_BLACK);
    if (gLastBurstPulses > 0) {
        snprintf(l2, sizeof(l2), "Last: %u p  short~%luus  long~%luus",
                 gLastBurstPulses,
                 static_cast<unsigned long>(gLastBurstShortUs),
                 static_cast<unsigned long>(gLastBurstLongUs));
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
    } else {
        snprintf(l2, sizeof(l2), "Press the remote near antenna...");
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    }
    tft.drawString(l2, kMarginLeft, kRowRxMeta, 1);

    tft.fillRect(0, kRowTxCount, tft.width(), 14, TFT_BLACK);
    if (gLastBurstPulses > 0) {
        char l3[48];
        snprintf(l3, sizeof(l3), "min:%luus max:%luus",
                 static_cast<unsigned long>(gLastBurstMinUs),
                 static_cast<unsigned long>(gLastBurstMaxUs));
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        tft.drawString(l3, kMarginLeft, kRowTxCount, 1);
    }
}

void redrawAll()
{
    if (gNeedFullRedraw) {
        gNeedFullRedraw = false;
        tft.fillRect(0, kHeaderHeight, tft.width(),
                     tft.height() - kHeaderHeight - kFooterHeight, TFT_BLACK);
        drawFreqRow();
        drawModeRow();
        drawDivider();
        if (gCurrentMode == RadioMode::SniffOok) {
            drawSniffRows();
        } else {
            drawRxRow();
            drawTxRow();
        }
        drawFooter(true);
        return;
    }

    if (gNeedFooterRedraw || gFocus != gLastDrawnFocus) {
        drawFooter(true);
    }

    if (gCurrentMode == RadioMode::SniffOok && gNeedSniffStatsRedraw) {
        gNeedSniffStatsRedraw = false;
        drawSniffRows();
    } else if (gCurrentMode == RadioMode::Receive) {
        drawRxRow();
    } else if (gCurrentMode == RadioMode::BurstTransmit) {
        drawTxRow();
    }
}

void printHelp()
{
    Serial.println();
    Serial.println(F("CC1101 send/receive test commands:"));
    Serial.println(F("  help              - show this help"));
    Serial.println(F("  status            - show current radio settings"));
    Serial.println(F("  rx                - enter receive mode"));
    Serial.println(F("  tx                - send a test packet every second"));
    Serial.println(F("  sniff             - raw OOK pulse sniffer (use for remotes)"));
    Serial.println(F("  send <text>       - send one packet immediately"));
    Serial.println(F("  freq 315|433|868  - switch frequency and re-init radio"));
    Serial.println(F("  prefix <text>     - change periodic TX message prefix"));
    Serial.println(F("Hardware controls:"));
    Serial.println(F("  USR key   - cycle frequency"));
    Serial.println(F("  BOOT key  - cycle mode RX -> TX -> SNIFF"));
    Serial.println(F("  encoder   - select BACK"));
    Serial.println();
}

void printStatus()
{
    Serial.println();
    Serial.print(F("[CC1101] Mode:        "));
    Serial.println(modeLabel(gCurrentMode));
    Serial.print(F("[CC1101] Frequency:   "));
    Serial.print(currentFrequencyMHz(), 2);
    Serial.println(F(" MHz"));
    Serial.print(F("[CC1101] Modulation:  "));
    Serial.println(kUseOok ? F("OOK") : F("2-FSK"));
    Serial.print(F("[CC1101] Bit rate:    "));
    Serial.print(kBitRateKbps, 1);
    Serial.println(F(" kbps"));
    Serial.print(F("[CC1101] RX BW:       "));
    Serial.print(kRxBandwidthKHz, 1);
    Serial.println(F(" kHz"));
    Serial.print(F("[CC1101] Sync word:   0x"));
    Serial.print(kSyncWordHigh, HEX);
    Serial.print(F(" 0x"));
    Serial.println(kSyncWordLow, HEX);
    Serial.print(F("[CC1101] TX prefix:   "));
    Serial.println(gBurstPrefix);
}

bool applyRadioSettings()
{
    if (!gRadio) {
        return false;
    }

    int state = gRadio->begin(currentFrequencyMHz());
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("[CC1101] radio.begin failed, code "));
        Serial.println(state);
        return false;
    }
    state = gRadio->setFrequency(currentFrequencyMHz());
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("[CC1101] setFrequency failed, code "));
        Serial.println(state);
        return false;
    }
    state = gRadio->setOOK(kUseOok);
    if (state != RADIOLIB_ERR_NONE) return false;
    state = gRadio->setBitRate(kBitRateKbps);
    if (state != RADIOLIB_ERR_NONE) return false;
    state = gRadio->setRxBandwidth(kRxBandwidthKHz);
    if (state != RADIOLIB_ERR_NONE) return false;
    state = gRadio->setFrequencyDeviation(kFrequencyDeviationKHz);
    if (state != RADIOLIB_ERR_NONE) return false;
    state = gRadio->setOutputPower(kOutputPowerDbm);
    if (state != RADIOLIB_ERR_NONE) return false;
    state = gRadio->setSyncWord(kSyncWordHigh, kSyncWordLow);
    if (state != RADIOLIB_ERR_NONE) return false;
    return true;
}

bool initRadio()
{
    if (!gRadio) {
        return false;
    }

    t_embed::board::deselectSharedSpiDevices();

    if (!t_embed::board::setCc1101RfPath(ioExpander, currentFrequencyMHz())) {
        Serial.println(F("[CC1101] Unsupported frequency for board RF switch."));
        return false;
    }

    delay(20);

    Serial.print(F("[CC1101] Initializing at "));
    Serial.print(currentFrequencyMHz(), 2);
    Serial.println(F(" MHz ..."));

    return applyRadioSettings();
}

void leaveSniffMode()
{
    if (!gRadio) {
        return;
    }

    detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO0));
    detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO2));
    (void)gRadio->SPIsendCommand(kCcCmdSidle);
    (void)gRadio->SPIsendCommand(kCcCmdSfrx);
    (void)applyRadioSettings();
}

bool enterReceiveMode()
{
    if (!gRadio) {
        return false;
    }

    const bool wasSniff = (gCurrentMode == RadioMode::SniffOok);
    gCurrentMode = RadioMode::Receive;
    gPacketReceived = false;
    gNeedFullRedraw = true;
    gNeedFooterRedraw = true;
    gScreenDirty = true;

    if (wasSniff) {
        leaveSniffMode();
    }
    detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO0));
    detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO2));
    gRadio->clearPacketSentAction();
    gRadio->clearPacketReceivedAction();
    (void)gRadio->finishTransmit();
    (void)gRadio->standby();

    gRadio->setPacketReceivedAction(onPacketReceived);
    const int state = gRadio->startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("[CC1101] startReceive failed, code "));
        Serial.println(state);
        return false;
    }
    Serial.println(F("[CC1101] Mode switched to RX."));
    return true;
}

void enterBurstTransmitMode()
{
    if (!gRadio) {
        return;
    }

    const bool wasSniff = (gCurrentMode == RadioMode::SniffOok);
    gCurrentMode = RadioMode::BurstTransmit;
    gLastBurstAtMs = 0;
    gNeedFullRedraw = true;
    gNeedFooterRedraw = true;
    gScreenDirty = true;

    if (wasSniff) {
        leaveSniffMode();
    }
    detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO0));
    detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO2));
    gRadio->clearPacketReceivedAction();
    gRadio->clearPacketSentAction();
    (void)gRadio->finishReceive();
    (void)gRadio->standby();

    Serial.println(F("[CC1101] Mode switched to TX burst. Sending once per second."));
}

bool enterSniffMode()
{
    if (!gRadio) {
        return false;
    }

    gCurrentMode = RadioMode::SniffOok;
    gNeedFullRedraw = true;
    gNeedFooterRedraw = true;
    gScreenDirty = true;

    gPulseHead = 0;
    gPulseTail = 0;
    gLastEdgeUs = micros();
    gIsrEdgeCount = 0;
    gTotalEdges = 0;
    gBurstCount = 0;
    gCurrentBurstPulses = 0;
    gCurrentBurstMinUs = 0;
    gCurrentBurstMaxUs = 0;
    gLastBurstPulses = 0;
    gLastBurstMinUs = 0;
    gLastBurstMaxUs = 0;
    gLastBurstShortUs = 0;
    gLastBurstLongUs = 0;
    gInBurst = false;

    detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO0));
    detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO2));
    gRadio->clearPacketReceivedAction();
    gRadio->clearPacketSentAction();
    (void)gRadio->finishReceive();
    (void)gRadio->finishTransmit();
    (void)gRadio->standby();

    int state = gRadio->setOOK(true);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("[CC1101] sniff: setOOK fail "));
        Serial.println(state);
        return false;
    }
    state = gRadio->setRxBandwidth(kSniffRxBwKHz);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("[CC1101] sniff: setRxBandwidth fail "));
        Serial.println(state);
    }
    state = gRadio->setBitRate(kSniffBitRateKbps);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("[CC1101] sniff: setBitRate fail "));
        Serial.println(state);
    }

    gRadio->SPIsetRegValue(kCcRegMdmCfg2, 0x00, 2, 0);
    gRadio->SPIsetRegValue(kCcRegIocfg0, kCcGdoSerialDataAsync);
    gRadio->SPIsetRegValue(kCcRegIocfg2, kCcGdoSerialDataAsync);
    gRadio->SPIsetRegValue(kCcRegPktCtrl0, kCcPktCtrl0AsyncSerial | 0x02);

    (void)gRadio->SPIsendCommand(kCcCmdSidle);
    (void)gRadio->SPIsendCommand(kCcCmdSfrx);
    (void)gRadio->SPIsendCommand(kCcCmdSrx);

    pinMode(BOARD_CC1101_GDO2, INPUT);
    attachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO2), onSniffEdge, CHANGE);

    Serial.print(F("[CC1101] Mode switched to OOK sniff @ "));
    Serial.print(currentFrequencyMHz(), 2);
    Serial.println(F(" MHz. RxBW=270kHz, async serial on GDO2."));
    return true;
}

bool sendOnePacket(String payload, const bool resumeRx)
{
    if (!gRadio) {
        return false;
    }

    gRadio->clearPacketReceivedAction();
    gRadio->clearPacketSentAction();
    (void)gRadio->finishReceive();
    (void)gRadio->standby();

    Serial.print(F("[CC1101] TX -> "));
    Serial.println(payload);

    const int state = gRadio->transmit(payload);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("[CC1101] TX success."));
        triggerLedEffect(LedEffect::TxFlash);
    } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
        Serial.println(F("[CC1101] TX failed: packet too long."));
    } else {
        Serial.print(F("[CC1101] TX failed, code "));
        Serial.println(state);
    }

    if (resumeRx) {
        return enterReceiveMode();
    }
    return state == RADIOLIB_ERR_NONE;
}

bool reinitializeRadioForCurrentMode()
{
    if (!gRadio) {
        return false;
    }

    detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO0));
    detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO2));
    gRadio->clearPacketReceivedAction();
    gRadio->clearPacketSentAction();
    (void)gRadio->finishReceive();
    (void)gRadio->finishTransmit();
    (void)gRadio->sleep();
    delay(10);

    if (!initRadio()) {
        gRadioReady = false;
        gNeedFooterRedraw = true;
        gScreenDirty = true;
        return false;
    }
    gRadioReady = true;

    if (gCurrentMode == RadioMode::Receive) {
        const bool ok = enterReceiveMode();
        gScreenDirty = true;
        return ok;
    }
    if (gCurrentMode == RadioMode::SniffOok) {
        const bool ok = enterSniffMode();
        gScreenDirty = true;
        return ok;
    }
    enterBurstTransmitMode();
    return true;
}

void handleReceivedPacket()
{
    if (!gRadio || !gPacketReceived) {
        return;
    }
    gPacketReceived = false;

    String payload;
    const int state = gRadio->readData(payload);
    if (state == RADIOLIB_ERR_NONE) {
        gRxCounter++;
        gLastRxPayload = payload;
        gLastRxRssi = gRadio->getRSSI();
        gLastRxLqi = gRadio->getLQI();
        gHasLastRx = true;
        gScreenDirty = true;
        triggerLedEffect(LedEffect::RxFlash);

        Serial.println(F("[CC1101] RX packet received."));
        Serial.print(F("[CC1101] Data: "));
        Serial.println(payload);
        Serial.print(F("[CC1101] RSSI: "));
        Serial.print(gLastRxRssi);
        Serial.println(F(" dBm"));
        Serial.print(F("[CC1101] LQI:  "));
        Serial.println(gLastRxLqi);
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println(F("[CC1101] RX CRC mismatch."));
    } else {
        Serial.print(F("[CC1101] RX readData failed, code "));
        Serial.println(state);
    }

    if (gCurrentMode == RadioMode::Receive) {
        const int restartState = gRadio->startReceive();
        if (restartState != RADIOLIB_ERR_NONE) {
            Serial.print(F("[CC1101] Failed to resume RX, code "));
            Serial.println(restartState);
        }
    }
}

void drainSniffBuffer()
{
    if (gCurrentMode != RadioMode::SniffOok) {
        return;
    }

    noInterrupts();
    const uint32_t isrCount = gIsrEdgeCount;
    interrupts();
    gTotalEdges = isrCount;

    bool burstClosedThisDrain = false;

    while (gPulseTail != gPulseHead) {
        PulseEvent ev = {};
        noInterrupts();
        ev.durationUs = gPulseRing[gPulseTail].durationUs;
        ev.level = gPulseRing[gPulseTail].level;
        gPulseTail = (gPulseTail + 1) % kPulseRingSize;
        interrupts();

        gLastPulseUs = ev.durationUs;
        gLastEdgeAtMs = millis();

        if (ev.durationUs >= kBurstSilenceUs) {
            if (gInBurst && gCurrentBurstPulses >= 4) {
                gLastBurstPulses = gCurrentBurstPulses;
                gLastBurstMinUs = gCurrentBurstMinUs;
                gLastBurstMaxUs = gCurrentBurstMaxUs;
                const uint32_t mid = (gLastBurstMinUs + gLastBurstMaxUs) / 2;
                gLastBurstShortUs = (gLastBurstMinUs + mid) / 2;
                gLastBurstLongUs = (gLastBurstMaxUs + mid) / 2;
                gBurstCount++;
                burstClosedThisDrain = true;

                Serial.print(F("[CC1101] Burst captured: "));
                Serial.print(gCurrentBurstPulses);
                Serial.print(F(" pulses, "));
                Serial.print(gCurrentBurstMinUs);
                Serial.print(F("us .. "));
                Serial.print(gCurrentBurstMaxUs);
                Serial.println(F("us"));
            }
            gInBurst = false;
            gCurrentBurstPulses = 0;
            gCurrentBurstMinUs = 0;
            gCurrentBurstMaxUs = 0;
            continue;
        }

        if (ev.durationUs < kMinValidPulseUs || ev.durationUs > kMaxValidPulseUs) {
            continue;
        }

        if (!gInBurst) {
            gInBurst = true;
            gCurrentBurstPulses = 0;
            gCurrentBurstMinUs = ev.durationUs;
            gCurrentBurstMaxUs = ev.durationUs;
        }
        gCurrentBurstPulses++;
        if (ev.durationUs < gCurrentBurstMinUs) gCurrentBurstMinUs = ev.durationUs;
        if (ev.durationUs > gCurrentBurstMaxUs) gCurrentBurstMaxUs = ev.durationUs;
    }

    if (gInBurst && (millis() - gLastEdgeAtMs > 50)) {
        if (gCurrentBurstPulses >= 4) {
            gLastBurstPulses = gCurrentBurstPulses;
            gLastBurstMinUs = gCurrentBurstMinUs;
            gLastBurstMaxUs = gCurrentBurstMaxUs;
            const uint32_t mid = (gLastBurstMinUs + gLastBurstMaxUs) / 2;
            gLastBurstShortUs = (gLastBurstMinUs + mid) / 2;
            gLastBurstLongUs = (gLastBurstMaxUs + mid) / 2;
            gBurstCount++;
            burstClosedThisDrain = true;
            Serial.print(F("[CC1101] Burst captured (timeout): "));
            Serial.print(gCurrentBurstPulses);
            Serial.println(F(" pulses"));
        }
        gInBurst = false;
        gCurrentBurstPulses = 0;
    }

    const uint32_t now = millis();
    if (burstClosedThisDrain) {
        gLastSniffDrawMs = now;
        gNeedSniffStatsRedraw = true;
        gScreenDirty = true;
        triggerLedEffect(LedEffect::SniffFlash);
    } else if (now - gLastSniffDrawMs > kSniffScreenIntervalMs) {
        gLastSniffDrawMs = now;
        gNeedSniffStatsRedraw = true;
        gScreenDirty = true;
    }
}

bool parseFrequencyToIndex(const String& input, uint8_t& outIndex)
{
    String trimmed = input;
    trimmed.trim();
    if (trimmed.equals("315")) {
        outIndex = 0;
        return true;
    }
    if (trimmed.equals("433") || trimmed.equals("434")) {
        outIndex = 1;
        return true;
    }
    if (trimmed.equals("868")) {
        outIndex = 2;
        return true;
    }
    return false;
}

void handleCommand(String line)
{
    line.trim();
    if (line.isEmpty()) {
        return;
    }

    if (line.equalsIgnoreCase("help")) {
        printHelp();
        return;
    }
    if (line.equalsIgnoreCase("status")) {
        printStatus();
        return;
    }

    if (!gRadioReady) {
        Serial.println(F("[CC1101] Radio not ready."));
        return;
    }

    if (line.equalsIgnoreCase("rx")) {
        (void)enterReceiveMode();
        return;
    }
    if (line.equalsIgnoreCase("tx")) {
        enterBurstTransmitMode();
        return;
    }
    if (line.equalsIgnoreCase("sniff")) {
        (void)enterSniffMode();
        return;
    }

    if (line.startsWith("send ")) {
        String payload = line.substring(5);
        payload.trim();
        if (payload.isEmpty()) {
            Serial.println(F("[CC1101] Empty payload ignored."));
            return;
        }
        const bool resumeRx = (gCurrentMode == RadioMode::Receive);
        (void)sendOnePacket(payload, resumeRx);
        return;
    }

    if (line.startsWith("prefix ")) {
        String prefix = line.substring(7);
        prefix.trim();
        if (prefix.isEmpty()) {
            Serial.println(F("[CC1101] Prefix cannot be empty."));
            return;
        }
        gBurstPrefix = prefix;
        Serial.print(F("[CC1101] TX prefix updated to: "));
        Serial.println(gBurstPrefix);
        return;
    }

    if (line.startsWith("freq ")) {
        uint8_t newIndex = 0;
        if (!parseFrequencyToIndex(line.substring(5), newIndex)) {
            Serial.println(F("[CC1101] Unsupported frequency. Use 315, 433 or 868."));
            return;
        }
        gCurrentFreqIndex = newIndex;
        if (reinitializeRadioForCurrentMode()) {
            Serial.print(F("[CC1101] Frequency switched to "));
            Serial.print(currentFrequencyMHz(), 2);
            Serial.println(F(" MHz."));
        }
        return;
    }

    Serial.print(F("[CC1101] Unknown command: "));
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

void handleBurstTransmit()
{
    if (!gRadioReady || gCurrentMode != RadioMode::BurstTransmit) {
        return;
    }

    const unsigned long now = millis();
    if ((gLastBurstAtMs != 0U) && (now - gLastBurstAtMs < kBurstIntervalMs)) {
        return;
    }

    gLastBurstAtMs = now;
    const String payload = gBurstPrefix + " #" + String(gBurstCounter++);
    (void)sendOnePacket(payload, false);
    gScreenDirty = true;
}

void cycleFrequency()
{
    gCurrentFreqIndex = (gCurrentFreqIndex + 1) % kFreqChoiceCount;
    Serial.print(F("[CC1101] USR key -> freq "));
    Serial.println(currentFrequencyLabel());
    (void)reinitializeRadioForCurrentMode();
}

void toggleMode()
{
    switch (gCurrentMode) {
        case RadioMode::Receive:
            enterBurstTransmitMode();
            break;
        case RadioMode::BurstTransmit:
            (void)enterSniffMode();
            break;
        case RadioMode::SniffOok:
        default:
            (void)enterReceiveMode();
            break;
    }
}

void handleEncoderFocus()
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
            gNeedFooterRedraw = true;
            gScreenDirty = true;
        }
    }
}

void handleButtons()
{
    if (g.usrBtn.event) {
        g.usrBtn.event = false;
        if (gRadioReady) {
            cycleFrequency();
        }
    }

    if (g.encBtn.event) {
        g.encBtn.event = false;
        if (gFocus == FocusItem::Back) {
            requestExitSubPage();
        } else if (gRadioReady) {
            toggleMode();
        }
    }
}

void createRadioObjects()
{
    if (!gModule) {
        gModule = new Module(
            BOARD_CC1101_CS,
            BOARD_CC1101_GDO0,
            RADIOLIB_NC,
            BOARD_CC1101_GDO2,
            sharedSpi());
    }
    if (!gRadio && gModule) {
        gRadio = new CC1101(gModule);
    }
}

void initStrip()
{
    if (gStrip) {
        delete gStrip;
        gStrip = nullptr;
    }
    gStrip = new Adafruit_NeoPixel(BOARD_WS2812_NUM_LEDS, BOARD_WS2812_DATA_PIN, NEO_GRB + NEO_KHZ800);
    gStrip->begin();
    gStrip->setBrightness(255);
    setStripColor(0, 0, 0);
}

}  // namespace

void init()
{
    gCurrentMode = RadioMode::Receive;
    gFocus = FocusItem::Controls;
    gLastDrawnFocus = FocusItem::Controls;
    gCurrentFreqIndex = 1;
    gLastBurstAtMs = 0;
    gBurstCounter = 0;
    gRxCounter = 0;
    gBurstPrefix = kDefaultTxPrefix;
    gLastRxPayload = "";
    gLastRxRssi = 0.0f;
    gLastRxLqi = 0;
    gHasLastRx = false;
    gScreenDirty = true;
    gNeedFullRedraw = true;
    gNeedSniffStatsRedraw = false;
    gNeedFooterRedraw = true;
    gInitOk = false;
    gRadioReady = false;
    gSerialLine = "";
    gEncSnapshot = g.encRaw;
    gPacketReceived = false;
    gPulseHead = 0;
    gPulseTail = 0;
    gLastEdgeUs = 0;
    gIsrEdgeCount = 0;
    gLedEffectUntilMs = 0;
    gLedEffect = LedEffect::None;
    gLedDirty = true;
    gTotalEdges = 0;
    gLastPulseUs = 0;
    gBurstCount = 0;
    gCurrentBurstPulses = 0;
    gCurrentBurstMinUs = 0;
    gCurrentBurstMaxUs = 0;
    gLastBurstPulses = 0;
    gLastBurstMinUs = 0;
    gLastBurstMaxUs = 0;
    gLastBurstShortUs = 0;
    gLastBurstLongUs = 0;
    gLastEdgeAtMs = 0;
    gLastSniffDrawMs = 0;
    gInBurst = false;

    t_embed::board::deselectSharedSpiDevices();
    delay(5);

    createRadioObjects();
    initStrip();

    drawHeader();
    drawFooter(true);

    gInitOk = (gRadio != nullptr && gModule != nullptr);
    if (!gInitOk) {
        Serial.println(F("[CC1101] Failed to create radio objects."));
        return;
    }

    gRadioReady = initRadio();
    if (!gRadioReady) {
        Serial.println(F("[CC1101] Radio init failed."));
    } else if (!enterReceiveMode()) {
        Serial.println(F("[CC1101] Failed to enter RX mode."));
        gRadioReady = false;
    }

    printStatus();
    printHelp();
}

void update()
{
    pollSerialCommands();
    handleEncoderFocus();
    handleButtons();

    if (gRadioReady) {
        handleReceivedPacket();
        handleBurstTransmit();
        drainSniffBuffer();
    }

    checkLedTimeout();
    updateLeds();
}

void render()
{
    if (!gScreenDirty) {
        return;
    }
    gScreenDirty = false;
    redrawAll();
    t_embed::board::deselectSharedSpiDevices();
}

void deinit()
{
    detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO0));
    detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO2));

    if (gRadio) {
        gRadio->clearPacketReceivedAction();
        gRadio->clearPacketSentAction();
        (void)gRadio->finishReceive();
        (void)gRadio->finishTransmit();
        (void)gRadio->standby();
    }

    t_embed::board::deselectSharedSpiDevices();

    if (gStrip) {
        setStripColor(0, 0, 0);
        delete gStrip;
        gStrip = nullptr;
    }
    g.encBtn.event = false;
    g.usrBtn.event = false;
    gInitOk = false;
    gRadioReady = false;
}

}  // namespace page_cc1101
