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

constexpr int16_t kUiMargin = 8;
constexpr int16_t kHeaderHeight = 24;
constexpr int16_t kFooterHeight = 18;

constexpr int16_t kStatusX = 8;
constexpr int16_t kStatusY = 34;
constexpr int16_t kStatusW = 304;
constexpr int16_t kStatusH = 38;

constexpr int16_t kUidX = 8;
constexpr int16_t kUidY = 80;
constexpr int16_t kUidW = 304;
constexpr int16_t kUidH = 20;

constexpr int16_t kMetaY = 108;
constexpr int16_t kMetaW = 148;
constexpr int16_t kMetaH = 18;
constexpr int16_t kMetaLeftX = 8;
constexpr int16_t kMetaRightX = 164;

constexpr int16_t kSizeX = 8;
constexpr int16_t kSizeY = 134;
constexpr int16_t kSizeW = 304;
constexpr int16_t kSizeH = 18;

constexpr uint16_t kColorBg = 0x0841;
constexpr uint16_t kColorPanel = 0x1082;
constexpr uint16_t kColorPanelEdge = 0x31A6;
constexpr uint16_t kColorCard = 0x18C3;
constexpr uint16_t kColorPassBg = 0x0A41;
constexpr uint16_t kColorWarnBg = 0x5A00;
constexpr uint16_t kColorFailBg = 0x3006;

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
TFT_eSprite canvas(&tft);
SPIClass nfcSPI(HSPI);
m5::unit::UnitUnified units;
m5::unit::UnitST25R3916 nfcUnit{BOARD_NFC_CS};
m5::nfc::NFCLayerA nfcA{nfcUnit};

UiState uiState = UiState::Init;
CardSnapshot currentCard;
String detailLine;
bool screenDirty = true;
bool canvasReady = false;
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

#include "nfc_st25r3916_ui.h"

void redrawScreen() {
  if (canvasReady) {
    drawUi(canvas);
    canvas.pushSprite(0, 0);
  } else {
    drawUi(tft);
  }
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

bool detectSinglePicc(m5::nfc::a::PICC& picc, const uint32_t timeoutMs) {
  const unsigned long startMs = millis();

  do {
    picc = {};

    uint16_t atqa = 0;
    bool detected = nfcA.request(atqa);
    if (!detected) {
      detected = nfcA.wakeup(atqa);
    }
    if (!detected) {
      delay(1);
      continue;
    }

    picc.atqa = atqa;
    if (!nfcA.select(picc)) {
      delay(1);
      continue;
    }

    return true;
  } while (millis() - startMs <= timeoutMs);

  return false;
}

void handleDetectedPicc() {
  m5::nfc::a::PICC picc{};
  if (!detectSinglePicc(picc, 100U)) {
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

  canvas.setColorDepth(16);
  canvasReady = (canvas.createSprite(tft.width(), tft.height()) != nullptr);
  if (!canvasReady) {
    Serial.println(F("[NFC] Sprite allocation failed, using direct TFT redraw."));
  }

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
