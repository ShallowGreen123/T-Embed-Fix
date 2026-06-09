#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>

#include <TEmbedBoard.h>

namespace {

constexpr uint8_t kRotation      = 1;
constexpr uint8_t kLineHeight    = 16;
constexpr uint8_t kMarginLeft    = 8;
constexpr int16_t kHeaderHeight  = 24;
constexpr int16_t kFooterHeight  = 18;
constexpr uint32_t kBusSettleMs  = 20;

// ── UI state ────────────────────────────────────────────────────────────────
TEmbedXL9555 ioExpander;
TFT_eSPI     tft;

int16_t cursorY = kHeaderHeight + 4;  // current print row, below the header bar

// ── helpers ─────────────────────────────────────────────────────────────────

void drawHeader() {
  tft.fillRect(0, 0, tft.width(), kHeaderHeight, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("SD Card Test", kMarginLeft, 6, 2);
}

void clearLogArea() {
  const int16_t contentY = kHeaderHeight;
  const int16_t contentH = tft.height() - kHeaderHeight - kFooterHeight;
  tft.fillRect(0, contentY, tft.width(), contentH, TFT_BLACK);
  cursorY = kHeaderHeight + 4;
}

void nextLine() {
  cursorY += kLineHeight;
  if (cursorY + kLineHeight > tft.height() - kFooterHeight) {
    clearLogArea();
  }
}

void printRow(const char* label, const String& value,
              uint16_t labelColor, uint16_t valueColor) {
  tft.fillRect(0, cursorY, tft.width(), kLineHeight, TFT_BLACK);
  tft.setTextColor(labelColor, TFT_BLACK);
  tft.drawString(label, kMarginLeft, cursorY, 1);
  tft.setTextColor(valueColor, TFT_BLACK);
  tft.drawString(value, kMarginLeft + 90, cursorY, 1);
  nextLine();
}

void printPass(const char* label, const String& value = "") {
  printRow(label, value, TFT_CYAN, TFT_GREEN);
}

void printFail(const char* label, const String& value = "") {
  printRow(label, value, TFT_CYAN, TFT_RED);
}

void printInfo(const char* label, const String& value) {
  printRow(label, value, TFT_LIGHTGREY, TFT_WHITE);
}

void drawFooter(const char* msg) {
  const int16_t y = tft.height() - kFooterHeight;
  tft.fillRect(0, y, tft.width(), kFooterHeight, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString(msg, kMarginLeft, y + 3, 1);
}

// ── display + power init (mirrors nfc example) ──────────────────────────────
bool initDisplayPower() {
  t_embed::board::deselectSharedSpiDevices();

  if (!t_embed::board::beginExpander(ioExpander)) {
    Serial.println(F("[SD] XL9555 init failed."));
    return false;
  }

  if (!t_embed::board::setLowPowerEnabled(ioExpander, true)) {
    Serial.println(F("[SD] Failed to enable LOW_PWR_3V3."));
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

// ── SD test ─────────────────────────────────────────────────────────────────

String cardTypeStr(uint8_t t) {
  switch (t) {
    case CARD_MMC:  return "MMC";
    case CARD_SD:   return "SD";
    case CARD_SDHC: return "SDHC";
    default:        return "UNKNOWN";
  }
}

void runSdTest() {
  // SD shares the main SPI bus — deselect others first
  t_embed::board::deselectSharedSpiDevices();

  // ── mount ──
  if (!SD.begin(BOARD_SD_CS, SPI, 4000000)) {
    printFail("Mount", "FAIL");
    Serial.println(F("[SD] SD.begin() failed."));
    drawFooter("SD mount failed — check card");
    return;
  }
  printPass("Mount", "OK");

  // ── card type ──
  const uint8_t ctype = SD.cardType();
  printInfo("Type", cardTypeStr(ctype));

  // ── capacity ──
  const uint64_t totalMB = SD.totalBytes() / (1024ULL * 1024ULL);
  const uint64_t usedMB  = SD.usedBytes()  / (1024ULL * 1024ULL);
  printInfo("Total", String((uint32_t)totalMB) + " MB");
  printInfo("Used",  String((uint32_t)usedMB)  + " MB");

  // ── write test ──
  const char* kTestPath = "/t_embed_sd_test.txt";
  const char* kTestData = "T-Embed SD test OK\n";

  File f = SD.open(kTestPath, FILE_WRITE);
  if (!f) {
    printFail("Write", "FAIL");
    Serial.println(F("[SD] Open for write failed."));
  } else {
    f.print(kTestData);
    f.close();
    printPass("Write", "OK");
    Serial.println(F("[SD] Write OK."));
  }

  // ── read-back ──
  f = SD.open(kTestPath, FILE_READ);
  if (!f) {
    printFail("Read", "FAIL");
    Serial.println(F("[SD] Open for read failed."));
  } else {
    String line = f.readStringUntil('\n');
    f.close();
    const bool match = line.startsWith("T-Embed SD test OK");
    if (match) {
      printPass("Read", "OK");
      Serial.println(F("[SD] Read OK."));
    } else {
      printFail("Read", "MISMATCH");
      Serial.print(F("[SD] Read mismatch: "));
      Serial.println(line);
    }
  }

  // ── clean up test file ──
  SD.remove(kTestPath);

  // ── list root ──
  File root = SD.open("/");
  if (root) {
    uint8_t count = 0;
    File entry = root.openNextFile();
    while (entry && count < 6) {
      String name = String(entry.name());
      if (entry.isDirectory()) name = "[" + name + "]";
      printInfo(count == 0 ? "Root:" : "", name);
      entry.close();
      entry = root.openNextFile();
      count++;
    }
    if (!count) printInfo("Root:", "(empty)");
    root.close();
  }

  drawFooter("Test complete");
  Serial.println(F("[SD] Test complete."));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\nT-Embed SD Card Test"));

  if (!initDisplayPower()) {
    Serial.println(F("[SD] Display power init failed — halting."));
    while (true) delay(1000);
  }

  SPI.begin(BOARD_SD_SCK, BOARD_SD_MISO, BOARD_SD_MOSI);
  delay(kBusSettleMs);

  tft.init();
  tft.setRotation(kRotation);
  tft.fillScreen(TFT_BLACK);
  t_embed::board::deselectSharedSpiDevices();

  drawHeader();
  clearLogArea();
  drawFooter("Testing...");

  runSdTest();
}

void loop() {
  delay(1000);
}
