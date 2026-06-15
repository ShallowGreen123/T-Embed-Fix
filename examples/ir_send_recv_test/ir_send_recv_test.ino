#include <Arduino.h>
#include <TFT_eSPI.h>

#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>

#include <Adafruit_NeoPixel.h>
#include <TEmbedBoard.h>

namespace {

// ---- pins ----
constexpr uint8_t kIrTxPin  = BOARD_IR_TX;
constexpr uint8_t kIrRxPin  = BOARD_IR_RX;
constexpr uint8_t kUsrKey   = BOARD_USER_KEY;   // BOOT key
constexpr uint8_t kEncA     = ENCODER_INA;
constexpr uint8_t kEncB     = ENCODER_INB;
constexpr uint8_t kEncKey   = ENCODER_KEY;
constexpr uint8_t kLedPin   = BOARD_WS2812_DATA_PIN;
constexpr uint8_t kLedCount = BOARD_WS2812_NUM_LEDS;

// ---- display ----
constexpr uint8_t kRotation = 1;  // 320 x 170

// ---- IR ----
constexpr uint16_t kIrCaptureBufSize = 1024;
constexpr uint8_t  kIrTimeoutMs      = 50;
constexpr uint32_t kDebounceMs       = 20;

// ---- loopback ----
constexpr uint32_t kLoopbackIntervalMs = 1500;

// ---- LED flash ----
constexpr uint32_t kLedFlashMs = 120;

// ---- echo suppression ----
// IR TX & RX are close enough on this board that the receiver always picks
// up our own transmission. We treat any decode within this window after a TX
// as a self-echo and choose what to do per-mode.
constexpr uint32_t kEchoWindowMs = 250;

// ---- preset codes ----
struct IrPreset {
  const char*   name;
  decode_type_t protocol;
  uint64_t      value;
  uint16_t      bits;
};

const IrPreset kPresets[] = {
  {"NEC  A",  NEC,     0x20DF10EFULL, 32},
  {"NEC  B",  NEC,     0x20DF40BFULL, 32},
  {"Sony12",  SONY,    0x00000A90ULL, 12},
  {"Samsung", SAMSUNG, 0xE0E040BFULL, 32},
  {"RC5",     RC5,     0x00000010ULL, 12},
};
constexpr uint8_t kPresetCount = sizeof(kPresets) / sizeof(kPresets[0]);

TEmbedXL9555       ioExpander;
TFT_eSPI           tft;
IRsend             irsend(kIrTxPin);
IRrecv             irrecv(kIrRxPin, kIrCaptureBufSize, kIrTimeoutMs, true);
decode_results     irRx;
Adafruit_NeoPixel  leds(kLedCount, kLedPin, NEO_GRB + NEO_KHZ800);

uint8_t  presetIndex  = 0;
bool     loopbackMode = false;
uint32_t lastTxMs     = 0;   // used for echo suppression

// ---- runtime state ----
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

RxInfo rxInfo;
TxInfo txInfo;

// ---- LED flash state ----
struct LedFlash {
  bool     active = false;
  uint32_t endMs  = 0;
};
LedFlash ledFlash;

// ---- button debounce ----
struct ButtonState {
  uint8_t  pin;
  bool     pressed;
  bool     pressedEvent;
  uint32_t lastChangeMs;
};

ButtonState usrBtn = {kUsrKey, false, false, 0};
ButtonState encBtn = {kEncKey, false, false, 0};

// ---- dirty flags (per region) ----
struct Dirty {
  bool chrome     = true;  // static frame, panel borders
  bool header     = true;  // header bar incl. LOOP badge
  bool preset     = true;  // preset banner
  bool tx         = true;  // TX panel values
  bool rx         = true;  // RX panel values
  bool txTime     = true;  // TX timestamp only
  bool rxTime     = true;  // RX timestamp only
};
Dirty dirty;

// ---- encoder state ----
volatile int32_t encoderCount = 0;
volatile uint8_t prevAB = 0;
static const int8_t kEncTable[4][4] = {
  { 0, -1,  1,  0},
  { 1,  0,  0, -1},
  {-1,  0,  0,  1},
  { 0,  1, -1,  0},
};

// ---- helpers ----
String fmtElapsed(uint32_t lastMs) {
  if (lastMs == 0) return "--";
  uint32_t sec = (millis() - lastMs) / 1000;
  if (sec < 60)   return String(sec) + "s ago";
  if (sec < 3600) return String(sec / 60) + "m ago";
  return String(sec / 3600) + "h ago";
}

// --------------------------------------------------------
// LED flash (non-blocking)
// --------------------------------------------------------
void startLedFlash(uint8_t r, uint8_t g, uint8_t b) {
  ledFlash.active = true;
  ledFlash.endMs  = millis() + kLedFlashMs;
  for (int i = 0; i < kLedCount; i++)
    leds.setPixelColor(i, leds.Color(r, g, b));
  leds.show();
}

void pollLedFlash() {
  if (!ledFlash.active) return;
  if (millis() >= ledFlash.endMs) {
    ledFlash.active = false;
    leds.clear();
    leds.show();
  }
}

// --------------------------------------------------------
// Encoder ISR
// --------------------------------------------------------
void IRAM_ATTR onEncoderChange() {
  const uint8_t a = digitalRead(kEncA);
  const uint8_t b = digitalRead(kEncB);
  const uint8_t cur = (a << 1) | b;
  encoderCount += kEncTable[prevAB][cur];
  prevAB = cur;
}

// --------------------------------------------------------
// Button poll
// --------------------------------------------------------
void pollButton(ButtonState& btn) {
  const bool raw = (digitalRead(btn.pin) == LOW);
  const uint32_t now = millis();
  if (raw != btn.pressed && (now - btn.lastChangeMs) >= kDebounceMs) {
    btn.lastChangeMs = now;
    btn.pressed = raw;
    if (raw) btn.pressedEvent = true;
  }
}

// --------------------------------------------------------
// Display init
// --------------------------------------------------------
bool initDisplay() {
  t_embed::board::deselectSharedSpiDevices();

  if (!t_embed::board::beginExpander(ioExpander)) {
    Serial.println(F("[IR] XL9555 init failed."));
    return false;
  }
  if (!t_embed::board::setLowPowerEnabled(ioExpander, true)) {
    Serial.println(F("[IR] LOW_PWR_3V3 enable failed."));
    return false;
  }

  pinMode(BOARD_LCD_BL, OUTPUT);
  digitalWrite(BOARD_LCD_BL, HIGH);

  t_embed::board::setLcdReset(ioExpander, true);
  delay(5);
  t_embed::board::setLcdReset(ioExpander, false);
  delay(20);
  t_embed::board::setLcdReset(ioExpander, true);
  delay(120);
  return true;
}

// --------------------------------------------------------
// IR send
// --------------------------------------------------------
bool sendPreset(const IrPreset& preset) {
  switch (preset.protocol) {
    case NEC:     irsend.sendNEC(preset.value, preset.bits);       return true;
    case SONY:    irsend.sendSony(preset.value, preset.bits, 2);   return true;
    case SAMSUNG: irsend.sendSAMSUNG(preset.value, preset.bits);   return true;
    case RC5:     irsend.sendRC5(preset.value, preset.bits);       return true;
    default:      return false;
  }
}

void doSendCurrentPreset() {
  const IrPreset& p = kPresets[presetIndex];

  Serial.print(F("[IR] TX -> "));
  Serial.print(p.name);
  Serial.print(F("  proto="));
  Serial.print(typeToString(p.protocol));
  Serial.print(F("  value=0x"));
  Serial.print(uint64ToString(p.value, 16));
  Serial.print(F("  bits="));
  Serial.println(p.bits);

  if (!sendPreset(p)) {
    Serial.println(F("[IR] Unsupported preset protocol."));
    return;
  }

  txInfo.valid    = true;
  txInfo.name     = p.name;
  txInfo.protocol = String(typeToString(p.protocol));
  txInfo.valueHex = "0x" + String(uint64ToString(p.value, 16));
  txInfo.bits     = p.bits;
  txInfo.lastMs   = millis();
  lastTxMs        = millis();
  ++txInfo.count;
  dirty.tx     = true;
  dirty.txTime = true;

  startLedFlash(0, 0, 80);  // blue on TX
}

// --------------------------------------------------------
// IR receive
// --------------------------------------------------------
void pollIrReceive() {
  if (!irrecv.decode(&irRx)) return;

  // Is this our own TX echoing back into the receiver?
  const uint32_t now = millis();
  const bool isSelfEcho = (lastTxMs != 0) && ((now - lastTxMs) < kEchoWindowMs);

  // Non-loopback: self-echo must NOT clobber the RX panel.
  if (isSelfEcho && !loopbackMode) {
    irrecv.resume();
    return;
  }

  rxInfo.valid    = (irRx.decode_type != UNKNOWN) && (irRx.bits > 0);
  rxInfo.protocol = String(typeToString(irRx.decode_type, irRx.repeat));
  rxInfo.valueHex = "0x" + String(uint64ToString(irRx.value, 16));
  rxInfo.bits     = irRx.bits;
  rxInfo.lastMs   = now;
  ++rxInfo.count;
  dirty.rx     = true;
  dirty.rxTime = true;

  Serial.print(F("[IR] RX  proto="));
  Serial.print(rxInfo.protocol);
  Serial.print(F("  value="));
  Serial.print(rxInfo.valueHex);
  Serial.print(F("  bits="));
  Serial.print(rxInfo.bits);
  if (isSelfEcho) Serial.print(F("  (loopback echo)"));
  Serial.println();

  // Loopback round-trip => purple. Real external signal => green.
  if (isSelfEcho) startLedFlash(80, 0, 80);   // purple
  else            startLedFlash(0, 80, 0);    // green

  irrecv.resume();
}

// --------------------------------------------------------
// Loopback
// --------------------------------------------------------
void pollLoopback() {
  if (!loopbackMode) return;
  static uint32_t lastLoopMs = 0;
  const uint32_t now = millis();
  if (now - lastLoopMs >= kLoopbackIntervalMs) {
    lastLoopMs = now;
    doSendCurrentPreset();
  }
}

void toggleLoopback() {
  loopbackMode = !loopbackMode;
  dirty.header = true;
  dirty.rx     = true;  // refresh OK badge area
  Serial.print(F("[IR] Loopback mode: "));
  Serial.println(loopbackMode ? F("ON") : F("OFF"));
}

// --------------------------------------------------------
// UI colours & layout
// --------------------------------------------------------
constexpr uint16_t kBg      = TFT_BLACK;
constexpr uint16_t kHeader  = 0x04FF;
constexpr uint16_t kPanelTx = 0x18E3;
constexpr uint16_t kPanelRx = 0x12CB;
constexpr uint16_t kLabel   = TFT_DARKGREY;
constexpr uint16_t kLoopBg  = 0x6200;

constexpr int16_t kHeaderH    = 22;
constexpr int16_t kBannerY    = 24;
constexpr int16_t kBannerH    = 26;
constexpr int16_t kBannerW    = 220;
constexpr int16_t kPanelTxY   = 53;
constexpr int16_t kPanelRxY   = 112;
constexpr int16_t kPanelH     = 56;

// --------------------------------------------------------
// Static chrome — drawn once unless dirty.chrome is set
// --------------------------------------------------------
void drawChrome() {
  const int16_t W = tft.width();
  tft.fillScreen(kBg);

  // panel frames (filled once; values overwrite their own backgrounds via padding)
  tft.fillRoundRect(6, kPanelTxY, W - 12, kPanelH, 6, kPanelTx);
  tft.drawRoundRect(6, kPanelTxY, W - 12, kPanelH, 6, TFT_DARKGREY);
  tft.fillRoundRect(6, kPanelRxY, W - 12, kPanelH, 6, kPanelRx);
  tft.drawRoundRect(6, kPanelRxY, W - 12, kPanelH, 6, TFT_DARKGREY);

  // static labels in panels
  tft.setTextDatum(TL_DATUM);
  tft.setTextPadding(0);

  tft.setTextColor(TFT_ORANGE, kPanelTx);
  tft.drawString("TX", 6 + 6, kPanelTxY + 4, 2);
  tft.setTextColor(kLabel, kPanelTx);
  tft.drawString("val", 6 + 6, kPanelTxY + 24, 1);

  tft.setTextColor(TFT_GREENYELLOW, kPanelRx);
  tft.drawString("RX", 6 + 6, kPanelRxY + 4, 2);
  tft.setTextColor(kLabel, kPanelRx);
  tft.drawString("val", 6 + 6, kPanelRxY + 24, 1);
}

// --------------------------------------------------------
// Header (title + LOOP badge)
// --------------------------------------------------------
void drawHeader() {
  const int16_t W = tft.width();
  tft.fillRect(0, 0, W, kHeaderH, kHeader);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, kHeader);
  tft.setTextPadding(0);
  tft.drawCentreString("IR TX / RX Test", W / 2, 4, 2);

  // LOOP badge area (always cleared with header bg above)
  if (loopbackMode) {
    tft.fillRoundRect(W - 70, 3, 66, 16, 4, kLoopBg);
    tft.setTextColor(TFT_YELLOW, kLoopBg);
    tft.drawCentreString("LOOP", W - 37, 6, 1);
  }
}

// --------------------------------------------------------
// Preset banner — only redrawn on selection change
// --------------------------------------------------------
void drawPresetBanner() {
  const int16_t W  = tft.width();
  const int16_t cx = W / 2;
  const int16_t x  = cx - kBannerW / 2;

  tft.fillRoundRect(x, kBannerY, kBannerW, kBannerH, 8, TFT_DARKCYAN);
  tft.drawRoundRect(x, kBannerY, kBannerW, kBannerH, 8, TFT_WHITE);

  char banner[48];
  snprintf(banner, sizeof(banner), "[%u/%u] %s  %s",
           (unsigned)(presetIndex + 1),
           (unsigned)kPresetCount,
           kPresets[presetIndex].name,
           typeToString(kPresets[presetIndex].protocol));

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_DARKCYAN);
  tft.setTextPadding(0);
  tft.drawCentreString(banner, cx, kBannerY + 5, 2);
}

// --------------------------------------------------------
// TX panel values (updated on TX event; uses padding to erase old text)
// --------------------------------------------------------
void drawTxValues() {
  const int16_t W  = tft.width();
  const int16_t pX = 6;
  const int16_t pW = W - 12;
  const int16_t pY = kPanelTxY;

  tft.setTextDatum(TL_DATUM);

  // name (after "TX " label)
  tft.setTextColor(TFT_WHITE, kPanelTx);
  tft.setTextPadding(70);
  tft.drawString(txInfo.valid ? txInfo.name.c_str() : "--", pX + 38, pY + 4, 2);

  // protocol (right side of row 1)
  tft.setTextColor(TFT_CYAN, kPanelTx);
  tft.setTextPadding(120);
  tft.drawString(txInfo.valid ? txInfo.protocol.c_str() : "", pX + 110, pY + 4, 2);

  // value
  tft.setTextColor(TFT_WHITE, kPanelTx);
  tft.setTextPadding(pW - 40);
  tft.drawString(txInfo.valueHex.c_str(), pX + 28, pY + 22, 2);

  // bits
  char buf[24];
  snprintf(buf, sizeof(buf), "bits:%u", (unsigned)txInfo.bits);
  tft.setTextColor(kLabel, kPanelTx);
  tft.setTextPadding(60);
  tft.drawString(buf, pX + 6, pY + 42, 1);

  // count
  snprintf(buf, sizeof(buf), "x%lu", (unsigned long)txInfo.count);
  tft.setTextColor(TFT_YELLOW, kPanelTx);
  tft.setTextPadding(60);
  tft.drawString(buf, pX + 68, pY + 42, 1);

  tft.setTextPadding(0);
}

void drawTxTime() {
  const int16_t W  = tft.width();
  const int16_t pX = 6;
  const int16_t pW = W - 12;
  const int16_t pY = kPanelTxY;

  String ts = fmtElapsed(txInfo.lastMs);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(kLabel, kPanelTx);
  tft.setTextPadding(70);
  tft.drawString(ts.c_str(), pX + pW - 4, pY + 42, 1);
  tft.setTextDatum(TL_DATUM);
  tft.setTextPadding(0);
}

// --------------------------------------------------------
// RX panel values
// --------------------------------------------------------
void drawRxValues() {
  const int16_t W  = tft.width();
  const int16_t pX = 6;
  const int16_t pW = W - 12;
  const int16_t pY = kPanelRxY;

  tft.setTextDatum(TL_DATUM);

  // protocol
  tft.setTextColor(TFT_WHITE, kPanelRx);
  tft.setTextPadding(160);
  tft.drawString(rxInfo.valid ? rxInfo.protocol.c_str() : "waiting...",
                 pX + 38, pY + 4, 2);

  // OK badge area: erase or draw
  const int16_t bx = pX + pW - 38;
  const int16_t by = pY + 2;
  if (loopbackMode && rxInfo.valid && txInfo.valid &&
      rxInfo.valueHex == txInfo.valueHex) {
    tft.fillRoundRect(bx, by, 34, 14, 3, TFT_DARKGREEN);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.setTextPadding(0);
    tft.drawCentreString("OK", bx + 17, by + 3, 1);
  } else {
    tft.fillRect(bx, by, 34, 14, kPanelRx);
  }

  // value
  tft.setTextColor(TFT_WHITE, kPanelRx);
  tft.setTextPadding(pW - 40);
  tft.drawString(rxInfo.valid ? rxInfo.valueHex.c_str() : "--",
                 pX + 28, pY + 22, 2);

  // bits
  char buf[24];
  snprintf(buf, sizeof(buf), "bits:%u", (unsigned)rxInfo.bits);
  tft.setTextColor(kLabel, kPanelRx);
  tft.setTextPadding(60);
  tft.drawString(buf, pX + 6, pY + 42, 1);

  // count
  snprintf(buf, sizeof(buf), "x%lu", (unsigned long)rxInfo.count);
  tft.setTextColor(TFT_YELLOW, kPanelRx);
  tft.setTextPadding(60);
  tft.drawString(buf, pX + 68, pY + 42, 1);

  tft.setTextPadding(0);
}

void drawRxTime() {
  const int16_t W  = tft.width();
  const int16_t pX = 6;
  const int16_t pW = W - 12;
  const int16_t pY = kPanelRxY;

  String ts = fmtElapsed(rxInfo.lastMs);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(kLabel, kPanelRx);
  tft.setTextPadding(70);
  tft.drawString(ts.c_str(), pX + pW - 4, pY + 42, 1);
  tft.setTextDatum(TL_DATUM);
  tft.setTextPadding(0);
}

// --------------------------------------------------------
// Render — only repaints regions whose dirty flag is set
// --------------------------------------------------------
void render() {
  if (dirty.chrome) { drawChrome();        dirty.chrome = false; }
  if (dirty.header) { drawHeader();        dirty.header = false; }
  if (dirty.preset) { drawPresetBanner();  dirty.preset = false; }
  if (dirty.tx)     { drawTxValues();      dirty.tx     = false; }
  if (dirty.rx)     { drawRxValues();      dirty.rx     = false; }
  if (dirty.txTime) { drawTxTime();        dirty.txTime = false; }
  if (dirty.rxTime) { drawRxTime();        dirty.rxTime = false; }
}

// --------------------------------------------------------
// Serial CLI
// --------------------------------------------------------
void printHelp() {
  Serial.println();
  Serial.println(F("IR send/receive test commands:"));
  Serial.println(F("  help         - show this help"));
  Serial.println(F("  status       - show current preset & counters"));
  Serial.println(F("  send         - transmit current preset"));
  Serial.println(F("  next / prev  - cycle preset"));
  Serial.println(F("  preset <n>   - select preset (1..N)"));
  Serial.println(F("  loopback     - toggle self-loopback mode"));
  Serial.println();
  Serial.println(F("Buttons:"));
  Serial.println(F("  Encoder rotate -> change preset"));
  Serial.println(F("  Encoder press  -> single send"));
  Serial.println(F("  BOOT key       -> toggle self-loopback mode"));
}

void printStatus() {
  const IrPreset& p = kPresets[presetIndex];
  Serial.println();
  Serial.print(F("[IR] TX pin:     GPIO")); Serial.println(kIrTxPin);
  Serial.print(F("[IR] RX pin:     GPIO")); Serial.println(kIrRxPin);
  Serial.print(F("[IR] Preset:     "));
  Serial.print(presetIndex + 1); Serial.print('/'); Serial.print(kPresetCount);
  Serial.print(F("  ")); Serial.println(p.name);
  Serial.print(F("[IR] Protocol:   ")); Serial.println(typeToString(p.protocol));
  Serial.print(F("[IR] TX count:   ")); Serial.println(txInfo.count);
  Serial.print(F("[IR] RX count:   ")); Serial.println(rxInfo.count);
  Serial.print(F("[IR] Loopback:   ")); Serial.println(loopbackMode ? F("ON") : F("OFF"));
}

void selectPreset(uint8_t idx) {
  if (idx >= kPresetCount) return;
  if (idx == presetIndex)  return;
  presetIndex = idx;
  dirty.preset = true;
  Serial.print(F("[IR] Preset -> "));
  Serial.println(kPresets[presetIndex].name);
}

void handleCommand(String line) {
  line.trim();
  if (line.isEmpty()) return;

  if (line.equalsIgnoreCase("help"))     { printHelp();    return; }
  if (line.equalsIgnoreCase("status"))   { printStatus();  return; }
  if (line.equalsIgnoreCase("send"))     { doSendCurrentPreset(); return; }
  if (line.equalsIgnoreCase("next"))     { selectPreset((presetIndex + 1) % kPresetCount); return; }
  if (line.equalsIgnoreCase("prev"))     { selectPreset((presetIndex + kPresetCount - 1) % kPresetCount); return; }
  if (line.equalsIgnoreCase("loopback")) { toggleLoopback(); return; }

  if (line.startsWith("preset ")) {
    long v = line.substring(7).toInt();
    if (v < 1 || v > kPresetCount) {
      Serial.print(F("[IR] preset must be 1.."));
      Serial.println(kPresetCount);
      return;
    }
    selectPreset(static_cast<uint8_t>(v - 1));
    return;
  }

  Serial.print(F("[IR] Unknown command: "));
  Serial.println(line);
  printHelp();
}

void pollSerialCommands() {
  if (!Serial.available()) return;
  handleCommand(Serial.readStringUntil('\n'));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println(F("T-Embed IR send/receive test"));

  pinMode(kUsrKey, INPUT_PULLUP);
  pinMode(kEncKey, INPUT_PULLUP);

  pinMode(kEncA, INPUT_PULLUP);
  pinMode(kEncB, INPUT_PULLUP);
  prevAB = ((digitalRead(kEncA) << 1) | digitalRead(kEncB));
  attachInterrupt(digitalPinToInterrupt(kEncA), onEncoderChange, CHANGE);
  attachInterrupt(digitalPinToInterrupt(kEncB), onEncoderChange, CHANGE);

  leds.begin();
  leds.setBrightness(60);
  leds.clear();
  leds.show();

  if (!initDisplay()) {
    Serial.println(F("[IR] Board init failed. Halting."));
    while (true) { delay(1000); }
  }

  tft.init();
  tft.setRotation(kRotation);
  tft.fillScreen(kBg);

  irsend.begin();
  irrecv.enableIRIn();

  render();
  printStatus();
  printHelp();
}

void loop() {
  pollSerialCommands();
  pollIrReceive();
  pollLoopback();
  pollLedFlash();
  pollButton(usrBtn);
  pollButton(encBtn);

  // BOOT (USR) key: toggle loopback
  if (usrBtn.pressedEvent) {
    usrBtn.pressedEvent = false;
    toggleLoopback();
  }

  // Encoder press: single send (manual mode only — avoid stomping loopback timer)
  if (encBtn.pressedEvent) {
    encBtn.pressedEvent = false;
    if (!loopbackMode) doSendCurrentPreset();
  }

  // Encoder rotation -> change preset
  static int32_t lastEnc = 0;
  const int32_t cur   = encoderCount;
  const int32_t delta = (cur - lastEnc) / 2;
  if (delta != 0) {
    lastEnc += delta * 2;
    int32_t idx = static_cast<int32_t>(presetIndex) + delta;
    idx %= kPresetCount;
    if (idx < 0) idx += kPresetCount;
    selectPreset(static_cast<uint8_t>(idx));
  }

  // Refresh only the timestamps every second (cheap, no flicker on the rest)
  static uint32_t lastTickMs = 0;
  if (millis() - lastTickMs >= 1000) {
    lastTickMs   = millis();
    dirty.txTime = true;
    dirty.rxTime = true;
  }

  render();
  delay(2);
}
