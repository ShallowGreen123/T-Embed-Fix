#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>

#include <GaugeBQ27220.hpp>
#include <bq27220.h>

#define XPOWERS_CHIP_SY6970
#include <XPowersLib.h>

#include <TEmbedBoard.h>

namespace {

constexpr uint8_t kRotation = 1;
constexpr uint32_t kPollIntervalMs = 1000;
constexpr uint32_t kUiStateDebounceMs = 2500;
constexpr uint32_t kChargeTopOffRetryMs = 60000;
constexpr uint32_t kDebounceMs = 20;
constexpr uint32_t kShutdownHoldMs = 2000;
constexpr uint32_t kTransientDetailMs = 2500;
constexpr uint32_t kPowerSettleMs = 20;

constexpr uint16_t kBatteryCapacityMah = 1300;
constexpr uint16_t kChargeTargetVoltageMv = 4208;
constexpr uint16_t kPrechargeCurrentMa = 128;
constexpr uint16_t kFastChargeCurrentMa = 512;
// SY6970 library does not currently expose a termination-current setter.
// Keep the pack spec here for reporting/reference.
constexpr uint16_t kTerminationCurrentMa = 128;
constexpr uint16_t kSysPowerDownVoltageMv = 3300;
constexpr uint16_t kInputCurrentSdpMa = 500;
constexpr uint16_t kInputCurrentAdapterMa = 1000;
constexpr uint16_t kChargeDoneSocThreshold = 100;
constexpr uint16_t kRechargeThresholdOffsetMv = 100;
constexpr uint16_t kChargeTopOffRestartMarginMv = 32;
constexpr uint16_t kBqRequestedChargeCurrentMa = kFastChargeCurrentMa;
constexpr uint16_t kBqRequestedChargeVoltageMv = kChargeTargetVoltageMv;
constexpr uint16_t kBqTaperCurrentMa = kTerminationCurrentMa;
constexpr uint16_t kBqChargeTerminationVoltageMv = 50;
constexpr uint16_t kBqChargeDetectThresholdMa = 40;
constexpr uint16_t kBqQuitCurrentMa = 20;
constexpr int16_t kChargeCurrentIntoBatteryThresholdMa = 30;
constexpr int16_t kDischargeCurrentThresholdMa = -30;

constexpr int16_t kHeaderHeight = 24;
constexpr int16_t kFooterHeight = 18;
constexpr int16_t kLeftX = 8;
constexpr int16_t kLeftValueX = 62;
constexpr int16_t kLeftValueW = 96;
constexpr int16_t kRightX = 162;
constexpr int16_t kRightValueX = 222;
constexpr int16_t kRightValueW = 94;
constexpr int16_t kRow0 = 80;
constexpr int16_t kRowGap = 14;
constexpr int16_t kStateX = 8;
constexpr int16_t kStateY = 30;
constexpr int16_t kStateW = 220;
constexpr int16_t kStateH = 26;
constexpr int16_t kDetailX = 8;
constexpr int16_t kDetailY = 64;
constexpr int16_t kDetailW = 304;
constexpr int16_t kDetailH = 10;
constexpr int16_t kValueFieldH = 10;

enum class UiState : uint8_t {
  Init = 0,
  Charging,
  ChargeDone,
  Discharging,
  Idle,
  BqConfigWarn,
  Error,
};

struct ButtonState {
  uint8_t pin;
  bool pressed = false;
  bool longPressHandled = false;
  uint32_t lastChangeMs = 0;
  uint32_t pressedAtMs = 0;
};

struct BatteryMetrics {
  uint16_t soc = 0;
  uint16_t bqVoltageMv = 0;
  int16_t ibatMa = 0;
  uint16_t remainMah = 0;
  uint16_t fullMah = 0;
  uint16_t designMah = 0;
  uint16_t soh = 0;
  int16_t tempDeciC = 0;
  uint16_t tteMin = 0;
  uint16_t ttfMin = 0;
  uint16_t syBattVoltageMv = 0;
  uint16_t vbusMv = 0;
  uint16_t vsysMv = 0;
  uint16_t chargeCurrentMa = 0;
  uint8_t busStatus = 0;
  uint8_t chargeStatus = 0;
  bool chargeEnabled = false;
  bool bqFullChargeDetected = false;
  bool isDischarging = false;
  bool vbusPresent = false;
};

TEmbedXL9555 ioExpander;
TFT_eSPI tft;
GaugeBQ27220 gauge;
BQ27220 gaugeDm;
XPowersPPM pmu;

ButtonState userButton;

UiState uiState = UiState::Init;
UiState pendingUiState = UiState::Init;
BatteryMetrics metrics;
BatteryMetrics lastDrawnMetrics;
bool hasMetrics = false;
bool hasDrawnMetrics = false;
bool screenDirty = true;
bool bqConfigWarning = false;
bool frameDrawn = false;
String bqConfigDetail = "BQ cfg pending";
String transientDetail;
String lastDrawnDetail;
UiState lastDrawnUiState = UiState::Init;
unsigned long detailExpiresAtMs = 0;
unsigned long lastPollAtMs = 0;
unsigned long pendingUiStateSinceMs = 0;
unsigned long lastChargeTopOffAttemptMs = 0;
int16_t appliedInputCurrentMa = -1;
String serialLine;

BQ27220DMData makeDmU16Entry(const uint16_t address, const uint16_t value) {
  BQ27220DMData entry = {};
  entry.type = BQ27220DMTypeU16;
  entry.address = address;
  entry.value.u16 = value;
  return entry;
}

BQ27220DMData makeDmEndEntry() {
  BQ27220DMData entry = {};
  entry.type = BQ27220DMTypeEnd;
  return entry;
}

void printBqChargeConfigTargets() {
  Serial.print(kBqRequestedChargeCurrentMa);
  Serial.print(F("/"));
  Serial.print(kBqRequestedChargeVoltageMv);
  Serial.print(F("/"));
  Serial.print(kBqTaperCurrentMa);
  Serial.print(F("/"));
  Serial.print(kBqChargeTerminationVoltageMv);
  Serial.print(F("/"));
  Serial.print(kBqChargeDetectThresholdMa);
  Serial.print(F("/"));
  Serial.println(kBqQuitCurrentMa);
}

const char* stateLabel(const UiState state) {
  switch (state) {
    case UiState::Init:         return "INIT";
    case UiState::Charging:     return "CHARGING";
    case UiState::ChargeDone:   return "CHARGE DONE";
    case UiState::Discharging:  return "DISCHARGING";
    case UiState::Idle:         return "IDLE";
    case UiState::BqConfigWarn: return "BQ CFG WARN";
    case UiState::Error:        return "ERROR";
  }
  return "?";
}

uint16_t stateColor(const UiState state) {
  switch (state) {
    case UiState::Init:         return TFT_CYAN;
    case UiState::Charging:     return TFT_GREEN;
    case UiState::ChargeDone:   return TFT_YELLOW;
    case UiState::Discharging:  return TFT_ORANGE;
    case UiState::Idle:         return TFT_LIGHTGREY;
    case UiState::BqConfigWarn: return TFT_RED;
    case UiState::Error:        return TFT_RED;
  }
  return TFT_WHITE;
}

const char* busStatusLabel(const uint8_t status) {
  switch (static_cast<XPowersPPM::BusStatus>(status)) {
    case XPowersPPM::BUS_STATE_NOINPUT:             return "No input";
    case XPowersPPM::BUS_STATE_USB_SDP:             return "USB SDP";
    case XPowersPPM::BUS_STATE_USB_CDP:             return "USB CDP";
    case XPowersPPM::BUS_STATE_USB_DCP:             return "USB DCP";
    case XPowersPPM::BUS_STATE_HVDCP:               return "HVDCP";
    case XPowersPPM::BUS_STATE_ADAPTER:             return "Adapter";
    case XPowersPPM::BUS_STATE_NO_STANDARD_ADAPTER: return "Adapter";
    case XPowersPPM::BUS_STATE_OTG:                 return "OTG";
  }
  return "Unknown";
}

const char* chargeStatusLabel(const uint8_t status) {
  switch (static_cast<XPowersPPM::ChargeStatus>(status)) {
    case XPowersPPM::CHARGE_STATE_NO_CHARGE:   return "Not Charging";
    case XPowersPPM::CHARGE_STATE_PRE_CHARGE:  return "Pre-charge";
    case XPowersPPM::CHARGE_STATE_FAST_CHARGE: return "Fast Charging";
    case XPowersPPM::CHARGE_STATE_DONE:        return "Done";
    case XPowersPPM::CHARGE_STATE_UNKOWN:      return "Unknown";
  }
  return "Unknown";
}

String currentDetail() {
  if (detailExpiresAtMs && millis() < detailExpiresAtMs) {
    return transientDetail;
  }
  return bqConfigDetail;
}

void setTransientDetail(const String& detail, const uint32_t durationMs = kTransientDetailMs) {
  transientDetail = detail;
  detailExpiresAtMs = millis() + durationMs;
  screenDirty = true;
}

String formatSignedMilliamp(const int16_t value) {
  return String(value) + " mA";
}

String formatVoltage(const uint16_t value) {
  if (!value) {
    return "-";
  }
  return String(value) + " mV";
}

String formatTimePair(const uint16_t tteMin, const uint16_t ttfMin) {
  auto part = [](const uint16_t value) -> String {
    if (!value || value == 65535U) {
      return "-";
    }
    return String(value);
  };
  return part(tteMin) + " / " + part(ttfMin) + " min";
}

String formatTemp(const int16_t deciC) {
  const bool negative = deciC < 0;
  const int16_t magnitude = negative ? -deciC : deciC;
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%s%d.%d C", negative ? "-" : "", magnitude / 10, magnitude % 10);
  return String(buffer);
}

String formatCapacityTriplet(const BatteryMetrics& m) {
  return String(m.remainMah) + "/" + String(m.fullMah) + "/" + String(m.designMah) + " mAh";
}

uint16_t effectiveBatteryVoltageMv(const BatteryMetrics& sample) {
  return max(sample.bqVoltageMv, sample.syBattVoltageMv);
}

bool hasExternalPower() {
  const auto bus = static_cast<XPowersPPM::BusStatus>(metrics.busStatus);
  return metrics.vbusPresent && bus != XPowersPPM::BUS_STATE_OTG;
}

void drawHeader() {
  tft.fillRect(0, 0, tft.width(), 24, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("Battery Charge/Discharge Test", 8, 6, 2);
}

void drawFooter() {
  const int16_t footerY = tft.height() - 18;
  tft.fillRect(0, footerY, tft.width(), 18, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString("Hold USER KEY 2s to shutdown | Serial: shutdown", 4, footerY + 3, 1);
}

void clearField(const int16_t x, const int16_t y, const int16_t w, const int16_t h) {
  tft.fillRect(x, y, w, h, TFT_BLACK);
}

void drawValue(const String& value, const int16_t x, const int16_t y, const uint16_t color = TFT_WHITE) {
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(value, x, y, 1);
}

void clearValueField(const int16_t x, const int16_t y, const int16_t w) {
  clearField(x, y, w, kValueFieldH);
}

void updateTextField(const bool force,
                     const String& value,
                     const String& lastValue,
                     const int16_t x,
                     const int16_t y,
                     const int16_t w,
                     const uint16_t color = TFT_WHITE) {
  if (!force && value == lastValue) {
    return;
  }
  clearValueField(x, y, w);
  drawValue(value, x, y, color);
}

void drawStaticFrame() {
  tft.fillScreen(TFT_BLACK);
  drawHeader();
  drawFooter();

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("SOC", kLeftX, kRow0, 1);
  tft.drawString("VBAT", kLeftX, kRow0 + kRowGap, 1);
  tft.drawString("IBAT", kLeftX, kRow0 + kRowGap * 2, 1);
  tft.drawString("R/F/D", kLeftX, kRow0 + kRowGap * 3, 1);
  tft.drawString("SOH/T", kLeftX, kRow0 + kRowGap * 4, 1);

  tft.drawString("VBUS/VSYS", kRightX, kRow0, 1);
  tft.drawString("SY Bus", kRightX, kRow0 + kRowGap, 1);
  tft.drawString("Charge", kRightX, kRow0 + kRowGap * 2, 1);
  tft.drawString("Cfg V/P/F", kRightX, kRow0 + kRowGap * 3, 1);
  tft.drawString("TTE/TTF", kRightX, kRow0 + kRowGap * 4, 1);

  frameDrawn = true;
}

void drawStatusArea(const bool force) {
  if (force || uiState != lastDrawnUiState) {
    clearField(kStateX, kStateY, kStateW, kStateH);
    tft.setTextColor(stateColor(uiState), TFT_BLACK);
    tft.drawString(stateLabel(uiState), kStateX, kStateY, 4);
    lastDrawnUiState = uiState;
  }

  const String detail = currentDetail();
  if (force || detail != lastDrawnDetail) {
    clearField(kDetailX, kDetailY, kDetailW, kDetailH);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(detail, kDetailX, kDetailY, 1);
    lastDrawnDetail = detail;
  }
}

void drawMetricValues(const bool force) {
  updateTextField(force,
                  String(metrics.soc) + "%",
                  String(lastDrawnMetrics.soc) + "%",
                  kLeftValueX,
                  kRow0,
                  kLeftValueW);
  updateTextField(force,
                  formatVoltage(metrics.bqVoltageMv),
                  formatVoltage(lastDrawnMetrics.bqVoltageMv),
                  kLeftValueX,
                  kRow0 + kRowGap,
                  kLeftValueW);
  updateTextField(force,
                  formatSignedMilliamp(metrics.ibatMa),
                  formatSignedMilliamp(lastDrawnMetrics.ibatMa),
                  kLeftValueX,
                  kRow0 + kRowGap * 2,
                  kLeftValueW);
  updateTextField(force,
                  formatCapacityTriplet(metrics),
                  formatCapacityTriplet(lastDrawnMetrics),
                  kLeftValueX,
                  kRow0 + kRowGap * 3,
                  kLeftValueW);
  updateTextField(force,
                  String(metrics.soh) + "% / " + formatTemp(metrics.tempDeciC),
                  String(lastDrawnMetrics.soh) + "% / " + formatTemp(lastDrawnMetrics.tempDeciC),
                  kLeftValueX,
                  kRow0 + kRowGap * 4,
                  kLeftValueW);

  updateTextField(force,
                  formatVoltage(metrics.vbusMv) + " / " + formatVoltage(metrics.vsysMv),
                  formatVoltage(lastDrawnMetrics.vbusMv) + " / " + formatVoltage(lastDrawnMetrics.vsysMv),
                  kRightValueX,
                  kRow0,
                  kRightValueW);
  updateTextField(force,
                  String(busStatusLabel(metrics.busStatus)),
                  String(busStatusLabel(lastDrawnMetrics.busStatus)),
                  kRightValueX,
                  kRow0 + kRowGap,
                  kRightValueW);
  updateTextField(force,
                  String(chargeStatusLabel(metrics.chargeStatus)) + " " + String(metrics.chargeCurrentMa) + "mA",
                  String(chargeStatusLabel(lastDrawnMetrics.chargeStatus)) + " " + String(lastDrawnMetrics.chargeCurrentMa) + "mA",
                  kRightValueX,
                  kRow0 + kRowGap * 2,
                  kRightValueW);
  updateTextField(force,
                  String(kChargeTargetVoltageMv) + "/" + String(kPrechargeCurrentMa) + "/" + String(kFastChargeCurrentMa),
                  String(kChargeTargetVoltageMv) + "/" + String(kPrechargeCurrentMa) + "/" + String(kFastChargeCurrentMa),
                  kRightValueX,
                  kRow0 + kRowGap * 3,
                  kRightValueW);
  updateTextField(force,
                  formatTimePair(metrics.tteMin, metrics.ttfMin),
                  formatTimePair(lastDrawnMetrics.tteMin, lastDrawnMetrics.ttfMin),
                  kRightValueX,
                  kRow0 + kRowGap * 4,
                  kRightValueW);
}

void redrawScreen() {
  const bool force = !frameDrawn || !hasDrawnMetrics;
  if (!frameDrawn) {
    drawStaticFrame();
  }

  tft.startWrite();
  drawStatusArea(force);
  drawMetricValues(force);
  tft.endWrite();

  t_embed::board::deselectSharedSpiDevices();
  lastDrawnMetrics = metrics;
  hasDrawnMetrics = hasMetrics;
}

void showFatalError(const __FlashStringHelper* message) {
  Serial.println(message);
  uiState = UiState::Error;
  bqConfigDetail = String(message);
  transientDetail = "";
  detailExpiresAtMs = 0;
  redrawScreen();
  while (true) {
    delay(1000);
  }
}

bool initDisplayPower() {
  t_embed::board::deselectSharedSpiDevices();

  if (!t_embed::board::beginExpander(ioExpander)) {
    Serial.println(F("[BAT] XL9555 init failed."));
    return false;
  }

  if (!t_embed::board::setLowPowerEnabled(ioExpander, true)) {
    Serial.println(F("[BAT] Failed to enable LOW_PWR_3V3."));
    return false;
  }

  delay(kPowerSettleMs);

  pinMode(BOARD_LCD_BL, OUTPUT);
  digitalWrite(BOARD_LCD_BL, HIGH);

  if (!t_embed::board::setLcdReset(ioExpander, true)) {
    Serial.println(F("[BAT] Failed to drive LCD reset high."));
    return false;
  }

  delay(5);

  if (!t_embed::board::setLcdReset(ioExpander, false)) {
    Serial.println(F("[BAT] Failed to drive LCD reset low."));
    return false;
  }

  delay(20);

  if (!t_embed::board::setLcdReset(ioExpander, true)) {
    Serial.println(F("[BAT] Failed to release LCD reset."));
    return false;
  }

  delay(120);
  return true;
}

void printHelp() {
  Serial.println();
  Serial.println(F("Battery test commands:"));
  Serial.println(F("  help       - show this help"));
  Serial.println(F("  status     - print current battery / charger status"));
  Serial.println(F("  charge on  - enable SY6970 charging"));
  Serial.println(F("  charge off - disable SY6970 charging"));
  Serial.println(F("  shutdown   - power off via SY6970 (battery-only)"));
  Serial.println();
}

void printStatus() {
  if (!hasMetrics) {
    Serial.println(F("[BAT] Metrics not ready yet."));
    return;
  }

  Serial.println();
  Serial.print(F("[BAT] UI State:        "));
  Serial.println(stateLabel(uiState));
  Serial.print(F("[BAT] Detail:          "));
  Serial.println(currentDetail());
  Serial.print(F("[BAT] BQ Voltage:      "));
  Serial.print(metrics.bqVoltageMv);
  Serial.println(F(" mV"));
  Serial.print(F("[BAT] BQ Current:      "));
  Serial.print(metrics.ibatMa);
  Serial.println(F(" mA"));
  Serial.print(F("[BAT] SOC / SOH:       "));
  Serial.print(metrics.soc);
  Serial.print(F("% / "));
  Serial.print(metrics.soh);
  Serial.println(F("%"));
  Serial.print(F("[BAT] Rem/Full/Design: "));
  Serial.print(metrics.remainMah);
  Serial.print(F(" / "));
  Serial.print(metrics.fullMah);
  Serial.print(F(" / "));
  Serial.print(metrics.designMah);
  Serial.println(F(" mAh"));
  Serial.print(F("[BAT] Temp:            "));
  Serial.print(metrics.tempDeciC / 10);
  Serial.print('.');
  Serial.print(abs(metrics.tempDeciC % 10));
  Serial.println(F(" C"));
  Serial.print(F("[BAT] VBUS / VSYS:     "));
  Serial.print(metrics.vbusMv);
  Serial.print(F(" / "));
  Serial.print(metrics.vsysMv);
  Serial.println(F(" mV"));
  Serial.print(F("[BAT] SY VBAT:         "));
  Serial.print(metrics.syBattVoltageMv);
  Serial.println(F(" mV"));
  Serial.print(F("[BAT] SY Bus:          "));
  Serial.println(busStatusLabel(metrics.busStatus));
  Serial.print(F("[BAT] Charge State:    "));
  Serial.println(chargeStatusLabel(metrics.chargeStatus));
  Serial.print(F("[BAT] Charge Enabled:  "));
  Serial.println(metrics.chargeEnabled ? F("yes") : F("no"));
  Serial.print(F("[BAT] BQ FC Flag:      "));
  Serial.println(metrics.bqFullChargeDetected ? F("yes") : F("no"));
  Serial.print(F("[BAT] Charge Current:  "));
  Serial.print(metrics.chargeCurrentMa);
  Serial.println(F(" mA"));
  Serial.print(F("[BAT] Term Current:    "));
  Serial.print(kTerminationCurrentMa);
  Serial.println(F(" mA (pack spec)"));
  Serial.print(F("[BAT] TTE / TTF:       "));
  Serial.print(metrics.tteMin);
  Serial.print(F(" / "));
  Serial.print(metrics.ttfMin);
  Serial.println(F(" min"));
  Serial.print(F("[BAT] BQ Req V/I:      "));
  Serial.print(gauge.getRequestChargingVoltage());
  Serial.print(F(" / "));
  Serial.print(gauge.getRequestChargingCurrent());
  Serial.println(F(" mV/mA"));
  Serial.print(F("[BAT] Input Limit:     "));
  if (appliedInputCurrentMa < 0) {
    Serial.println(F("n/a"));
  } else {
    Serial.print(appliedInputCurrentMa);
    Serial.println(F(" mA"));
  }
}

bool initGauge() {
  if (!gauge.begin(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL)) {
    Serial.println(F("[BAT] BQ27220 init failed."));
    return false;
  }

  Serial.print(F("[BAT] BQ27220 Chip ID: 0x"));
  Serial.println(gauge.getChipID(), HEX);
  Serial.print(F("[BAT] BQ27220 HW Ver: 0x"));
  Serial.println(gauge.getHardwareVersion(), HEX);

  if (!gauge.refresh()) {
    Serial.println(F("[BAT] BQ27220 refresh failed."));
    return false;
  }

  Serial.print(F("[BAT] BQ DesignCapacity: "));
  Serial.println(gauge.getDesignCapacity());
  Serial.print(F("[BAT] BQ FullChargeCapacity: "));
  Serial.println(gauge.getFullChargeCapacity());
  Serial.print(F("[BAT] BQ Req Charge V/I: "));
  Serial.print(gauge.getRequestChargingVoltage());
  Serial.print(F(" mV / "));
  Serial.print(gauge.getRequestChargingCurrent());
  Serial.println(F(" mA"));
  return true;
}

bool configureGaugeCapacity() {
  const uint16_t designCapacity = gauge.getDesignCapacity();
  const uint16_t fullCapacity = gauge.getFullChargeCapacity();

  if (designCapacity == kBatteryCapacityMah && fullCapacity == kBatteryCapacityMah) {
    bqConfigWarning = false;
    bqConfigDetail = "BQ CFG OK";
    Serial.println(F("[BAT] BQ27220 capacity already 1300/1300 mAh."));
    return true;
  }

  Serial.print(F("[BAT] Updating BQ27220 capacity from "));
  Serial.print(designCapacity);
  Serial.print(F("/"));
  Serial.print(fullCapacity);
  Serial.println(F(" to 1300/1300 mAh."));

  if (!gauge.setNewCapacity(kBatteryCapacityMah, kBatteryCapacityMah)) {
    bqConfigWarning = true;
    bqConfigDetail = "BQ write failed";
    Serial.println(F("[BAT] BQ27220 capacity update failed."));
    return false;
  }

  delay(50);

  if (!gauge.refresh()) {
    bqConfigWarning = true;
    bqConfigDetail = "BQ verify refresh failed";
    Serial.println(F("[BAT] BQ27220 refresh failed after update."));
    return false;
  }

  if (gauge.getDesignCapacity() != kBatteryCapacityMah || gauge.getFullChargeCapacity() != kBatteryCapacityMah) {
    bqConfigWarning = true;
    bqConfigDetail = "BQ verify mismatch";
    Serial.print(F("[BAT] BQ27220 verify mismatch: "));
    Serial.print(gauge.getDesignCapacity());
    Serial.print(F("/"));
    Serial.println(gauge.getFullChargeCapacity());
    return false;
  }

  bqConfigWarning = false;
  bqConfigDetail = "BQ CFG OK";
  Serial.println(F("[BAT] BQ27220 capacity update verified."));
  return true;
}

bool configureGaugeChargeParameters() {
  BQ27220DMData chargeConfig[] = {
      makeDmU16Entry(BQ27220DMAddressChargingChargingCurrent, kBqRequestedChargeCurrentMa),
      makeDmU16Entry(BQ27220DMAddressChargingChargingVoltage, kBqRequestedChargeVoltageMv),
      makeDmU16Entry(BQ27220DMAddressChargingTaperCurrent, kBqTaperCurrentMa),
      makeDmU16Entry(BQ27220DMAddressGasGaugingCEDVProfile1ChargeTerminationVoltage,
                     kBqChargeTerminationVoltageMv),
      makeDmU16Entry(BQ27220DMAddressConfigurationCurrentThresholdsChargeDetectThreshold,
                     kBqChargeDetectThresholdMa),
      makeDmU16Entry(BQ27220DMAddressConfigurationCurrentThresholdsQuitCurrent, kBqQuitCurrentMa),
      makeDmEndEntry(),
  };

  bool success = false;
  bool reseal = false;

  do {
    BQ27220OperationStatus operationStatus = {};
    gaugeDm.getOperationStatus(&operationStatus);
    reseal = operationStatus.reg.SEC == Bq27220OperationStatusSecSealed;

    if (!gaugeDm.unsealAccess()) {
      bqConfigWarning = true;
      bqConfigDetail = "BQ unseal failed";
      Serial.println(F("[BAT] Failed to unseal BQ27220."));
      break;
    }

    if (!gaugeDm.fullAccess()) {
      bqConfigWarning = true;
      bqConfigDetail = "BQ full access failed";
      Serial.println(F("[BAT] Failed to enter BQ27220 full access mode."));
      break;
    }

    if (gaugeDm.dateMemoryCheck(chargeConfig, false)) {
      Serial.println(F("[BAT] BQ27220 charge parameters already match target pack."));
      success = true;
      break;
    }

    Serial.print(F("[BAT] Updating BQ charge cfg I/V/Taper/TermV/Detect/Quit: "));
    printBqChargeConfigTargets();

    if (!gaugeDm.dateMemoryCheck(chargeConfig, true)) {
      bqConfigWarning = true;
      bqConfigDetail = "BQ write cfg failed";
      Serial.println(F("[BAT] Failed to update BQ27220 charge parameters."));
      break;
    }

    if (!gaugeDm.dateMemoryCheck(chargeConfig, false)) {
      bqConfigWarning = true;
      bqConfigDetail = "BQ cfg verify mismatch";
      Serial.println(F("[BAT] BQ27220 charge parameter verify mismatch."));
      break;
    }

    delay(100);
    if (!gauge.refresh()) {
      bqConfigWarning = true;
      bqConfigDetail = "BQ refresh after cfg failed";
      Serial.println(F("[BAT] BQ27220 refresh failed after charge parameter update."));
      break;
    }

    success = true;
  } while (false);

  if (reseal && !gaugeDm.sealAccess()) {
    bqConfigWarning = true;
    bqConfigDetail = "BQ reseal failed";
    Serial.println(F("[BAT] Failed to reseal BQ27220."));
    return false;
  }

  if (!success) {
    return false;
  }

  bqConfigWarning = false;
  bqConfigDetail = "BQ CFG OK";
  Serial.print(F("[BAT] BQ charge cfg verified I/V/Taper/TermV/Detect/Quit: "));
  printBqChargeConfigTargets();
  return true;
}

bool configureTerminationAndRecharge() {
  if (kTerminationCurrentMa < 64 || kTerminationCurrentMa > 1024 || (kTerminationCurrentMa % 64) != 0) {
    Serial.println(F("[BAT] Invalid SY6970 termination current setting."));
    return false;
  }

  int reg05 = pmu.readRegister(POWERS_PPM_REG_05H);
  if (reg05 < 0) {
    Serial.println(F("[BAT] Failed to read SY6970 REG05."));
    return false;
  }
  reg05 &= 0xF0;
  reg05 |= ((kTerminationCurrentMa - 64) / 64) & 0x0F;
  if (pmu.writeRegister(POWERS_PPM_REG_05H, static_cast<uint8_t>(reg05)) != 0) {
    Serial.println(F("[BAT] Failed to write SY6970 termination current."));
    return false;
  }

  int reg06 = pmu.readRegister(POWERS_PPM_REG_06H);
  if (reg06 < 0) {
    Serial.println(F("[BAT] Failed to read SY6970 REG06."));
    return false;
  }
  if (kRechargeThresholdOffsetMv >= 200) {
    reg06 |= 0x01;
  } else {
    reg06 &= ~0x01;
  }
  if (pmu.writeRegister(POWERS_PPM_REG_06H, static_cast<uint8_t>(reg06)) != 0) {
    Serial.println(F("[BAT] Failed to write SY6970 recharge threshold."));
    return false;
  }

  return true;
}

bool configurePmu() {
  if (!pmu.init(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL, BOARD_I2C_SY6970)) {
    Serial.println(F("[BAT] SY6970 init failed."));
    return false;
  }

  if (!pmu.setSysPowerDownVoltage(kSysPowerDownVoltageMv)) {
    Serial.println(F("[BAT] Failed to set SYS power-down voltage."));
    return false;
  }

  pmu.disableCurrentLimitPin();

  if (!pmu.setChargeTargetVoltage(kChargeTargetVoltageMv)) {
    Serial.println(F("[BAT] Failed to set charge target voltage."));
    return false;
  }

  if (!pmu.setPrechargeCurr(kPrechargeCurrentMa)) {
    Serial.println(F("[BAT] Failed to set precharge current."));
    return false;
  }

  if (!pmu.setChargerConstantCurr(kFastChargeCurrentMa)) {
    Serial.println(F("[BAT] Failed to set fast charge current."));
    return false;
  }

  if (!configureTerminationAndRecharge()) {
    return false;
  }

  pmu.enableChargingTermination();
  pmu.enableChargingSafetyTimer();
  pmu.setFastChargeTimer(XPowersPPM::FAST_CHARGE_TIMER_12H);

  if (!pmu.enableMeasure()) {
    Serial.println(F("[BAT] Failed to enable SY6970 ADC measurement."));
    return false;
  }

  pmu.enableCharge();

  Serial.print(F("[BAT] SY6970 target/pre/fast/term/rechg: "));
  Serial.print(kChargeTargetVoltageMv);
  Serial.print(F("mV / "));
  Serial.print(kPrechargeCurrentMa);
  Serial.print(F("mA / "));
  Serial.print(kFastChargeCurrentMa);
  Serial.print(F("mA / "));
  Serial.print(kTerminationCurrentMa);
  Serial.print(F("mA / "));
  Serial.print(kRechargeThresholdOffsetMv);
  Serial.println(F("mV"));
  return true;
}

bool metricsChanged(const BatteryMetrics& a, const BatteryMetrics& b) {
  return a.soc != b.soc ||
         a.bqVoltageMv != b.bqVoltageMv ||
         a.ibatMa != b.ibatMa ||
         a.remainMah != b.remainMah ||
         a.fullMah != b.fullMah ||
         a.designMah != b.designMah ||
         a.soh != b.soh ||
         a.tempDeciC != b.tempDeciC ||
         a.tteMin != b.tteMin ||
         a.ttfMin != b.ttfMin ||
         a.syBattVoltageMv != b.syBattVoltageMv ||
         a.vbusMv != b.vbusMv ||
         a.vsysMv != b.vsysMv ||
         a.chargeCurrentMa != b.chargeCurrentMa ||
         a.busStatus != b.busStatus ||
         a.chargeStatus != b.chargeStatus ||
         a.chargeEnabled != b.chargeEnabled ||
         a.bqFullChargeDetected != b.bqFullChargeDetected ||
         a.isDischarging != b.isDischarging ||
         a.vbusPresent != b.vbusPresent;
}

UiState evaluateObservedUiState(const BatteryMetrics& sample) {
  if (bqConfigWarning) {
    return UiState::BqConfigWarn;
  }

  const auto chargeStatus = static_cast<XPowersPPM::ChargeStatus>(sample.chargeStatus);
  const bool externalPower = sample.vbusPresent &&
                             static_cast<XPowersPPM::BusStatus>(sample.busStatus) != XPowersPPM::BUS_STATE_OTG;
  const bool bqChargeDone = sample.bqFullChargeDetected || sample.soc >= kChargeDoneSocThreshold;
  const bool activelyCharging = chargeStatus == XPowersPPM::CHARGE_STATE_FAST_CHARGE ||
                                chargeStatus == XPowersPPM::CHARGE_STATE_PRE_CHARGE ||
                                sample.ibatMa > kChargeCurrentIntoBatteryThresholdMa;
  const bool chargeDoneLikely = sample.chargeEnabled &&
                                bqChargeDone &&
                                sample.ibatMa > kDischargeCurrentThresholdMa;

  if (chargeStatus == XPowersPPM::CHARGE_STATE_DONE && bqChargeDone) {
    return UiState::ChargeDone;
  }

  if (externalPower) {
    if (activelyCharging) {
      return UiState::Charging;
    }
    if (chargeDoneLikely) {
      return UiState::ChargeDone;
    }
    return UiState::Idle;
  }

  if (sample.ibatMa <= kDischargeCurrentThresholdMa || sample.isDischarging) {
    return UiState::Discharging;
  }
  return UiState::Idle;
}

void updateUiState(const UiState observedState) {
  const unsigned long now = millis();

  if (uiState == UiState::Init || observedState == UiState::BqConfigWarn || observedState == UiState::Error) {
    pendingUiState = observedState;
    pendingUiStateSinceMs = 0;
    if (uiState != observedState) {
      uiState = observedState;
      screenDirty = true;
    }
    return;
  }

  if (observedState == uiState) {
    pendingUiState = observedState;
    pendingUiStateSinceMs = 0;
    return;
  }

  if (observedState != pendingUiState) {
    pendingUiState = observedState;
    pendingUiStateSinceMs = now;
    return;
  }

  if (pendingUiStateSinceMs && (now - pendingUiStateSinceMs) >= kUiStateDebounceMs) {
    uiState = observedState;
    pendingUiStateSinceMs = 0;
    screenDirty = true;
  }
}

bool maybeRestartChargeTopOff() {
  if (!metrics.vbusPresent || !metrics.chargeEnabled) {
    return false;
  }

  const auto chargeStatus = static_cast<XPowersPPM::ChargeStatus>(metrics.chargeStatus);
  if (chargeStatus != XPowersPPM::CHARGE_STATE_DONE &&
      chargeStatus != XPowersPPM::CHARGE_STATE_NO_CHARGE) {
    return false;
  }

  if (metrics.bqFullChargeDetected || metrics.soc >= kChargeDoneSocThreshold) {
    return false;
  }

  if (metrics.chargeCurrentMa > kChargeCurrentIntoBatteryThresholdMa) {
    return false;
  }

  if (lastChargeTopOffAttemptMs != 0 &&
      (millis() - lastChargeTopOffAttemptMs) < kChargeTopOffRetryMs) {
    return false;
  }

  if (effectiveBatteryVoltageMv(metrics) + kChargeTopOffRestartMarginMv >= kChargeTargetVoltageMv) {
    return false;
  }

  lastChargeTopOffAttemptMs = millis();
  Serial.print(F("[BAT] Restarting charge top-off at "));
  Serial.print(effectiveBatteryVoltageMv(metrics));
  Serial.println(F(" mV because BQ is not full yet."));
  pmu.disableCharge();
  delay(20);
  pmu.enableCharge();
  setTransientDetail("Charge top-off restart");
  return true;
}

bool applyDynamicInputCurrentLimit() {
  const auto bus = static_cast<XPowersPPM::BusStatus>(metrics.busStatus);

  int16_t desired = -1;
  switch (bus) {
    case XPowersPPM::BUS_STATE_USB_SDP:
      desired = kInputCurrentSdpMa;
      break;
    case XPowersPPM::BUS_STATE_USB_CDP:
    case XPowersPPM::BUS_STATE_USB_DCP:
    case XPowersPPM::BUS_STATE_HVDCP:
    case XPowersPPM::BUS_STATE_ADAPTER:
    case XPowersPPM::BUS_STATE_NO_STANDARD_ADAPTER:
      desired = kInputCurrentAdapterMa;
      break;
    case XPowersPPM::BUS_STATE_NOINPUT:
    case XPowersPPM::BUS_STATE_OTG:
      desired = -1;
      break;
  }

  if (desired < 0) {
    if (appliedInputCurrentMa != -1) {
      appliedInputCurrentMa = -1;
      Serial.println(F("[BAT] No VBUS input current limit applied."));
    }
    return true;
  }

  if (desired == appliedInputCurrentMa) {
    return true;
  }

  if (!pmu.setInputCurrentLimit(desired)) {
    Serial.print(F("[BAT] Failed to set input current limit to "));
    Serial.println(desired);
    return false;
  }

  appliedInputCurrentMa = desired;
  Serial.print(F("[BAT] Input current limit set to "));
  Serial.print(desired);
  Serial.print(F(" mA for "));
  Serial.println(busStatusLabel(metrics.busStatus));
  return true;
}

bool refreshMetrics() {
  if (!gauge.refresh()) {
    Serial.println(F("[BAT] BQ27220 refresh failed."));
    setTransientDetail("BQ refresh failed");
    return false;
  }

  const bool hadMetrics = hasMetrics;
  BatteryMetrics next{};
  const BatteryStatus batteryStatus = gauge.getBatteryStatus();

  next.soc = gauge.getStateOfCharge();
  next.bqVoltageMv = gauge.getVoltage();
  next.ibatMa = gauge.getCurrent();
  next.remainMah = gauge.getRemainingCapacity();
  next.fullMah = gauge.getFullChargeCapacity();
  next.designMah = gauge.getDesignCapacity();
  next.soh = gauge.getStateOfHealth();
  next.tempDeciC = static_cast<int16_t>(lroundf(gauge.getTemperature() * 10.0f));
  next.tteMin = gauge.getTimeToEmpty();
  next.ttfMin = gauge.getTimeToFull();
  next.syBattVoltageMv = pmu.getBattVoltage();
  next.vbusMv = pmu.getVbusVoltage();
  next.vsysMv = pmu.getSystemVoltage();
  next.chargeCurrentMa = pmu.getChargeCurrent();
  next.busStatus = static_cast<uint8_t>(pmu.getBusStatus());
  next.chargeStatus = static_cast<uint8_t>(pmu.chargeStatus());
  next.chargeEnabled = pmu.isEnableCharge();
  next.bqFullChargeDetected = batteryStatus.isFullChargeDetected();
  next.isDischarging = batteryStatus.isInDischargeMode();
  next.vbusPresent = static_cast<XPowersPPM::BusStatus>(next.busStatus) != XPowersPPM::BUS_STATE_NOINPUT &&
                     static_cast<XPowersPPM::BusStatus>(next.busStatus) != XPowersPPM::BUS_STATE_OTG;

  metrics = next;
  hasMetrics = true;

  if (!applyDynamicInputCurrentLimit()) {
    setTransientDetail("SY6970 input-limit update failed");
  }

  (void)maybeRestartChargeTopOff();
  updateUiState(evaluateObservedUiState(metrics));

  if (!hadMetrics || !hasDrawnMetrics || metricsChanged(metrics, lastDrawnMetrics)) {
    screenDirty = true;
  }

  return true;
}

void updateUserButton() {
  const bool rawPressed = (digitalRead(userButton.pin) == LOW);
  const uint32_t now = millis();

  if (rawPressed != userButton.pressed && (now - userButton.lastChangeMs) >= kDebounceMs) {
    userButton.lastChangeMs = now;
    userButton.pressed = rawPressed;

    if (rawPressed) {
      userButton.pressedAtMs = now;
      userButton.longPressHandled = false;
    } else {
      userButton.longPressHandled = false;
    }
  }
}

void attemptShutdown(const __FlashStringHelper* source) {
  Serial.print(F("[BAT] Shutdown requested by "));
  Serial.println(source);

  if (hasExternalPower()) {
    Serial.println(F("[BAT] VBUS present, refusing shutdown. Unplug USB first."));
    setTransientDetail("VBUS present; unplug USB first");
    return;
  }

  setTransientDetail("Shutting down...");
  redrawScreen();
  delay(100);
  Serial.println(F("[BAT] SY6970 shutdown now."));
  pmu.shutdown();
  while (true) {
    delay(1000);
  }
}

void handleLongPressShutdown() {
  if (!userButton.pressed || userButton.longPressHandled) {
    return;
  }

  const uint32_t now = millis();
  if (now - userButton.pressedAtMs >= kShutdownHoldMs) {
    userButton.longPressHandled = true;
    attemptShutdown(F("USER KEY"));
  }
}

void handleCommand(const String& line) {
  String cmd = line;
  cmd.trim();
  cmd.toLowerCase();

  if (cmd.isEmpty()) {
    return;
  }

  if (cmd == "help") {
    printHelp();
    return;
  }

  if (cmd == "status") {
    printStatus();
    return;
  }

  if (cmd == "charge on") {
    pmu.enableCharge();
    Serial.println(F("[BAT] Charging enabled."));
    setTransientDetail("Charging enabled");
    refreshMetrics();
    return;
  }

  if (cmd == "charge off") {
    pmu.disableCharge();
    Serial.println(F("[BAT] Charging disabled."));
    setTransientDetail("Charging disabled");
    refreshMetrics();
    return;
  }

  if (cmd == "shutdown") {
    attemptShutdown(F("serial"));
    return;
  }

  Serial.print(F("[BAT] Unknown command: "));
  Serial.println(line);
  printHelp();
}

void pollSerial() {
  while (Serial.available() > 0) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      handleCommand(serialLine);
      serialLine = "";
      continue;
    }
    serialLine += ch;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("T-Embed BQ27220 + SY6970 battery test"));

  userButton.pin = BOARD_USER_KEY;
  pinMode(BOARD_USER_KEY, INPUT_PULLUP);

  if (!initDisplayPower()) {
    showFatalError(F("[BAT] Display power init failed."));
  }

  tft.init();
  tft.setRotation(kRotation);
  tft.fillScreen(TFT_BLACK);
  t_embed::board::deselectSharedSpiDevices();
  frameDrawn = false;

  uiState = UiState::Init;
  bqConfigDetail = "Initializing BQ27220 / SY6970";
  redrawScreen();

  if (!initGauge()) {
    showFatalError(F("[BAT] BQ27220 init failed."));
  }

  (void)configureGaugeCapacity();
  (void)configureGaugeChargeParameters();

  if (!configurePmu()) {
    showFatalError(F("[BAT] SY6970 config failed."));
  }

  if (!refreshMetrics()) {
    showFatalError(F("[BAT] Initial battery refresh failed."));
  }

  printHelp();
  printStatus();
}

void loop() {
  pollSerial();
  updateUserButton();
  handleLongPressShutdown();

  if (detailExpiresAtMs && millis() >= detailExpiresAtMs) {
    detailExpiresAtMs = 0;
    transientDetail = "";
    screenDirty = true;
  }

  if (currentDetail() != lastDrawnDetail) {
    screenDirty = true;
  }

  const unsigned long now = millis();
  if (now - lastPollAtMs >= kPollIntervalMs) {
    lastPollAtMs = now;
    (void)refreshMetrics();
  }

  if (screenDirty) {
    screenDirty = false;
    redrawScreen();
  }

  delay(5);
}
