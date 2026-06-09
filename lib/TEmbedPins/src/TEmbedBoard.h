#pragma once

#include <Arduino.h>
#include <Wire.h>

#include <TEmbedPins.h>
#include <TEmbedXL9555.h>

namespace t_embed {
namespace board {

void deselectSharedSpiDevices();
void beginWire(TwoWire& wire = Wire);

bool beginExpander(TEmbedXL9555& expander, TwoWire& wire = Wire, uint8_t address = BOARD_I2C_XL9555);
bool setLowPowerEnabled(TEmbedXL9555& expander, bool enabled = true);
bool setAudioAmplifierEnabled(TEmbedXL9555& expander, bool enabled = true);
bool setLcdReset(TEmbedXL9555& expander, bool high);
bool setCc1101RfPath(TEmbedXL9555& expander, float frequency_mhz);

}  // namespace board
}  // namespace t_embed
