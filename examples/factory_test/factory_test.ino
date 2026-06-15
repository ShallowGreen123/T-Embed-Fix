#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <SPI.h>
#include <TEmbedBoard.h>

// ---- WiFi credentials (edit before flashing) ----
constexpr char kWifiSsid[]     = "LilyGo-AABB";
constexpr char kWifiPassword[] = "xinyuandianzi";

// Forward-declare sub-page headers (included after enum/struct defs)
// Each header defines its namespace functions: init, update, render, deinit
void requestExitSubPage();

// ---- Page IDs ----
enum class PageId : uint8_t {
    MainMenu = 0,
    Battery, CC1101, Encoder, IR, Mic, NFC, SD, WiFi, WS2812, Setting,
    kCount
};
constexpr uint8_t kPageCount = (uint8_t)PageId::kCount - 1;  // excludes MainMenu

// ---- Per-page descriptor ----
struct PageDescriptor {
    const char* label;
    void (*init)();
    void (*update)();
    void (*render)();
    void (*deinit)();
};

// ---- Shared globals (accessible to all page headers) ----
TEmbedXL9555 ioExpander;
TFT_eSPI     tft;

// ---- Button state ----
struct Btn {
    uint8_t  pin;
    bool     pressed     = false;
    bool     event       = false;
    uint32_t lastChangeMs = 0;
};

// ---- Factory state ----
struct FactoryState {
    PageId   activePage   = PageId::MainMenu;
    int8_t   menuCursor   = 0;
    bool     menuDirty    = true;
    bool     subPageExitRequested = false;
    volatile int32_t encRaw    = 0;
    int32_t           encLast  = 0;
    volatile uint8_t  encPrevAB = 0;
    Btn encBtn;
    Btn usrBtn;
} g;

// ---- Include sub-pages (after shared globals are declared) ----
#include "page_battery.h"
#include "page_cc1101.h"
#include "page_encoder.h"
#include "page_ir.h"
#include "page_mic.h"
#include "page_nfc.h"
#include "page_sd.h"
#include "page_wifi.h"
#include "page_ws2812.h"
#include "page_setting.h"

// ---- Page descriptor table (index 0 = Battery = PageId::Battery - 1) ----
static const PageDescriptor kPages[] = {
    { "Battery / PMU", page_battery::init, page_battery::update, page_battery::render, page_battery::deinit },
    { "CC1101 Radio",  page_cc1101::init,  page_cc1101::update,  page_cc1101::render,  page_cc1101::deinit  },
    { "Encoder / Keys",page_encoder::init, page_encoder::update, page_encoder::render, page_encoder::deinit },
    { "IR TX / RX",    page_ir::init,      page_ir::update,      page_ir::render,      page_ir::deinit      },
    { "Microphone",    page_mic::init,     page_mic::update,     page_mic::render,     page_mic::deinit     },
    { "NFC ST25R3916", page_nfc::init,     page_nfc::update,     page_nfc::render,     page_nfc::deinit     },
    { "SD Card",       page_sd::init,      page_sd::update,      page_sd::render,      page_sd::deinit      },
    { "WiFi",          page_wifi::init,    page_wifi::update,    page_wifi::render,    page_wifi::deinit    },
    { "WS2812 LEDs",   page_ws2812::init,  page_ws2812::update,  page_ws2812::render,  page_ws2812::deinit  },
    { "Device Info",   page_setting::init, page_setting::update, page_setting::render, page_setting::deinit },
};

// ---- Encoder ISR ----
namespace {

constexpr uint8_t kEncA   = ENCODER_INA;
constexpr uint8_t kEncB   = ENCODER_INB;
constexpr uint32_t kDebounceMs = 20;

static const int8_t kEncTable[4][4] = {
    { 0, -1,  1,  0},
    { 1,  0,  0, -1},
    {-1,  0,  0,  1},
    { 0,  1, -1,  0},
};

}  // namespace

void IRAM_ATTR onEncoderChange() {
    const uint8_t a = digitalRead(kEncA);
    const uint8_t b = digitalRead(kEncB);
    const uint8_t cur = (a << 1) | b;
    g.encRaw += kEncTable[g.encPrevAB][cur];
    g.encPrevAB = cur;
}

void pollButton(Btn& btn) {
    const bool raw = (digitalRead(btn.pin) == LOW);
    const uint32_t now = millis();
    if (raw != btn.pressed && (now - btn.lastChangeMs) >= kDebounceMs) {
        btn.lastChangeMs = now;
        btn.pressed = raw;
        if (raw) btn.event = true;
    }
}

// ---- Main menu rendering ----
// Layout (rotation=1 → 320×170):
//   Y=0..21    header
//   Y=22..151  10 rows × 13px
//   Y=152..169 footer
namespace {
constexpr uint16_t kMenuBg       = TFT_BLACK;
constexpr uint16_t kMenuHeader   = TFT_NAVY;
constexpr uint16_t kMenuFooter   = 0x2104;
constexpr uint16_t kMenuSelBg    = TFT_DARKCYAN;
constexpr uint16_t kMenuSelFg    = TFT_WHITE;
constexpr uint16_t kMenuItemFg   = TFT_LIGHTGREY;
constexpr int16_t  kRowH         = 13;
constexpr int16_t  kRowsY        = 22;
}  // namespace

void renderMenu() {
    const int16_t W = tft.width();

    // Header
    tft.fillRect(0, 0, W, 22, kMenuHeader);
    tft.setTextColor(TFT_WHITE, kMenuHeader);
    tft.drawCentreString("T-Embed Factory Test", W / 2, 4, 2);

    // Rows
    for (uint8_t i = 0; i < kPageCount; ++i) {
        const int16_t y = kRowsY + i * kRowH;
        if (g.menuCursor == (int8_t)i) {
            tft.fillRect(0, y, W, kRowH, kMenuSelBg);
            tft.setTextColor(kMenuSelFg, kMenuSelBg);
        } else {
            tft.fillRect(0, y, W, kRowH, kMenuBg);
            tft.setTextColor(kMenuItemFg, kMenuBg);
        }
        char buf[32];
        snprintf(buf, sizeof(buf), " %2u. %s", (unsigned)(i + 1), kPages[i].label);
        tft.drawString(buf, 4, y + 1, 1);
    }

    // Footer
    tft.fillRect(0, 152, W, 18, kMenuFooter);
    tft.setTextColor(TFT_DARKGREY, kMenuFooter);
    tft.drawCentreString("ENC=scroll  BTN=enter  USR=scroll", W / 2, 155, 1);

    g.menuDirty = false;
}

// ---- Sub-page navigation ----
void requestExitSubPage() {
    g.subPageExitRequested = true;
}

void enterSubPage(PageId id) {
    const uint8_t idx = (uint8_t)id - 1;
    if (idx >= kPageCount) return;
    g.activePage = id;
    g.subPageExitRequested = false;
    g.encLast = g.encRaw;
    tft.fillScreen(TFT_BLACK);
    kPages[idx].init();
}

void exitSubPage() {
    const uint8_t idx = (uint8_t)g.activePage - 1;
    if (idx < kPageCount) {
        kPages[idx].deinit();
    }
    g.activePage = PageId::MainMenu;
    g.menuDirty  = true;
    g.subPageExitRequested = false;
    g.encLast = g.encRaw;
    tft.fillScreen(TFT_BLACK);
}

// ---- setup() ----
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println(F("\nT-Embed CC1101 Factory Test"));

    // Button & encoder GPIOs
    g.encBtn.pin = ENCODER_KEY;
    g.usrBtn.pin = BOARD_USER_KEY;
    pinMode(g.encBtn.pin, INPUT_PULLUP);
    pinMode(g.usrBtn.pin, INPUT_PULLUP);
    pinMode(kEncA, INPUT_PULLUP);
    pinMode(kEncB, INPUT_PULLUP);
    g.encPrevAB = ((digitalRead(kEncA) << 1) | digitalRead(kEncB));
    attachInterrupt(digitalPinToInterrupt(kEncA), onEncoderChange, CHANGE);
    attachInterrupt(digitalPinToInterrupt(kEncB), onEncoderChange, CHANGE);

    // Board power + display
    t_embed::board::deselectSharedSpiDevices();
    if (!t_embed::board::beginExpander(ioExpander)) {
        Serial.println(F("[MAIN] XL9555 init failed. Halting."));
        while (true) delay(1000);
    }
    if (!t_embed::board::setLowPowerEnabled(ioExpander, true)) {
        Serial.println(F("[MAIN] LOW_PWR_3V3 failed. Halting."));
        while (true) delay(1000);
    }
    delay(20);

    pinMode(BOARD_LCD_BL, OUTPUT);
    digitalWrite(BOARD_LCD_BL, HIGH);

    t_embed::board::setLcdReset(ioExpander, true);  delay(5);
    t_embed::board::setLcdReset(ioExpander, false); delay(20);
    t_embed::board::setLcdReset(ioExpander, true);  delay(120);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    t_embed::board::deselectSharedSpiDevices();

    renderMenu();
}

// ---- loop() ----
void loop() {
    pollButton(g.encBtn);
    pollButton(g.usrBtn);

    if (g.activePage == PageId::MainMenu) {
        // Encoder scroll
        const int32_t cur = g.encRaw;
        const int32_t delta = (cur - g.encLast) / 2;
        if (delta != 0) {
            g.encLast += delta * 2;
            int32_t c = (int32_t)g.menuCursor + delta;
            c %= (int32_t)kPageCount;
            if (c < 0) c += kPageCount;
            g.menuCursor = (int8_t)c;
            g.menuDirty = true;
        }
        // Encoder push → enter
        if (g.encBtn.event) {
            g.encBtn.event = false;
            enterSubPage((PageId)(g.menuCursor + 1));
            return;
        }
        // USR button → scroll down
        if (g.usrBtn.event) {
            g.usrBtn.event = false;
            g.menuCursor = (g.menuCursor + 1) % kPageCount;
            g.menuDirty = true;
        }
        if (g.menuDirty) renderMenu();
    } else {
        // USR button → back to menu
        if (g.activePage != PageId::Battery &&
            g.activePage != PageId::CC1101 &&
            g.usrBtn.event) {
            g.usrBtn.event = false;
            exitSubPage();
            return;
        }
        const uint8_t idx = (uint8_t)g.activePage - 1;
        kPages[idx].update();
        if (g.subPageExitRequested) {
            exitSubPage();
            return;
        }
        kPages[idx].render();
    }

    delay(5);
}
