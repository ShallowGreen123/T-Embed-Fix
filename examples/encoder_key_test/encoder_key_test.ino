#include <Arduino.h>
#include <TFT_eSPI.h>

#include <TEmbedBoard.h>

namespace {

// ---- display ----
constexpr uint8_t kRotation = 1;

// ---- encoder ----
// INA/INB are the quadrature outputs; KEY is the encoder push-button
constexpr uint8_t kEncA   = ENCODER_INA;   // GPIO4
constexpr uint8_t kEncB   = ENCODER_INB;   // GPIO5
constexpr uint8_t kEncKey = ENCODER_KEY;   // GPIO0  (boot pin, active-low)
constexpr uint8_t kUsrKey = BOARD_USER_KEY; // GPIO6  (active-low)

// ---- debounce ----
constexpr uint32_t kDebounceMs = 20;

TEmbedXL9555 ioExpander;
TFT_eSPI tft;

// ---- encoder state ----
volatile int32_t encoderCount = 0;
volatile uint8_t prevAB = 0;

// Full-step gray-code table: [prevAB][curAB] -> delta
static const int8_t kEncTable[4][4] = {
  { 0, -1,  1,  0},
  { 1,  0,  0, -1},
  {-1,  0,  0,  1},
  { 0,  1, -1,  0},
};

// ---- button state ----
struct ButtonState {
  uint8_t pin;
  bool    pressed;        // current debounced state
  bool    pressedEvent;   // edge flag — consumed in loop
  uint32_t lastChangeMs;
};

ButtonState encBtn  = {kEncKey, false, false, 0};
ButtonState usrBtn  = {kUsrKey, false, false, 0};

// ---- display dirty flag ----
bool needsRedraw = true;

// --------------------------------------------------------
// Encoder ISR (called on any edge of INA or INB)
// --------------------------------------------------------
void IRAM_ATTR onEncoderChange() {
  const uint8_t a = digitalRead(kEncA);
  const uint8_t b = digitalRead(kEncB);
  const uint8_t cur = (a << 1) | b;
  encoderCount += kEncTable[prevAB][cur];
  prevAB = cur;
}

// --------------------------------------------------------
// Button debounce (polled)
// --------------------------------------------------------
void pollButton(ButtonState& btn) {
  const bool raw = (digitalRead(btn.pin) == LOW);
  const uint32_t now = millis();
  if (raw != btn.pressed && (now - btn.lastChangeMs) >= kDebounceMs) {
    btn.lastChangeMs = now;
    btn.pressed = raw;
    if (raw) {
      btn.pressedEvent = true;
    }
    needsRedraw = true;
  }
}

// --------------------------------------------------------
// Display initialisation
// --------------------------------------------------------
bool initDisplay() {
  t_embed::board::deselectSharedSpiDevices();

  if (!t_embed::board::beginExpander(ioExpander)) {
    Serial.println(F("[ENC] XL9555 init failed."));
    return false;
  }
  if (!t_embed::board::setLowPowerEnabled(ioExpander, true)) {
    Serial.println(F("[ENC] LOW_PWR_3V3 enable failed."));
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
// UI helpers
// --------------------------------------------------------
constexpr uint16_t kBg         = TFT_BLACK;
constexpr uint16_t kHeader     = 0x04FF;   // dark cyan
constexpr uint16_t kLabelColor = TFT_DARKGREY;
constexpr uint16_t kActiveHigh = TFT_GREEN;
constexpr uint16_t kActiveLow  = 0x2104;   // dim grey

// Pill button: width=120, height=32, centred at (cx, cy)
void drawButtonPill(int16_t cx, int16_t cy, bool pressed, const char* label) {
  const uint16_t bg = pressed ? kActiveHigh : kActiveLow;
  const uint16_t fg = pressed ? TFT_BLACK   : TFT_WHITE;
  tft.fillRoundRect(cx - 60, cy - 16, 120, 32, 10, bg);
  tft.drawRoundRect(cx - 60, cy - 16, 120, 32, 10,
                    pressed ? TFT_WHITE : TFT_DARKGREY);
  tft.setTextColor(fg, bg);
  tft.drawCentreString(label, cx, cy - 7, 2);
}

// --------------------------------------------------------
// Full screen redraw  (rotation=1 → 320 × 170)
//
//  Layout (Y coords):
//   0-22   header bar
//  24-169  body split left/right at W/2
//  Left  (encoder):  label, big count, pill, press count
//  Right (user key): label, pill, press count
// --------------------------------------------------------
void redraw(int32_t count, bool encPressed, uint32_t encPressCount,
            bool usrPressed, uint32_t usrPressCount) {
  const int16_t W  = tft.width();   // 320
  const int16_t H  = tft.height();  // 170
  const int16_t MX = W / 2;         // 160  vertical divider

  // ---- header ----
  tft.fillRect(0, 0, W, 22, kHeader);
  tft.setTextColor(TFT_WHITE, kHeader);
  tft.drawCentreString("Encoder & Key Test", MX, 4, 2);

  // ---- body background ----
  tft.fillRect(0, 22, W, H - 22, kBg);

  // ---- vertical divider ----
  tft.drawFastVLine(MX, 26, H - 30, TFT_DARKGREY);

  // ======== LEFT: encoder ========
  const int16_t LX = MX / 2;   // centre of left half = 80

  // Section label
  tft.setTextColor(TFT_CYAN, kBg);
  tft.drawCentreString("ENCODER", LX, 26, 2);

  // Big count
  tft.setTextColor(TFT_YELLOW, kBg);
  tft.drawCentreString(String(count), LX, 50, 4);

  // Encoder button pill  (y=110)
  drawButtonPill(LX, 110, encPressed,
                 encPressed ? "PRESSED" : "idle");

  // Press count
  char buf[24];
  snprintf(buf, sizeof(buf), "x%lu", (unsigned long)encPressCount);
  tft.setTextColor(kLabelColor, kBg);
  tft.drawCentreString(buf, LX, 146, 2);

  // ======== RIGHT: user key ========
  const int16_t RX = MX + MX / 2;   // 240

  // Section label
  tft.setTextColor(TFT_CYAN, kBg);
  tft.drawCentreString("USER KEY", RX, 26, 2);
  tft.setTextColor(TFT_DARKGREY, kBg);
  tft.drawCentreString("IO6", RX, 46, 2);

  // User key pill (y=100)
  drawButtonPill(RX, 100, usrPressed,
                 usrPressed ? "PRESSED" : "idle");

  // Press count
  snprintf(buf, sizeof(buf), "x%lu", (unsigned long)usrPressCount);
  tft.setTextColor(kLabelColor, kBg);
  tft.drawCentreString(buf, RX, 132, 2);
}

}  // namespace

// --------------------------------------------------------
// Globals for press counters (persistent across redraws)
// --------------------------------------------------------
static uint32_t gEncPressCount = 0;
static uint32_t gUsrPressCount = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("T-Embed encoder & key test"));

  // Buttons: internal pull-up, active-low
  pinMode(kEncKey, INPUT_PULLUP);
  pinMode(kUsrKey, INPUT_PULLUP);

  // Encoder pins
  pinMode(kEncA, INPUT_PULLUP);
  pinMode(kEncB, INPUT_PULLUP);
  prevAB = ((digitalRead(kEncA) << 1) | digitalRead(kEncB));
  attachInterrupt(digitalPinToInterrupt(kEncA), onEncoderChange, CHANGE);
  attachInterrupt(digitalPinToInterrupt(kEncB), onEncoderChange, CHANGE);

  if (!initDisplay()) {
    Serial.println(F("[ENC] Board init failed. Halting."));
    while (true) { delay(1000); }
  }

  tft.init();
  tft.setRotation(kRotation);
  tft.fillScreen(kBg);

  redraw(encoderCount, false, 0, false, 0);
}

void loop() {
  pollButton(encBtn);
  pollButton(usrBtn);

  if (encBtn.pressedEvent) {
    encBtn.pressedEvent = false;
    ++gEncPressCount;
    Serial.print(F("[ENC] Encoder press #"));
    Serial.println(gEncPressCount);
  }
  if (usrBtn.pressedEvent) {
    usrBtn.pressedEvent = false;
    ++gUsrPressCount;
    Serial.print(F("[ENC] User key press #"));
    Serial.println(gUsrPressCount);
  }

  // Check encoder count changed
  static int32_t lastCount = 0;
  const int32_t cur = encoderCount;
  if (cur != lastCount) {
    lastCount = cur;
    needsRedraw = true;
    Serial.print(F("[ENC] Count: "));
    Serial.println(cur);
  }

  if (needsRedraw) {
    needsRedraw = false;
    redraw(cur, encBtn.pressed, gEncPressCount, usrBtn.pressed, gUsrPressCount);
  }

  delay(5);
}
