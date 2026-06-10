#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include <TEmbedBoard.h>

namespace {

constexpr uint8_t kRotation = 1;
constexpr uint8_t kLineHeight = 16;
constexpr uint8_t kMarginLeft = 8;
constexpr int16_t kHeaderHeight = 24;
constexpr int16_t kFooterHeight = 18;
constexpr uint32_t kBusSettleMs = 20;
constexpr uint32_t kSdPowerSettleMs = 120;
constexpr uint32_t kSdMountFrequencies[] = {10000000UL, 4000000UL, 1000000UL};

TEmbedXL9555 ioExpander;
TFT_eSPI tft;

int16_t cursorY = kHeaderHeight + 4;

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

  if (!t_embed::board::setLcdReset(ioExpander, true)) return false;
  delay(5);
  if (!t_embed::board::setLcdReset(ioExpander, false)) return false;
  delay(20);
  if (!t_embed::board::setLcdReset(ioExpander, true)) return false;
  delay(120);
  return true;
}

String cardTypeStr(uint8_t type) {
  switch (type) {
    case CARD_MMC: return "MMC";
    case CARD_SD: return "SD";
    case CARD_SDHC: return "SDHC";
    default: return "UNKNOWN";
  }
}

SPIClass& sharedSpi() {
  return tft.getSPIinstance();
}

uint16_t listRootToSerial() {
  File root = SD.open("/");
  if (!root) {
    Serial.println(F("[SD] Failed to open root directory."));
    return 0;
  }

  Serial.println(F("[SD] Root directory:"));
  uint16_t count = 0;
  File entry = root.openNextFile();
  while (entry) {
    String name = String(entry.name());
    if (entry.isDirectory()) {
      name = "[" + name + "]";
    }
    Serial.print(F("  "));
    Serial.println(name);
    entry.close();
    entry = root.openNextFile();
    ++count;
  }

  if (!count) {
    Serial.println(F("  (empty)"));
  }
  root.close();
  return count;
}

bool mountSdCard(uint32_t& mountedFrequency) {
  pinMode(BOARD_SD_CS, OUTPUT);
  digitalWrite(BOARD_SD_CS, HIGH);
  delay(kSdPowerSettleMs);

  for (uint32_t frequency : kSdMountFrequencies) {
    SD.end();
    t_embed::board::deselectSharedSpiDevices();
    delay(kBusSettleMs);

    Serial.print(F("[SD] Mount attempt @ "));
    Serial.print(frequency / 1000000UL);
    Serial.println(F(" MHz"));

    if (SD.begin(BOARD_SD_CS, sharedSpi(), frequency)) {
      mountedFrequency = frequency;
      return true;
    }
  }

  mountedFrequency = 0;
  return false;
}

void runSdTest() {
  t_embed::board::deselectSharedSpiDevices();

  uint32_t mountedFrequency = 0;
  if (!mountSdCard(mountedFrequency)) {
    printFail("Mount", "FAIL");
    printFail("Result", "FAIL");
    Serial.println(F("[SD] SD.begin() failed."));
    Serial.println(F("[SD] TEST FAIL: mount failed."));
    drawFooter("SD TEST FAIL");
    return;
  }

  printPass("Mount", "OK");
  printInfo("SPI", String(mountedFrequency / 1000000UL) + " MHz");

  const uint8_t cardType = SD.cardType();
  printInfo("Type", cardTypeStr(cardType));

  const uint64_t totalMB = SD.totalBytes() / (1024ULL * 1024ULL);
  const uint64_t usedMB = SD.usedBytes() / (1024ULL * 1024ULL);
  printInfo("Total", String(static_cast<uint32_t>(totalMB)) + " MB");
  printInfo("Used", String(static_cast<uint32_t>(usedMB)) + " MB");

  const char* testPath = "/t_embed_sd_test.txt";
  const char* testData = "T-Embed SD test OK\n";
  bool writeOk = false;
  bool readOk = false;

  File file = SD.open(testPath, FILE_WRITE);
  if (!file) {
    printFail("Write", "FAIL");
    Serial.println(F("[SD] Open for write failed."));
  } else {
    file.print(testData);
    file.close();
    writeOk = true;
    printPass("Write", "OK");
    Serial.println(F("[SD] Write OK."));
  }

  file = SD.open(testPath, FILE_READ);
  if (!file) {
    printFail("Read", "FAIL");
    Serial.println(F("[SD] Open for read failed."));
  } else {
    String line = file.readStringUntil('\n');
    file.close();
    const bool match = line.startsWith("T-Embed SD test OK");
    if (match) {
      readOk = true;
      printPass("Read", "OK");
      Serial.println(F("[SD] Read OK."));
    } else {
      printFail("Read", "MISMATCH");
      Serial.print(F("[SD] Read mismatch: "));
      Serial.println(line);
    }
  }

  SD.remove(testPath);

  const uint16_t rootCount = listRootToSerial();
  printInfo("Root", String(rootCount) + " entries");

  const bool testPassed = writeOk && readOk;
  if (testPassed) {
    printPass("Result", "PASS");
    drawFooter("SD TEST PASS");
    Serial.println(F("[SD] TEST PASS."));
  } else {
    printFail("Result", "FAIL");
    drawFooter("SD TEST FAIL");
    Serial.println(F("[SD] TEST FAIL: write/read check failed."));
  }
  Serial.println(F("[SD] Test complete."));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\nT-Embed SD Card Test"));

  if (!initDisplayPower()) {
    Serial.println(F("[SD] Display power init failed - halting."));
    while (true) {
      delay(1000);
    }
  }

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
