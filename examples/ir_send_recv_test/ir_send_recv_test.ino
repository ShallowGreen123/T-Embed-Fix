#include <Arduino.h>
#include <TFT_eSPI.h>

#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>

#include <TEmbedBoard.h>

namespace {

// ---- pins ----
constexpr uint8_t kIrTxPin = BOARD_IR_TX;     // GPIO15
constexpr uint8_t kIrRxPin = BOARD_IR_RX;     // GPIO1
constexpr uint8_t kUsrKey  = BOARD_USER_KEY;  // GPIO6
constexpr uint8_t kEncA    = ENCODER_INA;
constexpr uint8_t kEncB    = ENCODER_INB;
constexpr uint8_t kEncKey  = ENCODER_KEY;

// ---- display ----
constexpr uint8_t kRotation = 1;

// ---- IR ----
constexpr uint16_t kIrCaptureBufSize = 1024;
constexpr uint8_t  kIrTimeoutMs      = 50;
constexpr uint32_t kDebounceMs       = 20;

// ---- preset codes ----
struct IrPreset {
  const char*    name;
  decode_type_t  protocol;
  uint64_t       value;
  uint16_t       bits;
};

const IrPreset kPresets[] = {
  {"NEC  A",  NEC,     0x20DF10EFULL, 32},
  {"NEC  B",  NEC,     0x20DF40BFULL, 32},
  {"Sony12",  SONY,    0x00000A90ULL, 12},
  {"Samsung", SAMSUNG, 0xE0E040BFULL, 32},
  {"RC5",     RC5,     0x00000010ULL, 12},
};
constexpr uint8_t kPresetCount = sizeof(kPresets) / sizeof(kPresets[0]);

TEmbedXL9555  ioExpander;
TFT_eSPI      tft;
IRsend        irsend(kIrTxPin);
IRrecv        irrecv(kIrRxPin, kIrCaptureBufSize, kIrTimeoutMs, true);
decode_results irRx;

uint8_t presetIndex = 0;

struct RxInfo {
  bool      valid       = false;
  String    protocol    = "--";
  String    valueHex    = "--";
  uint16_t  bits        = 0;
  uint32_t  count       = 0;
};

struct TxInfo {
  bool      valid       = false;
  String    name        = "--";
  String    valueHex    = "--";
  uint16_t  bits        = 0;
  uint32_t  count       = 0;
};

RxInfo rxInfo;
TxInfo txInfo;

// ---- button debounce ----
struct ButtonState {
  uint8_t  pin;
  bool     pressed;
  bool     pressedEvent;
  uint32_t lastChangeMs;
};

ButtonState usrBtn = {kUsrKey, false, false, 0};
ButtonState encBtn = {kEncKey, false, false, 0};

bool needsRedraw = true;

// ---- encoder state ----
volatile int32_t encoderCount = 0;
volatile uint8_t prevAB = 0;
static const int8_t kEncTable[4][4] = {
  { 0, -1,  1,  0},
  { 1,  0,  0, -1},
  {-1,  0,  0,  1},
  { 0,  1, -1,  0},
};

void IRAM_ATTR onEncoderChange() {
  const uint8_t a = digitalRead(kEncA);
  const uint8_t b = digitalRead(kEncB);
  const uint8_t cur = (a << 1) | b;
  encoderCount += kEncTable[prevAB][cur];
  prevAB = cur;
}

void pollButton(ButtonState& btn) {
  const bool raw = (digitalRead(btn.pin) == LOW);
  const uint32_t now = millis();
  if (raw != btn.pressed && (now - btn.lastChangeMs) >= kDebounceMs) {
    btn.lastChangeMs = now;
    btn.pressed = raw;
    if (raw) btn.pressedEvent = true;
    needsRedraw = true;
  }
}

// --------------------------------------------------------
// Display init (matches encoder_key_test pattern)
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
// IR send dispatch — picks the right IRsend method per protocol
// --------------------------------------------------------
bool sendPreset(const IrPreset& preset) {
  switch (preset.protocol) {
    case NEC:
      irsend.sendNEC(preset.value, preset.bits);
      return true;
    case SONY:
      irsend.sendSony(preset.value, preset.bits, 2);
      return true;
    case SAMSUNG:
      irsend.sendSAMSUNG(preset.value, preset.bits);
      return true;
    case RC5:
      irsend.sendRC5(preset.value, preset.bits);
      return true;
    default:
      return false;
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
  txInfo.valueHex = "0x" + String(uint64ToString(p.value, 16));
  txInfo.bits     = p.bits;
  ++txInfo.count;
  needsRedraw = true;

  // After TX, the receiver may have caught the echo — clear and resume RX.
  irrecv.resume();
}

// --------------------------------------------------------
// IR receive
// --------------------------------------------------------
void pollIrReceive() {
  if (!irrecv.decode(&irRx)) return;

  rxInfo.valid    = (irRx.decode_type != UNKNOWN) && (irRx.bits > 0);
  rxInfo.protocol = String(typeToString(irRx.decode_type, irRx.repeat));
  rxInfo.valueHex = "0x" + String(uint64ToString(irRx.value, 16));
  rxInfo.bits     = irRx.bits;
  ++rxInfo.count;
  needsRedraw = true;

  Serial.print(F("[IR] RX  proto="));
  Serial.print(rxInfo.protocol);
  Serial.print(F("  value="));
  Serial.print(rxInfo.valueHex);
  Serial.print(F("  bits="));
  Serial.println(rxInfo.bits);

  irrecv.resume();
}

// --------------------------------------------------------
// UI
// --------------------------------------------------------
constexpr uint16_t kBg     = TFT_BLACK;
constexpr uint16_t kHeader = 0x04FF;   // dark cyan
constexpr uint16_t kPanelTx = 0x18E3;  // dim warm
constexpr uint16_t kPanelRx = 0x12CB;  // dim cool
constexpr uint16_t kLabel   = TFT_DARKGREY;

// Pill banner for the active preset selection
void drawPresetBanner(int16_t cx, int16_t y, const char* label) {
  const int16_t w = 200;
  const int16_t h = 26;
  tft.fillRoundRect(cx - w / 2, y, w, h, 8, TFT_DARKCYAN);
  tft.drawRoundRect(cx - w / 2, y, w, h, 8, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_DARKCYAN);
  tft.drawCentreString(label, cx, y + 5, 2);
}

// Layout (rotation=1 → 320 x 170):
//   0  - 22   header
//  24  - 56   preset banner   (turn encoder to switch, USR-key to send)
//  58  - 110  TX panel
// 112  - 164  RX panel
void redraw() {
  const int16_t W  = tft.width();
  const int16_t H  = tft.height();
  const int16_t MX = W / 2;

  // ---- header ----
  tft.fillRect(0, 0, W, 22, kHeader);
  tft.setTextColor(TFT_WHITE, kHeader);
  tft.drawCentreString("IR TX / RX Test", MX, 4, 2);

  // ---- body bg ----
  tft.fillRect(0, 22, W, H - 22, kBg);

  // ---- preset banner ----
  char banner[40];
  snprintf(banner, sizeof(banner), "Preset %u/%u: %s",
           (unsigned)(presetIndex + 1), (unsigned)kPresetCount,
           kPresets[presetIndex].name);
  drawPresetBanner(MX, 26, banner);

  // ---- TX panel ----
  const int16_t txY = 58;
  const int16_t txH = 52;
  tft.fillRoundRect(6, txY, W - 12, txH, 8, kPanelTx);
  tft.drawRoundRect(6, txY, W - 12, txH, 8, TFT_DARKGREY);

  tft.setTextColor(TFT_ORANGE, kPanelTx);
  tft.drawString("TX", 14, txY + 6, 2);
  tft.setTextColor(TFT_WHITE, kPanelTx);
  tft.drawString(txInfo.valid ? txInfo.name.c_str() : "--", 50, txY + 6, 2);

  char buf[40];
  tft.setTextColor(kLabel, kPanelTx);
  tft.drawString("value", 14, txY + 26, 1);
  tft.setTextColor(TFT_WHITE, kPanelTx);
  tft.drawString(txInfo.valueHex.c_str(), 50, txY + 24, 2);

  snprintf(buf, sizeof(buf), "bits %u  x%lu",
           (unsigned)txInfo.bits, (unsigned long)txInfo.count);
  tft.setTextColor(kLabel, kPanelTx);
  tft.drawRightString(buf, W - 14, txY + 30, 1);

  // ---- RX panel ----
  const int16_t rxY = 112;
  const int16_t rxH = 52;
  tft.fillRoundRect(6, rxY, W - 12, rxH, 8, kPanelRx);
  tft.drawRoundRect(6, rxY, W - 12, rxH, 8, TFT_DARKGREY);

  tft.setTextColor(TFT_GREENYELLOW, kPanelRx);
  tft.drawString("RX", 14, rxY + 6, 2);
  tft.setTextColor(TFT_WHITE, kPanelRx);
  tft.drawString(rxInfo.valid ? rxInfo.protocol.c_str() : "waiting...",
                 50, rxY + 6, 2);

  tft.setTextColor(kLabel, kPanelRx);
  tft.drawString("value", 14, rxY + 26, 1);
  tft.setTextColor(TFT_WHITE, kPanelRx);
  tft.drawString(rxInfo.valid ? rxInfo.valueHex.c_str() : "--",
                 50, rxY + 24, 2);

  snprintf(buf, sizeof(buf), "bits %u  x%lu",
           (unsigned)rxInfo.bits, (unsigned long)rxInfo.count);
  tft.setTextColor(kLabel, kPanelRx);
  tft.drawRightString(buf, W - 14, rxY + 30, 1);
}

// --------------------------------------------------------
// Serial CLI (mirrors other examples)
// --------------------------------------------------------
void printHelp() {
  Serial.println();
  Serial.println(F("IR send/receive test commands:"));
  Serial.println(F("  help         - show this help"));
  Serial.println(F("  status       - show current preset & counters"));
  Serial.println(F("  send         - transmit current preset"));
  Serial.println(F("  next / prev  - cycle preset"));
  Serial.println(F("  preset <n>   - select preset (1..N)"));
  Serial.println();
}

void printStatus() {
  const IrPreset& p = kPresets[presetIndex];
  Serial.println();
  Serial.print(F("[IR] TX pin:  GPIO")); Serial.println(kIrTxPin);
  Serial.print(F("[IR] RX pin:  GPIO")); Serial.println(kIrRxPin);
  Serial.print(F("[IR] Preset:  "));
  Serial.print(presetIndex + 1); Serial.print('/'); Serial.print(kPresetCount);
  Serial.print(F("  ")); Serial.println(p.name);
  Serial.print(F("[IR] TX cnt:  ")); Serial.println(txInfo.count);
  Serial.print(F("[IR] RX cnt:  ")); Serial.println(rxInfo.count);
}

void selectPreset(uint8_t idx) {
  if (idx >= kPresetCount) return;
  presetIndex = idx;
  needsRedraw = true;
  Serial.print(F("[IR] Preset -> "));
  Serial.println(kPresets[presetIndex].name);
}

void handleCommand(String line) {
  line.trim();
  if (line.isEmpty()) return;

  if (line.equalsIgnoreCase("help"))   { printHelp();   return; }
  if (line.equalsIgnoreCase("status")) { printStatus(); return; }
  if (line.equalsIgnoreCase("send"))   { doSendCurrentPreset(); return; }
  if (line.equalsIgnoreCase("next"))   { selectPreset((presetIndex + 1) % kPresetCount); return; }
  if (line.equalsIgnoreCase("prev"))   { selectPreset((presetIndex + kPresetCount - 1) % kPresetCount); return; }

  if (line.startsWith("preset ")) {
    long v = line.substring(7).toInt();
    if (v < 1 || v > kPresetCount) {
      Serial.print(F("[IR] preset must be 1..")); Serial.println(kPresetCount);
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

  // Buttons
  pinMode(kUsrKey, INPUT_PULLUP);
  pinMode(kEncKey, INPUT_PULLUP);

  // Encoder
  pinMode(kEncA, INPUT_PULLUP);
  pinMode(kEncB, INPUT_PULLUP);
  prevAB = ((digitalRead(kEncA) << 1) | digitalRead(kEncB));
  attachInterrupt(digitalPinToInterrupt(kEncA), onEncoderChange, CHANGE);
  attachInterrupt(digitalPinToInterrupt(kEncB), onEncoderChange, CHANGE);

  if (!initDisplay()) {
    Serial.println(F("[IR] Board init failed. Halting."));
    while (true) { delay(1000); }
  }

  tft.init();
  tft.setRotation(kRotation);
  tft.fillScreen(kBg);

  // IR
  irsend.begin();
  irrecv.enableIRIn();

  redraw();
  printStatus();
  printHelp();
}

void loop() {
  pollSerialCommands();
  pollIrReceive();
  pollButton(usrBtn);
  pollButton(encBtn);

  // USR key: send current preset
  if (usrBtn.pressedEvent) {
    usrBtn.pressedEvent = false;
    doSendCurrentPreset();
  }
  // Encoder push: also send (handy)
  if (encBtn.pressedEvent) {
    encBtn.pressedEvent = false;
    doSendCurrentPreset();
  }

  // Encoder rotation -> change preset (4 detents per click on this encoder
  // so divide by 2 to feel right; tweak if it scrolls too fast/slow)
  static int32_t lastEnc = 0;
  const int32_t cur = encoderCount;
  const int32_t delta = (cur - lastEnc) / 2;
  if (delta != 0) {
    lastEnc += delta * 2;
    int32_t idx = static_cast<int32_t>(presetIndex) + delta;
    idx %= kPresetCount;
    if (idx < 0) idx += kPresetCount;
    selectPreset(static_cast<uint8_t>(idx));
  }

  if (needsRedraw) {
    needsRedraw = false;
    redraw();
  }

  delay(2);
}
