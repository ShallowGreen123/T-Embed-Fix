#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <RadioLib.h>

#include <TEmbedBoard.h>

namespace {

constexpr float kDefaultFrequencyMHz = 434.0f;
constexpr float kBitRateKbps = 1.2f;
constexpr float kRxBandwidthKHz = 58.0f;
constexpr float kFrequencyDeviationKHz = 5.2f;
constexpr int8_t kOutputPowerDbm = 10;
constexpr bool kUseOok = true;
constexpr uint8_t kSyncWordHigh = 0x01;
constexpr uint8_t kSyncWordLow = 0x23;
constexpr uint32_t kBurstIntervalMs = 1000;
constexpr char kDefaultTxPrefix[] = "T-Embed CC1101";

enum class RadioMode : uint8_t {
  Receive,
  BurstTransmit,
};

SPIClass radioSPI(HSPI);
CC1101 radio = new Module(BOARD_CC1101_CS, BOARD_CC1101_GDO0, RADIOLIB_NC, BOARD_CC1101_GDO2, radioSPI);
TEmbedXL9555 ioExpander;

volatile bool packetReceived = false;

RadioMode currentMode = RadioMode::Receive;
float currentFrequencyMHz = kDefaultFrequencyMHz;
unsigned long lastBurstAtMs = 0;
uint32_t burstCounter = 0;
String burstPrefix = kDefaultTxPrefix;

#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void onPacketReceived() {
  packetReceived = true;
}

const __FlashStringHelper* modeLabel(RadioMode mode) {
  return (mode == RadioMode::Receive) ? F("RX") : F("TX-BURST");
}

void printHelp() {
  Serial.println();
  Serial.println(F("CC1101 send/receive test commands:"));
  Serial.println(F("  help              - show this help"));
  Serial.println(F("  status            - show current radio settings"));
  Serial.println(F("  rx                - enter receive mode"));
  Serial.println(F("  tx                - send a test packet every second"));
  Serial.println(F("  send <text>       - send one packet immediately"));
  Serial.println(F("  freq 315|434|868|915 - switch frequency and re-init radio"));
  Serial.println(F("  prefix <text>     - change periodic TX message prefix"));
  Serial.println();
}

void printStatus() {
  Serial.println();
  Serial.print(F("[CC1101] Mode:        "));
  Serial.println(modeLabel(currentMode));
  Serial.print(F("[CC1101] Frequency:   "));
  Serial.print(currentFrequencyMHz, 1);
  Serial.println(F(" MHz"));
  Serial.print(F("[CC1101] Modulation:  "));
  Serial.println(kUseOok ? F("OOK") : F("2-FSK"));
  Serial.print(F("[CC1101] Bit rate:    "));
  Serial.print(kBitRateKbps, 1);
  Serial.println(F(" kbps"));
  Serial.print(F("[CC1101] RX BW:       "));
  Serial.print(kRxBandwidthKHz, 1);
  Serial.println(F(" kHz"));
  Serial.print(F("[CC1101] Freq dev:    "));
  Serial.print(kFrequencyDeviationKHz, 1);
  Serial.println(F(" kHz"));
  Serial.print(F("[CC1101] Sync word:   0x"));
  Serial.print(kSyncWordHigh, HEX);
  Serial.print(F(" 0x"));
  Serial.println(kSyncWordLow, HEX);
  Serial.print(F("[CC1101] TX prefix:   "));
  Serial.println(burstPrefix);
}

bool applyRadioSettings() {
  int state = radio.begin(currentFrequencyMHz);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[CC1101] radio.begin failed, code "));
    Serial.println(state);
    return false;
  }

  state = radio.setFrequency(currentFrequencyMHz);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[CC1101] setFrequency failed, code "));
    Serial.println(state);
    return false;
  }

  state = radio.setOOK(kUseOok);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[CC1101] setOOK failed, code "));
    Serial.println(state);
    return false;
  }

  state = radio.setBitRate(kBitRateKbps);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[CC1101] setBitRate failed, code "));
    Serial.println(state);
    return false;
  }

  state = radio.setRxBandwidth(kRxBandwidthKHz);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[CC1101] setRxBandwidth failed, code "));
    Serial.println(state);
    return false;
  }

  state = radio.setFrequencyDeviation(kFrequencyDeviationKHz);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[CC1101] setFrequencyDeviation failed, code "));
    Serial.println(state);
    return false;
  }

  state = radio.setOutputPower(kOutputPowerDbm);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[CC1101] setOutputPower failed, code "));
    Serial.println(state);
    return false;
  }

  state = radio.setSyncWord(kSyncWordHigh, kSyncWordLow);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[CC1101] setSyncWord failed, code "));
    Serial.println(state);
    return false;
  }

  return true;
}

bool initRadio() {
  t_embed::board::deselectSharedSpiDevices();

  if (!t_embed::board::beginExpander(ioExpander)) {
    Serial.println(F("[CC1101] XL9555 init failed."));
    return false;
  }

  if (!t_embed::board::setLowPowerEnabled(ioExpander, true)) {
    Serial.println(F("[CC1101] Failed to enable LOW_PWR_3V3."));
    return false;
  }

  if (!t_embed::board::setCc1101RfPath(ioExpander, currentFrequencyMHz)) {
    Serial.println(F("[CC1101] Unsupported frequency for board RF switch. Use 315/434/868/915."));
    return false;
  }

  radioSPI.begin(BOARD_CC1101_SCK, BOARD_CC1101_MISO, BOARD_CC1101_MOSI);
  delay(20);

  Serial.print(F("[CC1101] Initializing at "));
  Serial.print(currentFrequencyMHz, 1);
  Serial.println(F(" MHz ..."));

  return applyRadioSettings();
}

bool enterReceiveMode() {
  currentMode = RadioMode::Receive;
  packetReceived = false;

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
  currentMode = RadioMode::BurstTransmit;
  lastBurstAtMs = 0;

  radio.clearPacketReceivedAction();
  radio.clearPacketSentAction();
  (void)radio.finishReceive();
  (void)radio.standby();

  Serial.println(F("[CC1101] Mode switched to TX burst. Sending once per second."));
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
  radio.clearPacketReceivedAction();
  radio.clearPacketSentAction();
  (void)radio.finishReceive();
  (void)radio.finishTransmit();
  (void)radio.sleep();
  delay(10);

  if (!initRadio()) {
    return false;
  }

  if (currentMode == RadioMode::Receive) {
    return enterReceiveMode();
  }

  enterBurstTransmitMode();
  return true;
}

void handleReceivedPacket() {
  if (!packetReceived) {
    return;
  }

  packetReceived = false;

  String payload;
  int state = radio.readData(payload);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("[CC1101] RX packet received."));
    Serial.print(F("[CC1101] Data: "));
    Serial.println(payload);
    Serial.print(F("[CC1101] RSSI: "));
    Serial.print(radio.getRSSI());
    Serial.println(F(" dBm"));
    Serial.print(F("[CC1101] LQI:  "));
    Serial.println(radio.getLQI());
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

bool parseFrequency(const String& input, float& frequencyMHz) {
  String trimmed = input;
  trimmed.trim();

  if (trimmed.equals("315")) {
    frequencyMHz = 315.0f;
    return true;
  }
  if (trimmed.equals("434")) {
    frequencyMHz = 434.0f;
    return true;
  }
  if (trimmed.equals("868")) {
    frequencyMHz = 868.0f;
    return true;
  }
  if (trimmed.equals("915")) {
    frequencyMHz = 915.0f;
    return true;
  }

  return false;
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

  if (line.equalsIgnoreCase("rx")) {
    (void)enterReceiveMode();
    return;
  }

  if (line.equalsIgnoreCase("tx")) {
    enterBurstTransmitMode();
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
    float newFrequency = 0.0f;
    if (!parseFrequency(line.substring(5), newFrequency)) {
      Serial.println(F("[CC1101] Unsupported frequency. Use 315, 434, 868 or 915."));
      return;
    }

    currentFrequencyMHz = newFrequency;
    if (reinitializeRadioForCurrentMode()) {
      Serial.print(F("[CC1101] Frequency switched to "));
      Serial.print(currentFrequencyMHz, 1);
      Serial.println(F(" MHz."));
    }
    return;
  }

  Serial.print(F("[CC1101] Unknown command: "));
  Serial.println(line);
  printHelp();
}

void pollSerialCommands() {
  if (!Serial.available()) {
    return;
  }

  String line = Serial.readStringUntil('\n');
  handleCommand(line);
}

void handleBurstTransmit() {
  if (currentMode != RadioMode::BurstTransmit) {
    return;
  }

  const unsigned long now = millis();
  if ((lastBurstAtMs != 0U) && (now - lastBurstAtMs < kBurstIntervalMs)) {
    return;
  }

  lastBurstAtMs = now;
  String payload = burstPrefix + " #" + String(burstCounter++);
  (void)sendOnePacket(payload, false);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("T-Embed CC1101 send/receive test"));

  if (!initRadio()) {
    Serial.println(F("[CC1101] Radio init failed. Halting."));
    while (true) {
      delay(1000);
    }
  }

  if (!enterReceiveMode()) {
    Serial.println(F("[CC1101] Failed to enter RX mode. Halting."));
    while (true) {
      delay(1000);
    }
  }

  printStatus();
  printHelp();
}

void loop() {
  pollSerialCommands();
  handleReceivedPacket();
  handleBurstTransmit();
  delay(1);
}
