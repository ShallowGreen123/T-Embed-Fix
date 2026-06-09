#include <TEmbedXL9555.h>

namespace {

constexpr uint8_t kXl9555PinCount = 16;

}  // namespace

bool TEmbedXL9555::begin(TwoWire& wire, uint8_t address) {
  wire_ = &wire;
  address_ = address;

  if (!readPair(kRegConfig0, direction_state_)) {
    return false;
  }

  if (!readPair(kRegOutput0, output_state_)) {
    return false;
  }

  if (!writePair(kRegPolarity0, 0x0000)) {
    return false;
  }

  initialized_ = true;
  return true;
}

bool TEmbedXL9555::pinMode(uint8_t pin, uint8_t mode) {
  if (!initialized_ || !isValidPin(pin)) {
    return false;
  }

  const uint16_t bit = static_cast<uint16_t>(1U << pin);
  const bool is_input = (mode != OUTPUT);

  if (is_input) {
    direction_state_ |= bit;
  } else {
    direction_state_ &= ~bit;
  }

  return writePair(kRegConfig0, direction_state_);
}

bool TEmbedXL9555::digitalWrite(uint8_t pin, bool level) {
  if (!initialized_ || !isValidPin(pin)) {
    return false;
  }

  const uint16_t bit = static_cast<uint16_t>(1U << pin);
  if (level) {
    output_state_ |= bit;
  } else {
    output_state_ &= ~bit;
  }

  return writePair(kRegOutput0, output_state_);
}

int TEmbedXL9555::digitalRead(uint8_t pin) {
  if (!initialized_ || !isValidPin(pin)) {
    return -1;
  }

  uint16_t input_state = 0x0000;
  if (!readPair(kRegInput0, input_state)) {
    return -1;
  }

  return (input_state & static_cast<uint16_t>(1U << pin)) ? HIGH : LOW;
}

bool TEmbedXL9555::setOutput(uint8_t pin, bool level) {
  return pinMode(pin, OUTPUT) && digitalWrite(pin, level);
}

bool TEmbedXL9555::isValidPin(uint8_t pin) const {
  return pin < kXl9555PinCount;
}

bool TEmbedXL9555::readPair(uint8_t start_register, uint16_t& value) {
  if (wire_ == nullptr) {
    return false;
  }

  wire_->beginTransmission(address_);
  wire_->write(start_register);
  if (wire_->endTransmission(false) != 0) {
    return false;
  }

  const uint8_t received = wire_->requestFrom(static_cast<int>(address_), 2);
  if (received != 2) {
    return false;
  }

  const uint8_t low = wire_->read();
  const uint8_t high = wire_->read();
  value = static_cast<uint16_t>(low | (static_cast<uint16_t>(high) << 8));
  return true;
}

bool TEmbedXL9555::writePair(uint8_t start_register, uint16_t value) {
  if (wire_ == nullptr) {
    return false;
  }

  wire_->beginTransmission(address_);
  wire_->write(start_register);
  wire_->write(static_cast<uint8_t>(value & 0xFF));
  wire_->write(static_cast<uint8_t>((value >> 8) & 0xFF));
  return wire_->endTransmission() == 0;
}
