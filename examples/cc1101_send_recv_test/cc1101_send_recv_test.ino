#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <RadioLib.h>
#include <TFT_eSPI.h>
#include <Adafruit_NeoPixel.h>

#include <TEmbedBoard.h>

namespace {

// ---------- frequency choices ----------
struct FreqChoice {
  float       mhz;
  const char* label;
};

constexpr FreqChoice kFreqChoices[] = {
  {315.0f, "315 MHz"},
  {433.92f, "433.92 MHz"},
  {868.0f, "868 MHz"},
};
constexpr uint8_t kFreqChoiceCount = sizeof(kFreqChoices) / sizeof(kFreqChoices[0]);

// ---------- radio settings ----------
constexpr float    kBitRateKbps           = 1.2f;
constexpr float    kRxBandwidthKHz        = 58.0f;
constexpr float    kFrequencyDeviationKHz = 5.2f;
constexpr int8_t   kOutputPowerDbm        = 10;
constexpr bool     kUseOok                = true;
constexpr uint8_t  kSyncWordHigh          = 0x01;
constexpr uint8_t  kSyncWordLow           = 0x23;
constexpr uint32_t kBurstIntervalMs       = 1000;
constexpr char     kDefaultTxPrefix[]     = "T-Embed CC1101";

// ---------- display layout ----------
constexpr uint8_t  kRotation     = 1;
constexpr uint8_t  kMarginLeft   = 8;
constexpr int16_t  kHeaderHeight = 24;
constexpr int16_t  kFooterHeight = 18;
constexpr uint32_t kBusSettleMs  = 20;

constexpr int16_t  kRowFreqLabel = 30;
constexpr int16_t  kRowFreqValue = 30;
constexpr int16_t  kRowMode      = 64;
constexpr int16_t  kRowDivider   = 86;
constexpr int16_t  kRowRxLabel   = 92;
constexpr int16_t  kRowRxData    = 106;
constexpr int16_t  kRowRxMeta    = 122;
constexpr int16_t  kRowTxCount   = 138;

// ---------- runtime state ----------
enum class RadioMode : uint8_t {
  Receive,
  BurstTransmit,
  SniffOok,
};

// CC1101 register / strobe addresses (subset we touch directly for sniff mode)
constexpr uint8_t kCcRegIocfg2   = 0x00;
constexpr uint8_t kCcRegIocfg0   = 0x02;
constexpr uint8_t kCcRegPktCtrl0 = 0x08;
constexpr uint8_t kCcRegMdmCfg2  = 0x12;
constexpr uint8_t kCcCmdSidle    = 0x36;
constexpr uint8_t kCcCmdSfrx     = 0x3A;
constexpr uint8_t kCcCmdSrx      = 0x34;
constexpr uint8_t kCcGdoSerialDataAsync = 0x0D;
constexpr uint8_t kCcPktCtrl0AsyncSerial = 0x30; // bits [5:4]=11
constexpr float   kSniffRxBwKHz   = 270.0f;
constexpr float   kSniffBitRateKbps = 50.0f;

SPIClass     radioSPI(HSPI);
CC1101       radio = new Module(BOARD_CC1101_CS, BOARD_CC1101_GDO0, RADIOLIB_NC, BOARD_CC1101_GDO2, radioSPI);
TEmbedXL9555 ioExpander;
TFT_eSPI     tft;
Adafruit_NeoPixel strip(BOARD_WS2812_NUM_LEDS, BOARD_WS2812_DATA_PIN, NEO_GRB + NEO_KHZ800);

volatile bool packetReceived = false;

RadioMode currentMode       = RadioMode::Receive;
uint8_t   currentFreqIndex  = 1;  // default 433 MHz
unsigned long lastBurstAtMs = 0;
uint32_t  burstCounter      = 0;
uint32_t  rxCounter         = 0;
String    burstPrefix       = kDefaultTxPrefix;

String    lastRxPayload;
float     lastRxRssi = 0.0f;
uint8_t   lastRxLqi  = 0;
bool      hasLastRx  = false;

bool screenDirty   = true;
bool radioReady    = false;

// Render-throttle state: incremental redraws of just the volatile rows so the
// sniff mode stops flickering. Set when the next loop tick should refresh.
bool needFullRedraw      = true;   // changed mode/freq -> repaint full body
bool needSniffStatsRedraw = false; // sniff stats only

// LED effect state
enum class LedEffect : uint8_t {
  None = 0,
  RxFlash,
  TxFlash,
  SniffFlash,
};

uint32_t gLedEffectUntilMs = 0;
LedEffect gLedEffect       = LedEffect::None;
bool      gLedDirty        = true;
constexpr uint8_t  kLedBrightness    = 10;
constexpr uint32_t kLedFlashMs       = 200;

// ---------- OOK sniff state ----------
struct PulseEvent {
  uint32_t durationUs;
  uint8_t  level;     // pin level AFTER the edge (the new state)
};

constexpr size_t   kPulseRingSize       = 256;
constexpr uint32_t kBurstSilenceUs      = 5000;    // >=5 ms gap ends a burst
constexpr uint32_t kMinValidPulseUs     = 80;       // ignore obvious glitches
constexpr uint32_t kMaxValidPulseUs     = 20000;
constexpr uint32_t kSniffScreenIntervalMs = 250;

volatile PulseEvent gPulseRing[kPulseRingSize];
volatile uint16_t   gPulseHead = 0;       // ISR writes here
volatile uint16_t   gPulseTail = 0;       // main loop reads from here
volatile uint32_t   gLastEdgeUs = 0;
volatile uint32_t   gIsrEdgeCount = 0;

// Stats updated by main-loop drain
uint32_t gTotalEdges       = 0;
uint32_t gLastPulseUs      = 0;
uint32_t gBurstCount       = 0;
uint16_t gCurrentBurstPulses = 0;
uint32_t gCurrentBurstMinUs = 0;
uint32_t gCurrentBurstMaxUs = 0;
uint16_t gLastBurstPulses  = 0;
uint32_t gLastBurstMinUs   = 0;
uint32_t gLastBurstMaxUs   = 0;
uint32_t gLastBurstShortUs = 0;   // average of pulses below median
uint32_t gLastBurstLongUs  = 0;   // average of pulses above median
uint32_t gLastEdgeAtMs     = 0;
uint32_t gLastSniffDrawMs  = 0;
bool     gInBurst          = false;

inline float currentFrequencyMHz() {
  return kFreqChoices[currentFreqIndex].mhz;
}

inline const char* currentFrequencyLabel() {
  return kFreqChoices[currentFreqIndex].label;
}

#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void onPacketReceived() {
  packetReceived = true;
}

#if defined(ESP8266) || defined(ESP32)
IRAM_ATTR
#endif
void onSniffEdge() {
  const uint32_t now = micros();
  const uint32_t dur = now - gLastEdgeUs;
  gLastEdgeUs = now;

  const uint8_t level = (uint8_t)digitalRead(BOARD_CC1101_GDO2);

  uint16_t next = (gPulseHead + 1) % kPulseRingSize;
  if (next != gPulseTail) {
    gPulseRing[gPulseHead].durationUs = dur;
    gPulseRing[gPulseHead].level      = level;
    gPulseHead = next;
  }
  gIsrEdgeCount++;
}

const __FlashStringHelper* modeLabel(RadioMode mode) {
  switch (mode) {
    case RadioMode::Receive:       return F("RX");
    case RadioMode::BurstTransmit: return F("TX-BURST");
    case RadioMode::SniffOok:      return F("SNIFF-OOK");
  }
  return F("?");
}

// ---------- LED effects ----------

void triggerLedEffect(LedEffect effect) {
  gLedEffect = effect;
  gLedEffectUntilMs = millis() + kLedFlashMs;
  gLedDirty = true;
}

void setStripColor(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < BOARD_WS2812_NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

void updateLeds() {
  if (!gLedDirty) return;
  gLedDirty = false;

  if (gLedEffect == LedEffect::None || millis() > gLedEffectUntilMs) {
    gLedEffect = LedEffect::None;
    setStripColor(0, 0, 0);
    return;
  }

  switch (gLedEffect) {
    case LedEffect::RxFlash:    setStripColor(0, kLedBrightness, 0);   break; // green
    case LedEffect::TxFlash:    setStripColor(0, 0, kLedBrightness);   break; // blue
    case LedEffect::SniffFlash: setStripColor(kLedBrightness, 0, kLedBrightness); break; // magenta
    default:                    setStripColor(0, 0, 0);                break;
  }
}

void checkLedTimeout() {
  if (gLedEffect != LedEffect::None && millis() > gLedEffectUntilMs) {
    gLedDirty = true; // will turn off on next updateLeds()
  }
}

// ---------- display helpers ----------

void drawHeader() {
  tft.fillRect(0, 0, tft.width(), kHeaderHeight, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("CC1101 Send / Recv", kMarginLeft, 6, 2);
}

void drawFooter(const char* msg) {
  const int16_t y = tft.height() - kFooterHeight;
  tft.fillRect(0, y, tft.width(), kFooterHeight, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString(msg, kMarginLeft, y + 3, 1);
}

void drawFreqRow() {
  tft.fillRect(0, kRowFreqValue, tft.width(), 32, TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("FREQ:", kMarginLeft, kRowFreqValue + 8, 1);

  uint16_t color;
  switch (currentFreqIndex) {
    case 0: color = TFT_GREEN;  break;
    case 1: color = TFT_YELLOW; break;
    default: color = TFT_ORANGE; break;
  }
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(currentFrequencyLabel(), kMarginLeft + 50, kRowFreqValue, 4);
}

void drawModeRow() {
  tft.fillRect(0, kRowMode, tft.width(), 18, TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("MODE:", kMarginLeft, kRowMode + 2, 1);

  uint16_t color;
  const char* label;
  switch (currentMode) {
    case RadioMode::Receive:       color = TFT_GREEN;   label = "RX";        break;
    case RadioMode::BurstTransmit: color = TFT_ORANGE;  label = "TX BURST";  break;
    case RadioMode::SniffOok:      color = TFT_MAGENTA; label = "SNIFF OOK"; break;
    default:                       color = TFT_WHITE;   label = "?";         break;
  }
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(label, kMarginLeft + 50, kRowMode, 2);
}

void drawDivider() {
  tft.drawFastHLine(0, kRowDivider, tft.width(), TFT_DARKGREY);
}

void drawRxRow() {
  tft.fillRect(0, kRowRxLabel, tft.width(), kRowTxCount - kRowRxLabel, TFT_BLACK);

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Last RX:", kMarginLeft, kRowRxLabel, 1);

  if (!hasLastRx) {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("(none)", kMarginLeft + 60, kRowRxLabel, 1);
    return;
  }

  String payload = lastRxPayload;
  if (payload.length() > 36) {
    payload = payload.substring(0, 33) + "...";
  }
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(payload, kMarginLeft, kRowRxData, 1);

  char meta[40];
  snprintf(meta, sizeof(meta), "RSSI:%.0f dBm  LQI:%u  #%lu",
           lastRxRssi, (unsigned)lastRxLqi, (unsigned long)rxCounter);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(meta, kMarginLeft, kRowRxMeta, 1);
}

void drawTxRow() {
  tft.fillRect(0, kRowTxCount, tft.width(),
               tft.height() - kFooterHeight - kRowTxCount, TFT_BLACK);
  char buf[32];
  snprintf(buf, sizeof(buf), "TX count: %lu", (unsigned long)burstCounter);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(buf, kMarginLeft, kRowTxCount, 1);
}

void drawSniffRows() {
  // Only clear the region we draw into, line by line, so the screen does not flash.
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.fillRect(0, kRowRxLabel, tft.width(), 14, TFT_BLACK);
  tft.drawString("OOK sniff:", kMarginLeft, kRowRxLabel, 1);

  char l1[48];
  snprintf(l1, sizeof(l1), "Edges: %lu  Bursts: %lu",
           (unsigned long)gTotalEdges, (unsigned long)gBurstCount);
  tft.fillRect(0, kRowRxData, tft.width(), 14, TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(l1, kMarginLeft, kRowRxData, 1);

  char l2[48];
  tft.fillRect(0, kRowRxMeta, tft.width(), 14, TFT_BLACK);
  if (gLastBurstPulses > 0) {
    snprintf(l2, sizeof(l2), "Last: %u p  short~%luus  long~%luus",
             gLastBurstPulses,
             (unsigned long)gLastBurstShortUs,
             (unsigned long)gLastBurstLongUs);
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
             (unsigned long)gLastBurstMinUs, (unsigned long)gLastBurstMaxUs);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(l3, kMarginLeft, kRowTxCount, 1);
  }
}

void redrawAll() {
  if (needFullRedraw) {
    needFullRedraw = false;
    tft.fillRect(0, kHeaderHeight, tft.width(),
                 tft.height() - kHeaderHeight - kFooterHeight, TFT_BLACK);
    drawFreqRow();
    drawModeRow();
    drawDivider();
    if (currentMode == RadioMode::SniffOok) {
      drawSniffRows();
    } else {
      drawRxRow();
      drawTxRow();
    }
    if (!radioReady) {
      drawFooter("Radio init failed");
    } else if (currentMode == RadioMode::SniffOok) {
      drawFooter("USR=freq  KEY=next mode");
    } else {
      drawFooter("USR=freq  KEY=next mode");
    }
    return;
  }

  // Incremental updates only — keep static rows as-is to avoid flicker.
  if (currentMode == RadioMode::SniffOok && needSniffStatsRedraw) {
    needSniffStatsRedraw = false;
    drawSniffRows();
  } else if (currentMode == RadioMode::Receive) {
    drawRxRow();
  } else if (currentMode == RadioMode::BurstTransmit) {
    drawTxRow();
  }
}

bool initDisplayPower() {
  t_embed::board::deselectSharedSpiDevices();

  if (!t_embed::board::beginExpander(ioExpander)) {
    Serial.println(F("[CC1101] XL9555 init failed."));
    return false;
  }
  if (!t_embed::board::setLowPowerEnabled(ioExpander, true)) {
    Serial.println(F("[CC1101] Failed to enable LOW_PWR_3V3."));
    return false;
  }
  delay(kBusSettleMs);

  pinMode(BOARD_LCD_BL, OUTPUT);
  digitalWrite(BOARD_LCD_BL, HIGH);

  if (!t_embed::board::setLcdReset(ioExpander, true))  return false;
  delay(5);
  if (!t_embed::board::setLcdReset(ioExpander, false)) return false;
  delay(20);
  if (!t_embed::board::setLcdReset(ioExpander, true))  return false;
  delay(120);
  return true;
}

// ---------- radio helpers ----------

void printHelp() {
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
  Serial.println(F("  ENC key   - cycle mode RX -> TX -> SNIFF"));
  Serial.println();
}

void printStatus() {
  Serial.println();
  Serial.print(F("[CC1101] Mode:        "));
  Serial.println(modeLabel(currentMode));
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
  Serial.println(burstPrefix);
}

bool applyRadioSettings() {
  int state = radio.begin(currentFrequencyMHz());
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[CC1101] radio.begin failed, code "));
    Serial.println(state);
    return false;
  }
  state = radio.setFrequency(currentFrequencyMHz());
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[CC1101] setFrequency failed, code "));
    Serial.println(state);
    return false;
  }
  state = radio.setOOK(kUseOok);
  if (state != RADIOLIB_ERR_NONE) return false;
  state = radio.setBitRate(kBitRateKbps);
  if (state != RADIOLIB_ERR_NONE) return false;
  state = radio.setRxBandwidth(kRxBandwidthKHz);
  if (state != RADIOLIB_ERR_NONE) return false;
  state = radio.setFrequencyDeviation(kFrequencyDeviationKHz);
  if (state != RADIOLIB_ERR_NONE) return false;
  state = radio.setOutputPower(kOutputPowerDbm);
  if (state != RADIOLIB_ERR_NONE) return false;
  state = radio.setSyncWord(kSyncWordHigh, kSyncWordLow);
  if (state != RADIOLIB_ERR_NONE) return false;
  return true;
}

bool initRadio() {
  t_embed::board::deselectSharedSpiDevices();

  if (!t_embed::board::setCc1101RfPath(ioExpander, currentFrequencyMHz())) {
    Serial.println(F("[CC1101] Unsupported frequency for board RF switch."));
    return false;
  }

  radioSPI.begin(BOARD_CC1101_SCK, BOARD_CC1101_MISO, BOARD_CC1101_MOSI);
  delay(20);

  Serial.print(F("[CC1101] Initializing at "));
  Serial.print(currentFrequencyMHz(), 2);
  Serial.println(F(" MHz ..."));

  return applyRadioSettings();
}

// Tear down sniff-mode register state and put the radio back into normal
// packet mode. Must be called whenever we leave SniffOok before going to RX/TX.
void leaveSniffMode() {
  detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO0));
  detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO2));
  (void)radio.SPIsendCommand(kCcCmdSidle);
  (void)radio.SPIsendCommand(kCcCmdSfrx);
  // Re-apply packet-mode settings (this rewrites PKTCTRL0, MDMCFG2 sync, BR, BW, sync word).
  (void)applyRadioSettings();
}

bool enterReceiveMode() {
  const bool wasSniff = (currentMode == RadioMode::SniffOok);
  currentMode = RadioMode::Receive;
  packetReceived = false;
  needFullRedraw = true;
  screenDirty = true;

  if (wasSniff) {
    leaveSniffMode();
  }
  detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO0));
  detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO2));
  radio.clearPacketSentAction();
  radio.clearPacketReceivedAction();
  (void)radio.finishTransmit();
  (void)radio.standby();

  radio.setPacketReceivedAction(onPacketReceived);
  int state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[CC1101] startReceive failed, code "));
    Serial.println(state);
    return false;
  }
  Serial.println(F("[CC1101] Mode switched to RX."));
  return true;
}

void enterBurstTransmitMode() {
  const bool wasSniff = (currentMode == RadioMode::SniffOok);
  currentMode = RadioMode::BurstTransmit;
  lastBurstAtMs = 0;
  needFullRedraw = true;
  screenDirty = true;

  if (wasSniff) {
    leaveSniffMode();
  }
  detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO0));
  detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO2));
  radio.clearPacketReceivedAction();
  radio.clearPacketSentAction();
  (void)radio.finishReceive();
  (void)radio.standby();

  Serial.println(F("[CC1101] Mode switched to TX burst. Sending once per second."));
}

bool enterSniffMode() {
  currentMode = RadioMode::SniffOok;
  needFullRedraw = true;
  screenDirty = true;

  // Reset stats
  gPulseHead = gPulseTail = 0;
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
  radio.clearPacketReceivedAction();
  radio.clearPacketSentAction();
  (void)radio.finishReceive();
  (void)radio.finishTransmit();
  (void)radio.standby();

  // Bruce-style raw OOK reception:
  //  - wider RX bandwidth (~270 kHz) so we don't miss off-frequency remotes
  //  - higher baud (50 kbps) keeps AGC time constant short
  //  - async serial mode: GDO0/GDO2 directly output the demodulated bit stream
  //  - sync mode = 0 (no preamble/sync) so the packet handler does not gate the bits
  int state = radio.setOOK(true);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[CC1101] sniff: setOOK fail "));
    Serial.println(state);
    return false;
  }
  state = radio.setRxBandwidth(kSniffRxBwKHz);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[CC1101] sniff: setRxBandwidth fail "));
    Serial.println(state);
  }
  state = radio.setBitRate(kSniffBitRateKbps);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[CC1101] sniff: setBitRate fail "));
    Serial.println(state);
  }
  // Disable sync word matching (sync mode = 0 in MDMCFG2[2:0]).
  radio.SPIsetRegValue(kCcRegMdmCfg2, 0x00, 2, 0);
  // Route the demodulated bitstream to BOTH GDO pins. We listen on GDO2.
  radio.SPIsetRegValue(kCcRegIocfg0, kCcGdoSerialDataAsync);
  radio.SPIsetRegValue(kCcRegIocfg2, kCcGdoSerialDataAsync);
  // PKTCTRL0: bits [5:4]=11 -> async serial mode, length config kept at variable (0x02).
  radio.SPIsetRegValue(kCcRegPktCtrl0, kCcPktCtrl0AsyncSerial | 0x02);

  // Enter RX
  (void)radio.SPIsendCommand(kCcCmdSidle);
  (void)radio.SPIsendCommand(kCcCmdSfrx);
  (void)radio.SPIsendCommand(kCcCmdSrx);

  // Bitstream tap: GDO2 (Bruce uses GDO2; GDO0 also carries it but GDO2 is preferred).
  pinMode(BOARD_CC1101_GDO2, INPUT);
  attachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO2),
                  onSniffEdge, CHANGE);

  Serial.print(F("[CC1101] Mode switched to OOK sniff @ "));
  Serial.print(currentFrequencyMHz(), 2);
  Serial.println(F(" MHz. RxBW=270kHz, async serial on GDO2."));
  return true;
}

bool sendOnePacket(String payload, bool resumeRx) {
  radio.clearPacketReceivedAction();
  radio.clearPacketSentAction();
  (void)radio.finishReceive();
  (void)radio.standby();

  Serial.print(F("[CC1101] TX -> "));
  Serial.println(payload);

  int state = radio.transmit(payload);
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

bool reinitializeRadioForCurrentMode() {
  detachInterrupt(digitalPinToInterrupt(BOARD_CC1101_GDO0));
  radio.clearPacketReceivedAction();
  radio.clearPacketSentAction();
  (void)radio.finishReceive();
  (void)radio.finishTransmit();
  (void)radio.sleep();
  delay(10);

  if (!initRadio()) {
    radioReady = false;
    screenDirty = true;
    return false;
  }
  radioReady = true;

  if (currentMode == RadioMode::Receive) {
    bool ok = enterReceiveMode();
    screenDirty = true;
    return ok;
  }
  if (currentMode == RadioMode::SniffOok) {
    bool ok = enterSniffMode();
    screenDirty = true;
    return ok;
  }
  enterBurstTransmitMode();
  return true;
}

void handleReceivedPacket() {
  if (!packetReceived) return;
  packetReceived = false;

  String payload;
  int state = radio.readData(payload);
  if (state == RADIOLIB_ERR_NONE) {
    rxCounter++;
    lastRxPayload = payload;
    lastRxRssi    = radio.getRSSI();
    lastRxLqi     = radio.getLQI();
    hasLastRx     = true;
    screenDirty   = true;
    triggerLedEffect(LedEffect::RxFlash);

    Serial.println(F("[CC1101] RX packet received."));
    Serial.print(F("[CC1101] Data: "));
    Serial.println(payload);
    Serial.print(F("[CC1101] RSSI: "));
    Serial.print(lastRxRssi);
    Serial.println(F(" dBm"));
    Serial.print(F("[CC1101] LQI:  "));
    Serial.println(lastRxLqi);
  } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    Serial.println(F("[CC1101] RX CRC mismatch."));
  } else {
    Serial.print(F("[CC1101] RX readData failed, code "));
    Serial.println(state);
  }

  if (currentMode == RadioMode::Receive) {
    int restartState = radio.startReceive();
    if (restartState != RADIOLIB_ERR_NONE) {
      Serial.print(F("[CC1101] Failed to resume RX, code "));
      Serial.println(restartState);
    }
  }
}

void drainSniffBuffer() {
  if (currentMode != RadioMode::SniffOok) return;

  // Snapshot ISR counter
  noInterrupts();
  const uint32_t isrCount = gIsrEdgeCount;
  interrupts();
  gTotalEdges = isrCount;

  bool burstClosedThisDrain = false;

  while (gPulseTail != gPulseHead) {
    PulseEvent ev;
    noInterrupts();
    ev.durationUs = gPulseRing[gPulseTail].durationUs;
    ev.level      = gPulseRing[gPulseTail].level;
    gPulseTail    = (gPulseTail + 1) % kPulseRingSize;
    interrupts();

    gLastPulseUs = ev.durationUs;
    gLastEdgeAtMs = millis();

    // A long silence ends the previous burst
    if (ev.durationUs >= kBurstSilenceUs) {
      if (gInBurst && gCurrentBurstPulses >= 4) {
        gLastBurstPulses = gCurrentBurstPulses;
        gLastBurstMinUs  = gCurrentBurstMinUs;
        gLastBurstMaxUs  = gCurrentBurstMaxUs;
        // simple short/long estimate: midpoint between min and max
        const uint32_t mid = (gLastBurstMinUs + gLastBurstMaxUs) / 2;
        gLastBurstShortUs = (gLastBurstMinUs + mid) / 2;
        gLastBurstLongUs  = (gLastBurstMaxUs + mid) / 2;
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

  // Also close a long-stale in-flight burst (no new edges for >50 ms)
  if (gInBurst && (millis() - gLastEdgeAtMs > 50)) {
    if (gCurrentBurstPulses >= 4) {
      gLastBurstPulses = gCurrentBurstPulses;
      gLastBurstMinUs  = gCurrentBurstMinUs;
      gLastBurstMaxUs  = gCurrentBurstMaxUs;
      const uint32_t mid = (gLastBurstMinUs + gLastBurstMaxUs) / 2;
      gLastBurstShortUs = (gLastBurstMinUs + mid) / 2;
      gLastBurstLongUs  = (gLastBurstMaxUs + mid) / 2;
      gBurstCount++;
      burstClosedThisDrain = true;
      Serial.print(F("[CC1101] Burst captured (timeout): "));
      Serial.print(gCurrentBurstPulses);
      Serial.println(F(" pulses"));
    }
    gInBurst = false;
    gCurrentBurstPulses = 0;
  }

  // Throttle redraws to keep loop responsive
  const uint32_t now = millis();
  if (burstClosedThisDrain) {
    gLastSniffDrawMs = now;
    needSniffStatsRedraw = true;
    screenDirty = true;
    triggerLedEffect(LedEffect::SniffFlash);
  } else if (now - gLastSniffDrawMs > kSniffScreenIntervalMs) {
    gLastSniffDrawMs = now;
    needSniffStatsRedraw = true;
    screenDirty = true;
  }
}

bool parseFrequencyToIndex(const String& input, uint8_t& outIndex) {
  String trimmed = input;
  trimmed.trim();
  if (trimmed.equals("315"))  { outIndex = 0; return true; }
  if (trimmed.equals("433") || trimmed.equals("434")) { outIndex = 1; return true; }
  if (trimmed.equals("868"))  { outIndex = 2; return true; }
  return false;
}

void handleCommand(String line) {
  line.trim();
  if (line.isEmpty()) return;

  if (line.equalsIgnoreCase("help")) { printHelp(); return; }
  if (line.equalsIgnoreCase("status")) { printStatus(); return; }

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
    bool resumeRx = (currentMode == RadioMode::Receive);
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
    burstPrefix = prefix;
    Serial.print(F("[CC1101] TX prefix updated to: "));
    Serial.println(burstPrefix);
    return;
  }

  if (line.startsWith("freq ")) {
    uint8_t newIndex = 0;
    if (!parseFrequencyToIndex(line.substring(5), newIndex)) {
      Serial.println(F("[CC1101] Unsupported frequency. Use 315, 433 or 868."));
      return;
    }
    currentFreqIndex = newIndex;
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

void pollSerialCommands() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  handleCommand(line);
}

void handleBurstTransmit() {
  if (currentMode != RadioMode::BurstTransmit) return;

  const unsigned long now = millis();
  if ((lastBurstAtMs != 0U) && (now - lastBurstAtMs < kBurstIntervalMs)) return;

  lastBurstAtMs = now;
  String payload = burstPrefix + " #" + String(burstCounter++);
  (void)sendOnePacket(payload, false);
  screenDirty = true;
}

void cycleFrequency() {
  currentFreqIndex = (currentFreqIndex + 1) % kFreqChoiceCount;
  Serial.print(F("[CC1101] USR key -> freq "));
  Serial.println(currentFrequencyLabel());
  reinitializeRadioForCurrentMode();
}

void toggleMode() {
  switch (currentMode) {
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

void pollButtons() {
  static bool lastUsr = false;
  static bool lastEnc = false;

  const bool usr = (digitalRead(BOARD_USER_KEY) == LOW);
  const bool enc = (digitalRead(ENCODER_KEY)   == LOW);

  if (usr && !lastUsr) cycleFrequency();
  if (enc && !lastEnc) toggleMode();

  lastUsr = usr;
  lastEnc = enc;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("T-Embed CC1101 send/receive test"));

  if (!initDisplayPower()) {
    Serial.println(F("[CC1101] Display power init failed - halting."));
    while (true) { delay(1000); }
  }

  tft.init();
  tft.setRotation(kRotation);
  tft.fillScreen(TFT_BLACK);
  t_embed::board::deselectSharedSpiDevices();

  pinMode(BOARD_USER_KEY, INPUT_PULLUP);
  pinMode(ENCODER_KEY,    INPUT_PULLUP);

  strip.begin();
  strip.setBrightness(255);  // raw color values already scaled by kLedBrightness
  setStripColor(0, 0, 0);

  drawHeader();
  drawFooter("Initializing radio...");

  radioReady = initRadio();
  if (!radioReady) {
    Serial.println(F("[CC1101] Radio init failed."));
  } else {
    if (!enterReceiveMode()) {
      Serial.println(F("[CC1101] Failed to enter RX mode."));
    }
  }

  redrawAll();
  printStatus();
  printHelp();
}

void loop() {
  pollSerialCommands();
  pollButtons();
  if (radioReady) {
    handleReceivedPacket();
    handleBurstTransmit();
    drainSniffBuffer();
  }

  checkLedTimeout();
  updateLeds();

  if (screenDirty) {
    screenDirty = false;
    redrawAll();
  }

  delay(2);
}
