#include <Arduino.h>
#include <TFT_eSPI.h>

#include <TEmbedBoard.h>

namespace {

constexpr uint32_t kAutoAdvanceMs = 3000;
constexpr uint32_t kAnimationFrameMs = 20;
constexpr uint8_t kDefaultRotation = 1;

TEmbedXL9555 ioExpander;
TFT_eSPI tft;

enum class DemoPage : uint8_t {
  Summary = 0,
  ColorBars,
  Geometry,
  Text,
  Animation,
  Count,
};

struct BallState {
  int16_t x;
  int16_t y;
  int16_t vx;
  int16_t vy;
  int16_t radius;
};

bool autoAdvance = true;
uint8_t currentRotation = kDefaultRotation;
DemoPage currentPage = DemoPage::Summary;
unsigned long pageChangedAtMs = 0;
unsigned long lastAnimationAtMs = 0;
BallState ball = {};

uint8_t pageCount() {
  return static_cast<uint8_t>(DemoPage::Count);
}

DemoPage pageFromIndex(int index) {
  const int count = pageCount();
  int wrapped = index % count;
  if (wrapped < 0) {
    wrapped += count;
  }
  return static_cast<DemoPage>(wrapped);
}

DemoPage nextPage(DemoPage page, int delta) {
  return pageFromIndex(static_cast<int>(page) + delta);
}

const char* pageName(DemoPage page) {
  switch (page) {
    case DemoPage::Summary:   return "summary";
    case DemoPage::ColorBars: return "color-bars";
    case DemoPage::Geometry:  return "geometry";
    case DemoPage::Text:      return "text";
    case DemoPage::Animation: return "animation";
    case DemoPage::Count:     break;
  }
  return "?";
}

bool initBoardForDisplay() {
  t_embed::board::deselectSharedSpiDevices();

  if (!t_embed::board::beginExpander(ioExpander)) {
    Serial.println(F("[TFT] XL9555 init failed."));
    return false;
  }

  if (!t_embed::board::setLowPowerEnabled(ioExpander, true)) {
    Serial.println(F("[TFT] Failed to enable LOW_PWR_3V3."));
    return false;
  }

  pinMode(BOARD_LCD_BL, OUTPUT);
  digitalWrite(BOARD_LCD_BL, HIGH);

  if (!t_embed::board::setLcdReset(ioExpander, true)) {
    Serial.println(F("[TFT] Failed to drive LCD reset high."));
    return false;
  }

  delay(5);

  if (!t_embed::board::setLcdReset(ioExpander, false)) {
    Serial.println(F("[TFT] Failed to drive LCD reset low."));
    return false;
  }

  delay(20);

  if (!t_embed::board::setLcdReset(ioExpander, true)) {
    Serial.println(F("[TFT] Failed to release LCD reset."));
    return false;
  }

  delay(120);
  return true;
}

void printHelp() {
  Serial.println();
  Serial.println(F("TFT display test commands:"));
  Serial.println(F("  help        - show this help"));
  Serial.println(F("  status      - show current page/rotation"));
  Serial.println(F("  next        - show next test page"));
  Serial.println(F("  prev        - show previous test page"));
  Serial.println(F("  page <0-4>  - switch to a specific page"));
  Serial.println(F("  rotate <0-3>- change TFT rotation"));
  Serial.println(F("  auto        - enable auto page switching"));
  Serial.println(F("  hold        - stay on the current page"));
  Serial.println();
}

void printStatus() {
  Serial.println();
  Serial.print(F("[TFT] Page:       "));
  Serial.println(pageName(currentPage));
  Serial.print(F("[TFT] Rotation:   "));
  Serial.println(currentRotation);
  Serial.print(F("[TFT] Resolution: "));
  Serial.print(tft.width());
  Serial.print('x');
  Serial.println(tft.height());
  Serial.print(F("[TFT] Auto page:  "));
  Serial.println(autoAdvance ? F("on") : F("off"));
}

void drawHeader(const char* title, uint16_t color) {
  tft.fillRect(0, 0, tft.width(), 28, color);
  tft.setTextColor(TFT_BLACK, color);
  const uint8_t font = (tft.width() < 220) ? 1 : 2;
  tft.setTextFont(font);
  tft.drawString(title, 8, (font == 1) ? 9 : 6, font);
}

void drawFooter(const char* text) {
  const int16_t footerY = tft.height() - 18;
  tft.fillRect(0, footerY, tft.width(), 18, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString(text, 6, footerY + 3, 1);
}

void drawSummaryPage() {
  tft.fillScreen(TFT_BLACK);
  drawHeader("T-Embed TFT Test", TFT_CYAN);

  const int16_t footerY = tft.height() - 18;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Board: T-Embed PN532", 10, 40, 2);
  tft.drawString("Panel: ST7789 170x320", 10, 58, 2);
  tft.drawString("Reset: XL9555", 10, 76, 2);
  tft.drawString("Rot: " + String(currentRotation) + "  Auto: " + String(autoAdvance ? "on" : "off"), 10, 94, 2);

  const uint16_t swatchColors[] = {TFT_RED, TFT_ORANGE, TFT_YELLOW, TFT_GREEN, TFT_CYAN, TFT_BLUE};
  int16_t swatchY = footerY - 42;
  if (swatchY > 114) {
    swatchY = 114;
  }
  if (swatchY < 106) {
    swatchY = 106;
  }
  const int16_t swatchW = (tft.width() - 26) / 6;
  for (uint8_t i = 0; i < 6; ++i) {
    tft.fillRoundRect(10 + i * swatchW, swatchY, swatchW - 4, 20, 6, swatchColors[i]);
  }

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("help | next | rotate 1", 10, swatchY + 24, 1);

  drawFooter("Page 0  summary");
}

void drawColorBarsPage() {
  static const uint16_t colors[] = {
      TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW, TFT_CYAN, TFT_MAGENTA, TFT_WHITE};
  static const char* labels[] = {
      "RED", "GREEN", "BLUE", "YELLOW", "CYAN", "MAGENTA", "WHITE"};

  tft.fillScreen(TFT_BLACK);
  drawHeader("Color Bars", TFT_GREEN);

  const int16_t top = 34;
  const int16_t barHeight = (tft.height() - top - 20) / 7;
  for (uint8_t i = 0; i < 7; ++i) {
    const int16_t y = top + i * barHeight;
    tft.fillRect(0, y, tft.width(), barHeight, colors[i]);
    tft.setTextColor((colors[i] == TFT_WHITE || colors[i] == TFT_YELLOW) ? TFT_BLACK : TFT_WHITE, colors[i]);
    tft.drawString(labels[i], 10, y + 4, 2);
  }

  drawFooter("Page 1  colors");
}

void drawGeometryPage() {
  tft.fillScreen(TFT_NAVY);
  drawHeader("Geometry", TFT_YELLOW);

  const int16_t top = 34;
  for (int16_t x = 0; x < tft.width(); x += 20) {
    tft.drawFastVLine(x, top, tft.height() - top - 18, TFT_DARKGREY);
  }
  for (int16_t y = top; y < tft.height() - 18; y += 20) {
    tft.drawFastHLine(0, y, tft.width(), TFT_DARKGREY);
  }

  tft.drawRect(8, top + 8, tft.width() - 16, tft.height() - top - 34, TFT_WHITE);
  tft.drawLine(8, top + 8, tft.width() - 9, tft.height() - 27, TFT_RED);
  tft.drawLine(tft.width() - 9, top + 8, 8, tft.height() - 27, TFT_CYAN);
  tft.drawCircle(tft.width() / 2, top + 42, 28, TFT_GREEN);
  tft.fillCircle(tft.width() / 2, top + 42, 8, TFT_GREEN);
  tft.drawRoundRect(18, tft.height() - 72, 88, 34, 8, TFT_ORANGE);
  tft.fillRoundRect(tft.width() - 110, tft.height() - 72, 92, 34, 10, TFT_MAGENTA);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("grid / lines / circles", 16, tft.height() - 98, 2);

  drawFooter("Page 2  primitives");
}

void drawTextPage() {
  tft.fillScreen(TFT_BLACK);
  drawHeader("Text Rendering", TFT_MAGENTA);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Font 2: quick status text", 10, 40, 2);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("T-Embed", 10, 60, 4);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("170x320 ST7789", 10, 94, 2);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("RGB565 palette check", 10, 112, 2);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("0123456789  +-.", 10, 130, 2);

  drawFooter("Page 3  text/colors");
}

void drawAnimationFrame() {
  tft.fillScreen(TFT_BLACK);
  drawHeader("Animation", TFT_ORANGE);
  tft.drawRoundRect(12, 40, tft.width() - 24, tft.height() - 64, 12, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Ball checks refresh/fill.", 18, tft.height() - 42, 1);
  drawFooter("Page 4  animation");
}

void resetBall() {
  ball.radius = 10;
  ball.x = 40;
  ball.y = 64;
  ball.vx = 3;
  ball.vy = 2;
}

void drawBall(uint16_t color) {
  tft.fillCircle(ball.x, ball.y, ball.radius, color);
}

void renderPage(DemoPage page) {
  switch (page) {
    case DemoPage::Summary:   drawSummaryPage(); break;
    case DemoPage::ColorBars: drawColorBarsPage(); break;
    case DemoPage::Geometry:  drawGeometryPage(); break;
    case DemoPage::Text:      drawTextPage(); break;
    case DemoPage::Animation:
      drawAnimationFrame();
      resetBall();
      drawBall(TFT_YELLOW);
      break;
    case DemoPage::Count:
      break;
  }
}

void showPage(DemoPage page) {
  currentPage = page;
  pageChangedAtMs = millis();
  lastAnimationAtMs = 0;
  renderPage(page);
  Serial.print(F("[TFT] Page -> "));
  Serial.println(pageName(currentPage));
}

void setRotation(uint8_t rotation) {
  currentRotation = rotation & 0x03;
  tft.setRotation(currentRotation);
  showPage(currentPage);
}

void animateBall() {
  if (currentPage != DemoPage::Animation) {
    return;
  }

  const unsigned long now = millis();
  if ((lastAnimationAtMs != 0U) && (now - lastAnimationAtMs < kAnimationFrameMs)) {
    return;
  }
  lastAnimationAtMs = now;

  const int16_t left = 24 + ball.radius;
  const int16_t right = tft.width() - 24 - ball.radius;
  const int16_t top = 52 + ball.radius;
  const int16_t bottom = tft.height() - 34 - ball.radius;

  drawBall(TFT_BLACK);

  ball.x += ball.vx;
  ball.y += ball.vy;

  if (ball.x <= left || ball.x >= right) {
    ball.vx = -ball.vx;
    ball.x += ball.vx;
  }
  if (ball.y <= top || ball.y >= bottom) {
    ball.vy = -ball.vy;
    ball.y += ball.vy;
  }

  drawBall(TFT_YELLOW);
}

void maybeAdvancePage() {
  if (!autoAdvance) {
    return;
  }
  if (millis() - pageChangedAtMs < kAutoAdvanceMs) {
    return;
  }
  showPage(nextPage(currentPage, 1));
}

void handleCommand(String line) {
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
  if (line.equalsIgnoreCase("next")) {
    showPage(nextPage(currentPage, 1));
    return;
  }
  if (line.equalsIgnoreCase("prev")) {
    showPage(nextPage(currentPage, -1));
    return;
  }
  if (line.equalsIgnoreCase("auto")) {
    autoAdvance = true;
    pageChangedAtMs = millis();
    Serial.println(F("[TFT] Auto page switching enabled."));
    return;
  }
  if (line.equalsIgnoreCase("hold")) {
    autoAdvance = false;
    Serial.println(F("[TFT] Auto page switching disabled."));
    return;
  }

  if (line.startsWith("page ")) {
    const long value = line.substring(5).toInt();
    if (value < 0 || value >= pageCount()) {
      Serial.println(F("[TFT] Page must be between 0 and 4."));
      return;
    }
    showPage(pageFromIndex(static_cast<int>(value)));
    return;
  }

  if (line.startsWith("rotate ")) {
    const long value = line.substring(7).toInt();
    if (value < 0 || value > 3) {
      Serial.println(F("[TFT] Rotation must be 0, 1, 2 or 3."));
      return;
    }
    setRotation(static_cast<uint8_t>(value));
    return;
  }

  Serial.print(F("[TFT] Unknown command: "));
  Serial.println(line);
  printHelp();
}

void pollSerialCommands() {
  if (!Serial.available()) {
    return;
  }
  handleCommand(Serial.readStringUntil('\n'));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("T-Embed TFT display test"));

  if (!initBoardForDisplay()) {
    Serial.println(F("[TFT] Display power/reset init failed. Halting."));
    while (true) {
      delay(1000);
    }
  }

  tft.init();
  tft.setRotation(currentRotation);
  tft.fillScreen(TFT_BLACK);

  showPage(currentPage);
  printStatus();
  printHelp();
}

void loop() {
  pollSerialCommands();
  maybeAdvancePage();
  animateBall();
  delay(1);
}
