#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>

#include <TEmbedBoard.h>

namespace {

constexpr uint8_t kRotation = 1;
constexpr uint32_t kScanIntervalMs = 180;
constexpr uint32_t kCardLostTimeoutMs = 700;
constexpr uint32_t kNoCardMessageMs = 1200;
constexpr uint32_t kSpiClockHz = 10000000;
constexpr uint32_t kBusSettleMs = 20;

enum class UiState : uint8_t {
  Init = 0,
  Scanning,
  CardFound,
  IdentifyFail,
  NoCard,
  FatalError,
};

struct CardSnapshot {
  String uid;
  String type;
  uint16_t atqa = 0;
  uint8_t sak = 0;
  uint16_t userAreaSize = 0;
  uint16_t totalSize = 0;

  void clear() {
    uid = "";
    type = "";
    atqa = 0;
    sak = 0;
    userAreaSize = 0;
    totalSize = 0;
  }
};

TEmbedXL9555 ioExpander;
TFT_eSPI tft;
SPIClass nfcSPI(HSPI);
m5::unit::UnitUnified units;
m5::unit::UnitST25R3916 nfcUnit{BOARD_NFC_CS};
m5::nfc::NFCLayerA nfcA{nfcUnit};

UiState uiState = UiState::Init;
CardSnapshot currentCard;
String detailLine;
bool screenDirty = true;
bool cardPresent = false;
unsigned long lastPollAtMs = 0;
unsigned long lastSeenAtMs = 0;
unsigned long stateChangedAtMs = 0;

void setState(UiState next, const String& detail = String()) {
  if (uiState != next || detailLine != detail) {
    uiState = next;
    detailLine = detail;
    stateChangedAtMs = millis();
    screenDirty = true;
  }
}

String stateLabel() {
  switch (uiState) {
    case UiState::Init:         return "INIT";
    case UiState::Scanning:     return "SCANNING";
    case UiState::CardFound:    return "CARD FOUND";
    case UiState::IdentifyFail: return "IDENTIFY FAIL";
    case UiState::NoCard:       return "NO CARD";
    case UiState::FatalError:   return "FATAL ERROR";
  }
  return "?";
}

uint16_t stateColor() {
  switch (uiState) {
    case UiState::Init:         return TFT_CYAN;
    case UiState::Scanning:     return TFT_YELLOW;
    case UiState::CardFound:    return TFT_GREEN;
    case UiState::IdentifyFail: return TFT_ORANGE;
    case UiState::NoCard:       return TFT_DARKGREY;
    case UiState::FatalError:   return TFT_RED;
  }
  return TFT_WHITE;
}

bool initDisplayPower() {
  t_embed::board::deselectSharedSpiDevices();

  if (!t_embed::board::beginExpander(ioExpander)) {
    Serial.println(F("[NFC] XL9555 init failed."));
    return false;
  }

  if (!t_embed::board::setLowPowerEnabled(ioExpander, true)) {
    Serial.println(F("[NFC] Failed to enable LOW_PWR_3V3."));
    return false;
  }
  delay(kBusSettleMs);

  pinMode(BOARD_LCD_BL, OUTPUT);
  digitalWrite(BOARD_LCD_BL, HIGH);

  if (!t_embed::board::setLcdReset(ioExpander, true)) {
    Serial.println(F("[NFC] Failed to drive LCD reset high."));
    return false;
  }

  delay(5);

  if (!t_embed::board::setLcdReset(ioExpander, false)) {
    Serial.println(F("[NFC] Failed to drive LCD reset low."));
    return false;
  }

  delay(20);

  if (!t_embed::board::setLcdReset(ioExpander, true)) {
    Serial.println(F("[NFC] Failed to release LCD reset."));
    return false;
  }

  delay(120);
  return true;
}

bool initNfc() {
  t_embed::board::deselectSharedSpiDevices();
  delay(5);

  auto cfg = nfcUnit.config();
  cfg.mode = m5::nfc::NFC::A;
  cfg.vdd_voltage_5V = false;
  cfg.using_irq = true;
  cfg.irq = BOARD_NFC_IRQ;
  cfg.emulation = false;
  nfcUnit.config(cfg);

  SPISettings settings{kSpiClockHz, MSBFIRST, SPI_MODE1};
  if (!units.add(nfcUnit, nfcSPI, settings)) {
    Serial.println(F("[NFC] Units.add failed."));
    return false;
  }

  if (!units.begin()) {
    Serial.println(F("[NFC] Units.begin failed."));
    return false;
  }

  Serial.println(F("[NFC] ST25R3916 initialized in NFC-A mode."));
  return true;
}

void drawHeader() {
  tft.fillRect(0, 0, tft.width(), 24, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("ST25R3916 NFC-A Test", 8, 6, 2);
}

void drawFooter() {
  const int16_t footerY = tft.height() - 18;
  tft.fillRect(0, footerY, tft.width(), 18, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString("Place an NFC-A tag on the antenna", 6, footerY + 3, 1);
}

void drawValueRow(const char* label, const String& value, int16_t y) {
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(label, 10, y, 1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(value, 92, y, 1);
}

String sizeText() {
  if (!currentCard.totalSize) {
    return "-";
  }
  return String(currentCard.userAreaSize) + " / " + String(currentCard.totalSize) + " bytes";
}

String atqaSakText() {
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%04X / %02X", currentCard.atqa, currentCard.sak);
  return String(buffer);
}

void redrawScreen() {
  tft.fillScreen(TFT_BLACK);
  drawHeader();

  tft.setTextColor(stateColor(), TFT_BLACK);
  tft.drawString(stateLabel(), 10, 34, 4);

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(detailLine.isEmpty() ? "Polling every 180 ms" : detailLine, 10, 70, 1);

  drawValueRow("UID", currentCard.uid.isEmpty() ? "-" : currentCard.uid, 92);
  drawValueRow("Type", currentCard.type.isEmpty() ? "-" : currentCard.type, 108);
  drawValueRow("ATQA/SAK", currentCard.atqa || currentCard.sak ? atqaSakText() : "-", 124);
  drawValueRow("User/Total", sizeText(), 140);

  drawFooter();
  t_embed::board::deselectSharedSpiDevices();
}

void showFatalError(const __FlashStringHelper* message) {
  Serial.println(message);
  currentCard.clear();
  setState(UiState::FatalError, String(message));
  redrawScreen();
  while (true) {
    delay(1000);
  }
}

void logCard(const CardSnapshot& card) {
  Serial.print(F("[NFC] UID: "));
  Serial.println(card.uid);
  Serial.print(F("[NFC] Type: "));
  Serial.println(card.type);
  Serial.print(F("[NFC] ATQA: 0x"));
  Serial.println(card.atqa, HEX);
  Serial.print(F("[NFC] SAK: 0x"));
  Serial.println(card.sak, HEX);
  Serial.print(F("[NFC] User/Total: "));
  Serial.print(card.userAreaSize);
  Serial.print(F(" / "));
  Serial.println(card.totalSize);
}

void rememberCard(const m5::nfc::a::PICC& picc) {
  currentCard.uid = String(picc.uidAsString().c_str());
  currentCard.type = String(picc.typeAsString().c_str());
  currentCard.atqa = picc.atqa;
  currentCard.sak = picc.sak;
  currentCard.userAreaSize = picc.userAreaSize();
  currentCard.totalSize = picc.totalSize();
}

void handleDetectedPicc() {
  m5::nfc::a::PICC picc{};
  if (!nfcA.detect(picc, 100U)) {
    return;
  }

  const String detectedUid = String(picc.uidAsString().c_str());
  if (cardPresent && detectedUid == currentCard.uid) {
    lastSeenAtMs = millis();
    (void)nfcA.deactivate();
    return;
  }

  if (nfcA.identify(picc)) {
    rememberCard(picc);
    cardPresent = true;
    lastSeenAtMs = millis();
    setState(UiState::CardFound, "NFC-A PICC detected");
    logCard(currentCard);
  } else {
    currentCard.clear();
    currentCard.uid = detectedUid;
    cardPresent = true;
    lastSeenAtMs = millis();
    setState(UiState::IdentifyFail, "Detected tag but identify failed");
    Serial.print(F("[NFC] Failed to identify PICC: "));
    Serial.println(detectedUid);
  }

  (void)nfcA.deactivate();
}

void handleCardTimeout() {
  const unsigned long now = millis();

  if (cardPresent && (now - lastSeenAtMs > kCardLostTimeoutMs)) {
    cardPresent = false;
    currentCard.clear();
    setState(UiState::NoCard, "Card removed");
    Serial.println(F("[NFC] Card removed."));
    return;
  }

  if (!cardPresent && uiState == UiState::NoCard && (now - stateChangedAtMs > kNoCardMessageMs)) {
    setState(UiState::Scanning, "Waiting for NFC-A tag");
  }
}

void pollNfc() {
  const unsigned long now = millis();
  if (now - lastPollAtMs < kScanIntervalMs) {
    return;
  }
  lastPollAtMs = now;

  t_embed::board::deselectSharedSpiDevices();
  units.update();
  handleDetectedPicc();
  handleCardTimeout();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("T-Embed ST25R3916 NFC-A screen test"));

  if (!initDisplayPower()) {
    showFatalError(F("[NFC] Display power init failed."));
  }

  nfcSPI.begin(BOARD_NFC_SCK, BOARD_NFC_MISO, BOARD_NFC_MOSI);
  delay(kBusSettleMs);

  tft.init();
  tft.setRotation(kRotation);
  tft.fillScreen(TFT_BLACK);
  t_embed::board::deselectSharedSpiDevices();

  setState(UiState::Init, "Power rails and display ready");
  redrawScreen();

  if (!initNfc()) {
    showFatalError(F("[NFC] ST25R3916 init failed."));
  }

  setState(UiState::Scanning, "Waiting for NFC-A tag");
  screenDirty = true;
}

void loop() {
  pollNfc();

  if (screenDirty) {
    screenDirty = false;
    redrawScreen();
  }

  delay(5);
}
