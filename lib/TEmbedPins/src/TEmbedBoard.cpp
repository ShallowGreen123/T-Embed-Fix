#include <TEmbedBoard.h>

#include <math.h>

namespace t_embed {
namespace board {

void deselectSharedSpiDevices() {
  pinMode(BOARD_LCD_CS, OUTPUT);
  digitalWrite(BOARD_LCD_CS, HIGH);

  pinMode(BOARD_SD_CS, OUTPUT);
  digitalWrite(BOARD_SD_CS, HIGH);

  pinMode(BOARD_CC1101_CS, OUTPUT);
  digitalWrite(BOARD_CC1101_CS, HIGH);
}

void beginWire(TwoWire& wire) {
  wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
}

bool beginExpander(TEmbedXL9555& expander, TwoWire& wire, uint8_t address) {
  beginWire(wire);
  return expander.begin(wire, address);
}

bool setLowPowerEnabled(TEmbedXL9555& expander, bool enabled) {
  return expander.setOutput(BOARD_XL9555_LOW_PWR_EN, enabled);
}

bool setAudioAmplifierEnabled(TEmbedXL9555& expander, bool enabled) {
  return expander.setOutput(BOARD_XL9555_AP_EN, enabled);
}

bool setLcdReset(TEmbedXL9555& expander, bool high) {
  return expander.setOutput(BOARD_XL9555_LCD_RST, high);
}

bool setCc1101RfPath(TEmbedXL9555& expander, float frequency_mhz) {
  bool sw1 = false;
  bool sw0 = false;

  if (fabsf(frequency_mhz - 315.0f) < 0.5f) {
    sw1 = true;
    sw0 = false;
  } else if (fabsf(frequency_mhz - 434.0f) < 2.0f) {
    sw1 = true;
    sw0 = true;
  } else if ((fabsf(frequency_mhz - 868.0f) < 2.0f) || (fabsf(frequency_mhz - 915.0f) < 2.0f)) {
    sw1 = false;
    sw0 = true;
  } else {
    return false;
  }

  return expander.setOutput(BOARD_XL9555_CC_SW1, sw1) &&
         expander.setOutput(BOARD_XL9555_CC_SW0, sw0);
}

}  // namespace board
}  // namespace t_embed
