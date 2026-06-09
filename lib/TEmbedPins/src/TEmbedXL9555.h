#pragma once

#include <Arduino.h>
#include <Wire.h>

#include <TEmbedPins.h>

class TEmbedXL9555 {
 public:
  bool begin(TwoWire& wire = Wire, uint8_t address = BOARD_I2C_XL9555);

  bool pinMode(uint8_t pin, uint8_t mode);
  bool digitalWrite(uint8_t pin, bool level);
  int digitalRead(uint8_t pin);

  bool setOutput(uint8_t pin, bool level);

  uint16_t outputState() const { return output_state_; }
  uint16_t directionState() const { return direction_state_; }

 private:
  static constexpr uint8_t kRegInput0 = 0x00;
  static constexpr uint8_t kRegOutput0 = 0x02;
  static constexpr uint8_t kRegPolarity0 = 0x04;
  static constexpr uint8_t kRegConfig0 = 0x06;

  bool isValidPin(uint8_t pin) const;
  bool readPair(uint8_t start_register, uint16_t& value);
  bool writePair(uint8_t start_register, uint16_t value);

  TwoWire* wire_ = nullptr;
  uint8_t address_ = BOARD_I2C_XL9555;
  uint16_t output_state_ = 0x0000;
  uint16_t direction_state_ = 0xFFFF;
  bool initialized_ = false;
};
