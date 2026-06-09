#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include <TEmbedPins.h>

namespace {

constexpr uint16_t kNumLeds = BOARD_WS2812_NUM_LEDS;
constexpr uint8_t kDataPin = BOARD_WS2812_DATA_PIN;
constexpr uint8_t kDefaultBrightness = 32;
constexpr uint32_t kFrameIntervalMs = 30;

Adafruit_NeoPixel strip(kNumLeds, kDataPin, NEO_GRB + NEO_KHZ800);

enum class DemoMode : uint8_t {
  Rainbow,
  ColorWipe,
  Theater,
  Breathing,
  Off,
};

const char* demoLabel(DemoMode mode) {
  switch (mode) {
    case DemoMode::Rainbow:   return "rainbow";
    case DemoMode::ColorWipe: return "color-wipe";
    case DemoMode::Theater:   return "theater-chase";
    case DemoMode::Breathing: return "breathing";
    case DemoMode::Off:       return "off";
  }
  return "?";
}

DemoMode currentMode = DemoMode::Rainbow;
uint8_t currentBrightness = kDefaultBrightness;
uint32_t frameCounter = 0;
unsigned long lastFrameAtMs = 0;

void printHelp() {
  Serial.println();
  Serial.println(F("WS2812 test commands:"));
  Serial.println(F("  help                 - show this help"));
  Serial.println(F("  status               - show current animation state"));
  Serial.println(F("  rainbow              - rolling rainbow animation"));
  Serial.println(F("  wipe                 - color wipe (R -> G -> B)"));
  Serial.println(F("  theater              - theater chase"));
  Serial.println(F("  breath               - white breathing"));
  Serial.println(F("  off                  - turn all LEDs off"));
  Serial.println(F("  bright <0-255>       - set brightness"));
  Serial.println(F("  fill <r> <g> <b>     - solid color (0-255 each)"));
  Serial.println();
}

void printStatus() {
  Serial.println();
  Serial.print(F("[WS2812] Pin:        GPIO"));
  Serial.println(kDataPin);
  Serial.print(F("[WS2812] LED count:  "));
  Serial.println(kNumLeds);
  Serial.print(F("[WS2812] Brightness: "));
  Serial.println(currentBrightness);
  Serial.print(F("[WS2812] Mode:       "));
  Serial.println(demoLabel(currentMode));
}

void clearAll() {
  strip.clear();
  strip.show();
}

void renderRainbow(uint32_t step) {
  for (uint16_t i = 0; i < kNumLeds; ++i) {
    uint16_t hue = static_cast<uint16_t>((i * 65536UL / kNumLeds + step * 256UL) & 0xFFFFu);
    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue)));
  }
  strip.show();
}

void renderColorWipe(uint32_t step) {
  const uint16_t cycle = kNumLeds * 3U;
  const uint16_t pos = step % cycle;
  const uint8_t band = pos / kNumLeds;
  const uint16_t head = pos % kNumLeds;

  uint32_t color = strip.Color(0, 0, 0);
  if (band == 0) color = strip.Color(255, 0, 0);
  else if (band == 1) color = strip.Color(0, 255, 0);
  else color = strip.Color(0, 0, 255);

  for (uint16_t i = 0; i <= head && i < kNumLeds; ++i) {
    strip.setPixelColor(i, color);
  }
  for (uint16_t i = head + 1; i < kNumLeds; ++i) {
    strip.setPixelColor(i, 0);
  }
  strip.show();
}

void renderTheaterChase(uint32_t step) {
  const uint8_t phase = step % 3U;
  for (uint16_t i = 0; i < kNumLeds; ++i) {
    if ((i + phase) % 3U == 0U) {
      strip.setPixelColor(i, strip.Color(255, 200, 0));
    } else {
      strip.setPixelColor(i, 0);
    }
  }
  strip.show();
}

void renderBreathing(uint32_t step) {
  const float angle = static_cast<float>(step) * 0.05f;
  const float wave = (sinf(angle) + 1.0f) * 0.5f;
  const uint8_t value = static_cast<uint8_t>(wave * 255.0f);
  const uint32_t color = strip.Color(value, value, value);
  for (uint16_t i = 0; i < kNumLeds; ++i) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void renderCurrentFrame() {
  switch (currentMode) {
    case DemoMode::Rainbow:   renderRainbow(frameCounter); break;
    case DemoMode::ColorWipe: renderColorWipe(frameCounter); break;
    case DemoMode::Theater:   renderTheaterChase(frameCounter); break;
    case DemoMode::Breathing: renderBreathing(frameCounter); break;
    case DemoMode::Off:       /* nothing to do */ break;
  }
}

void setMode(DemoMode mode) {
  currentMode = mode;
  frameCounter = 0;
  lastFrameAtMs = 0;
  if (mode == DemoMode::Off) {
    clearAll();
  }
  Serial.print(F("[WS2812] Mode -> "));
  Serial.println(demoLabel(mode));
}

void applySolidColor(uint8_t r, uint8_t g, uint8_t b) {
  currentMode = DemoMode::Off;  // disable animation
  for (uint16_t i = 0; i < kNumLeds; ++i) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
  Serial.print(F("[WS2812] Fill ("));
  Serial.print(r);
  Serial.print(F(", "));
  Serial.print(g);
  Serial.print(F(", "));
  Serial.print(b);
  Serial.println(F(")"));
}

bool parseFillCommand(const String& args, uint8_t& r, uint8_t& g, uint8_t& b) {
  int firstSpace = args.indexOf(' ');
  if (firstSpace < 0) return false;
  int secondSpace = args.indexOf(' ', firstSpace + 1);
  if (secondSpace < 0) return false;

  long rl = args.substring(0, firstSpace).toInt();
  long gl = args.substring(firstSpace + 1, secondSpace).toInt();
  long bl = args.substring(secondSpace + 1).toInt();

  if (rl < 0 || rl > 255 || gl < 0 || gl > 255 || bl < 0 || bl > 255) return false;

  r = static_cast<uint8_t>(rl);
  g = static_cast<uint8_t>(gl);
  b = static_cast<uint8_t>(bl);
  return true;
}

void handleCommand(String line) {
  line.trim();
  if (line.isEmpty()) return;

  if (line.equalsIgnoreCase("help"))    { printHelp();                       return; }
  if (line.equalsIgnoreCase("status"))  { printStatus();                     return; }
  if (line.equalsIgnoreCase("rainbow")) { setMode(DemoMode::Rainbow);        return; }
  if (line.equalsIgnoreCase("wipe"))    { setMode(DemoMode::ColorWipe);      return; }
  if (line.equalsIgnoreCase("theater")) { setMode(DemoMode::Theater);        return; }
  if (line.equalsIgnoreCase("breath"))  { setMode(DemoMode::Breathing);      return; }
  if (line.equalsIgnoreCase("off"))     { setMode(DemoMode::Off);            return; }

  if (line.startsWith("bright ")) {
    long v = line.substring(7).toInt();
    if (v < 0 || v > 255) {
      Serial.println(F("[WS2812] Brightness must be 0-255."));
      return;
    }
    currentBrightness = static_cast<uint8_t>(v);
    strip.setBrightness(currentBrightness);
    strip.show();
    Serial.print(F("[WS2812] Brightness -> "));
    Serial.println(currentBrightness);
    return;
  }

  if (line.startsWith("fill ")) {
    uint8_t r = 0, g = 0, b = 0;
    if (!parseFillCommand(line.substring(5), r, g, b)) {
      Serial.println(F("[WS2812] Usage: fill <r> <g> <b>  (each 0-255)"));
      return;
    }
    applySolidColor(r, g, b);
    return;
  }

  Serial.print(F("[WS2812] Unknown command: "));
  Serial.println(line);
  printHelp();
}

void pollSerialCommands() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  handleCommand(line);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("T-Embed WS2812 test"));

  strip.begin();
  strip.setBrightness(currentBrightness);
  strip.clear();
  strip.show();

  printStatus();
  printHelp();
}

void loop() {
  pollSerialCommands();

  const unsigned long now = millis();
  if (now - lastFrameAtMs >= kFrameIntervalMs) {
    lastFrameAtMs = now;
    renderCurrentFrame();
    ++frameCounter;
  }

  delay(1);
}
