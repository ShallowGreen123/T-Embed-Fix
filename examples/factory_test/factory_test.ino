#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <SPI.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <TEmbedBoard.h>

// Forward-declare sub-page headers (included after enum/struct defs)
// Each header defines its namespace functions: init, update, render, deinit
void requestExitSubPage();
void requestSystemSleep();
String currentRotationLabel();
String autoSleepPresetLabel();
String autoDimTimeoutLabel();
void cycleDisplayRotation();
void cycleAutoSleepPreset();

// ---- Page IDs ----
enum class PageId : uint8_t {
    MainMenu = 0,
    Battery, CC1101, IR, Mic, NFC, SD, WiFi, TFT, WS2812, Setting,
    kCount
};
constexpr uint8_t kPageCount = static_cast<uint8_t>(PageId::kCount) - 1;  // excludes MainMenu

// ---- Per-page descriptor ----
struct PageDescriptor {
    const char* label;
    void (*init)();
    void (*update)();
    void (*render)();
    void (*deinit)();
};

enum class AutoSleepPreset : uint8_t {
    Off = 0,
    Sec30,
    Min1,
    Min2,
    Min5,
};

struct FactorySettings {
    uint8_t rotation = 3;
    AutoSleepPreset autoSleepPreset = AutoSleepPreset::Off;
};

// ---- Shared globals (accessible to all page headers) ----
TEmbedXL9555 ioExpander;
TFT_eSPI     tft;
TFT_eSprite  gMainMenuCanvas(&tft);
Preferences  gPrefs;
FactorySettings gSettings;
bool         gPrefsReady = false;
bool         gMainMenuCanvasReady = false;
uint32_t     gMainMenuLastDrawMs = 0;

// ---- Button state ----
struct Btn {
    uint8_t  pin;
    bool     pressed      = false;
    bool     event        = false;
    uint32_t lastChangeMs = 0;
};

// ---- Factory state ----
struct FactoryState {
    PageId   activePage            = PageId::MainMenu;
    int8_t   menuCursor            = 0;
    bool     menuDirty             = true;
    bool     subPageExitRequested  = false;
    bool     systemSleepRequested  = false;
    bool     backlightDimmed       = false;
    volatile int32_t encRaw        = 0;
    int32_t           encLast      = 0;
    int32_t           encActivitySnapshot = 0;
    uint32_t          lastUserInputMs = 0;
    volatile uint8_t  encPrevAB    = 0;
    Btn encBtn;
    Btn usrBtn;
} g;

// ---- Include sub-pages (after shared globals are declared) ----
#include "page_battery.h"
#include "page_cc1101.h"
#include "page_ir.h"
#include "page_mic.h"
#include "page_nfc.h"
#include "page_sd.h"
#include "page_wifi.h"
#include "page_tft.h"
#include "page_ws2812.h"
#include "page_setting.h"
#include "initial_page.h"

// ---- Page descriptor table (index 0 = Battery = PageId::Battery - 1) ----
static const PageDescriptor kPages[] = {
    { "Battery / PMU", page_battery::init, page_battery::update, page_battery::render, page_battery::deinit },
    { "CC1101 Radio",  page_cc1101::init,  page_cc1101::update,  page_cc1101::render,  page_cc1101::deinit  },
    { "IR TX / RX",    page_ir::init,      page_ir::update,      page_ir::render,      page_ir::deinit      },
    { "Microphone",    page_mic::init,     page_mic::update,     page_mic::render,     page_mic::deinit     },
    { "NFC ST25R3916", page_nfc::init,     page_nfc::update,     page_nfc::render,     page_nfc::deinit     },
    { "SD Card",       page_sd::init,      page_sd::update,      page_sd::render,      page_sd::deinit      },
    { "WiFi",          page_wifi::init,    page_wifi::update,    page_wifi::render,    page_wifi::deinit    },
    { "TFT Display",   page_tft::init,     page_tft::update,     page_tft::render,     page_tft::deinit     },
    { "WS2812 LEDs",   page_ws2812::init,  page_ws2812::update,  page_ws2812::render,  page_ws2812::deinit  },
    { "Settings",      page_setting::init, page_setting::update, page_setting::render, page_setting::deinit },
};

// ---- Encoder ISR / shared helpers ----
constexpr uint8_t  kEncA   = ENCODER_INA;
constexpr uint8_t  kEncB   = ENCODER_INB;
constexpr uint32_t kDebounceMs = 20;

constexpr uint8_t  kRotationLandscape        = 3;
constexpr uint8_t  kRotationReverseLandscape = 1;

constexpr uint8_t  kBacklightChannel    = 0;
constexpr uint8_t  kBacklightResolution = 8;
constexpr uint32_t kBacklightFreqHz     = 12000;
constexpr uint8_t  kBacklightBright     = 255;
constexpr uint8_t  kBacklightDim        = 72;
constexpr uint8_t  kBacklightOff        = 0;

constexpr uint32_t kMinDimLeadMs = 10000;

constexpr char kPrefsNamespace[]   = "factory";
constexpr char kPrefRotationKey[]  = "rotation";
constexpr char kPrefAutoSleepKey[] = "autosleep";

static const int8_t kEncTable[4][4] = {
    { 0, -1,  1,  0},
    { 1,  0,  0, -1},
    {-1,  0,  0,  1},
    { 0,  1, -1,  0},
};

uint8_t normalizeRotation(const uint8_t rotation)
{
    return rotation == kRotationReverseLandscape
        ? kRotationReverseLandscape
        : kRotationLandscape;
}

AutoSleepPreset normalizeAutoSleepPreset(const uint8_t raw)
{
    switch (raw) {
        case static_cast<uint8_t>(AutoSleepPreset::Sec30): return AutoSleepPreset::Sec30;
        case static_cast<uint8_t>(AutoSleepPreset::Min1):  return AutoSleepPreset::Min1;
        case static_cast<uint8_t>(AutoSleepPreset::Min2):  return AutoSleepPreset::Min2;
        case static_cast<uint8_t>(AutoSleepPreset::Min5):  return AutoSleepPreset::Min5;
        case static_cast<uint8_t>(AutoSleepPreset::Off):
        default:
            return AutoSleepPreset::Off;
    }
}

void saveRotationSetting()
{
    if (gPrefsReady) {
        gPrefs.putUChar(kPrefRotationKey, gSettings.rotation);
    }
}

void saveAutoSleepSetting()
{
    if (gPrefsReady) {
        gPrefs.putUChar(kPrefAutoSleepKey,
                        static_cast<uint8_t>(gSettings.autoSleepPreset));
    }
}

void releaseMainMenuCanvas()
{
    gMainMenuCanvas.deleteSprite();
    gMainMenuCanvasReady = false;
}

void initMainMenuCanvas()
{
    releaseMainMenuCanvas();
    gMainMenuCanvas.setColorDepth(16);
    gMainMenuCanvasReady = (gMainMenuCanvas.createSprite(tft.width(), tft.height()) != nullptr);
    gMainMenuLastDrawMs = 0;
    if (!gMainMenuCanvasReady) {
        Serial.println(F("[MAIN] Menu sprite allocation failed, using direct TFT redraw."));
    }
}

void loadFactorySettings()
{
    if (!gPrefs.begin(kPrefsNamespace, false)) {
        Serial.println(F("[MAIN] Preferences init failed, using defaults."));
        gPrefsReady = false;
        gSettings = FactorySettings{};
        return;
    }

    gPrefsReady = true;
    gSettings.rotation = normalizeRotation(
        gPrefs.getUChar(kPrefRotationKey, kRotationLandscape));
    gSettings.autoSleepPreset = normalizeAutoSleepPreset(
        gPrefs.getUChar(kPrefAutoSleepKey,
                        static_cast<uint8_t>(AutoSleepPreset::Off)));
}

void setBacklightBrightness(const uint8_t value)
{
    ledcWrite(kBacklightChannel, value);
}

void initBacklightControl()
{
    pinMode(BOARD_LCD_BL, OUTPUT);
    ledcSetup(kBacklightChannel, kBacklightFreqHz, kBacklightResolution);
    ledcAttachPin(BOARD_LCD_BL, kBacklightChannel);
    setBacklightBrightness(kBacklightBright);
    g.backlightDimmed = false;
}

void noteUserActivity()
{
    g.lastUserInputMs = millis();
    if (g.backlightDimmed) {
        g.backlightDimmed = false;
        setBacklightBrightness(kBacklightBright);
    }
}

String formatDurationLabel(const uint32_t ms)
{
    if (ms == 0) {
        return "Off";
    }

    const uint32_t totalSec = (ms + 500U) / 1000U;
    if (totalSec < 60U) {
        return String(totalSec) + "s";
    }
    if ((totalSec % 60U) == 0U) {
        return String(totalSec / 60U) + "m";
    }
    return String(totalSec / 60U) + "m " + String(totalSec % 60U) + "s";
}

uint32_t autoSleepTimeoutMs()
{
    switch (gSettings.autoSleepPreset) {
        case AutoSleepPreset::Sec30: return 30000UL;
        case AutoSleepPreset::Min1:  return 60000UL;
        case AutoSleepPreset::Min2:  return 120000UL;
        case AutoSleepPreset::Min5:  return 300000UL;
        case AutoSleepPreset::Off:
        default:
            return 0;
    }
}

uint32_t autoDimTimeoutMs()
{
    const uint32_t sleepMs = autoSleepTimeoutMs();
    if (sleepMs == 0) {
        return 0;
    }

    uint32_t dimMs = (sleepMs * 2UL) / 3UL;
    if ((sleepMs - dimMs) < kMinDimLeadMs) {
        dimMs = (sleepMs > kMinDimLeadMs)
            ? (sleepMs - kMinDimLeadMs)
            : (sleepMs / 2UL);
    }
    return dimMs;
}

String currentRotationLabel()
{
    return gSettings.rotation == kRotationLandscape
        ? "Landscape"
        : "Reverse";
}

String autoSleepPresetLabel()
{
    return formatDurationLabel(autoSleepTimeoutMs());
}

String autoDimTimeoutLabel()
{
    return formatDurationLabel(autoDimTimeoutMs());
}

void setDisplayRotation(const uint8_t rotation)
{
    const uint8_t next = normalizeRotation(rotation);
    if (gSettings.rotation == next) {
        return;
    }

    gSettings.rotation = next;
    saveRotationSetting();

    tft.setRotation(gSettings.rotation);
    tft.fillScreen(TFT_BLACK);
    t_embed::board::deselectSharedSpiDevices();

    if (g.activePage == PageId::MainMenu) {
        initMainMenuCanvas();
        g.menuDirty = true;
    }
}

void cycleDisplayRotation()
{
    setDisplayRotation(
        gSettings.rotation == kRotationLandscape
            ? kRotationReverseLandscape
            : kRotationLandscape);
}

void cycleAutoSleepPreset()
{
    switch (gSettings.autoSleepPreset) {
        case AutoSleepPreset::Off:
            gSettings.autoSleepPreset = AutoSleepPreset::Sec30;
            break;
        case AutoSleepPreset::Sec30:
            gSettings.autoSleepPreset = AutoSleepPreset::Min1;
            break;
        case AutoSleepPreset::Min1:
            gSettings.autoSleepPreset = AutoSleepPreset::Min2;
            break;
        case AutoSleepPreset::Min2:
            gSettings.autoSleepPreset = AutoSleepPreset::Min5;
            break;
        case AutoSleepPreset::Min5:
        default:
            gSettings.autoSleepPreset = AutoSleepPreset::Off;
            break;
    }
    saveAutoSleepSetting();
    noteUserActivity();
}

void prepareExpanderForSystemSleep()
{
    // Put the external connector control lines into a safe low state first.
    bool ok = true;
    ok &= ioExpander.setOutput(BOARD_XL9555_MOD_SEL, false);
    ok &= ioExpander.setOutput(BOARD_XL9555_EX_PWR_EN, false);
    ok &= ioExpander.setOutput(BOARD_XL9555_ESP32C5_RST, false);
    if (!ok) {
        Serial.println(F("[MAIN] Failed to pull J5 control lines low before sleep."));
    }
}

void requestSystemSleep()
{
    g.systemSleepRequested = true;
}

void enterSystemSleepNow()
{
    Serial.println(F("[MAIN] Entering deep sleep. Wake with USER key."));
    g.systemSleepRequested = false;

    if (g.activePage != PageId::MainMenu) {
        const uint8_t idx = static_cast<uint8_t>(g.activePage) - 1;
        if (idx < kPageCount) {
            kPages[idx].deinit();
        }
    }

    t_embed::board::deselectSharedSpiDevices();
    t_embed::board::setAudioAmplifierEnabled(ioExpander, false);
    setBacklightBrightness(kBacklightOff);
    delay(30);
    t_embed::board::setLcdReset(ioExpander, false);
    delay(10);
    prepareExpanderForSystemSleep();
    delay(2);
    t_embed::board::setLowPowerEnabled(ioExpander, false);

    pinMode(BOARD_USER_KEY, INPUT_PULLUP);
    rtc_gpio_pullup_en(static_cast<gpio_num_t>(BOARD_USER_KEY));
    rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(BOARD_USER_KEY));
    esp_sleep_enable_ext1_wakeup(1ULL << BOARD_USER_KEY, ESP_EXT1_WAKEUP_ANY_LOW);

    Serial.flush();
    delay(20);
    esp_deep_sleep_start();
}

void trackUserActivity()
{
    if (g.encRaw != g.encActivitySnapshot) {
        g.encActivitySnapshot = g.encRaw;
        noteUserActivity();
    }

    if (g.encBtn.event || g.usrBtn.event) {
        noteUserActivity();
    }
}

void handleIdlePowerState()
{
    const uint32_t sleepMs = autoSleepTimeoutMs();
    if (sleepMs == 0) {
        return;
    }

    const uint32_t idleMs = millis() - g.lastUserInputMs;
    const uint32_t dimMs = autoDimTimeoutMs();

    if (!g.backlightDimmed && dimMs > 0 && idleMs >= dimMs) {
        g.backlightDimmed = true;
        setBacklightBrightness(kBacklightDim);
    }

    if (idleMs >= sleepMs) {
        requestSystemSleep();
    }
}

void IRAM_ATTR onEncoderChange()
{
    const uint8_t a = digitalRead(kEncA);
    const uint8_t b = digitalRead(kEncB);
    const uint8_t cur = (a << 1) | b;
    g.encRaw += kEncTable[g.encPrevAB][cur];
    g.encPrevAB = cur;
}

void pollButton(Btn& btn)
{
    const bool raw = (digitalRead(btn.pin) == LOW);
    const uint32_t now = millis();
    if (raw != btn.pressed && (now - btn.lastChangeMs) >= kDebounceMs) {
        btn.lastChangeMs = now;
        btn.pressed = raw;
        if (raw) {
            btn.event = true;
        }
    }
}

// ---- Main menu rendering ----
#include "main_menu_ui.h"

void renderMenu()
{
    const uint32_t now = millis();
    if (gMainMenuLastDrawMs != 0 &&
        (now - gMainMenuLastDrawMs) < kMenuFrameIntervalMs) {
        return;
    }

    if (gMainMenuCanvasReady) {
        drawMenuUi(gMainMenuCanvas);
        gMainMenuCanvas.pushSprite(0, 0);
    } else {
        drawMenuUi(tft);
    }

    t_embed::board::deselectSharedSpiDevices();
    g.menuDirty = false;
    gMainMenuLastDrawMs = now;
}

void enterSubPage(PageId id)
{
    const uint8_t idx = static_cast<uint8_t>(id) - 1;
    if (idx >= kPageCount) {
        return;
    }

    g.activePage = id;
    g.subPageExitRequested = false;
    g.encLast = g.encRaw;
    noteUserActivity();
    releaseMainMenuCanvas();
    kPages[idx].init();
    if (!g.subPageExitRequested) {
        kPages[idx].render();
    }
}

void exitSubPage()
{
    const uint8_t idx = static_cast<uint8_t>(g.activePage) - 1;
    if (idx < kPageCount) {
        kPages[idx].deinit();
    }

    g.activePage = PageId::MainMenu;
    g.menuDirty = true;
    g.subPageExitRequested = false;
    g.encLast = g.encRaw;
    noteUserActivity();
    initMainMenuCanvas();
    renderMenu();
}

// ---- Sub-page navigation ----
void requestExitSubPage()
{
    g.subPageExitRequested = true;
}

// ---- setup() ----
void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println(F("\nT-Embed CC1101 Factory Test"));
    loadFactorySettings();

    const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
    if (wakeCause == ESP_SLEEP_WAKEUP_EXT1) {
        Serial.println(F("[MAIN] Wakeup source: USER key."));
    }

    g.encBtn.pin = ENCODER_KEY;
    g.usrBtn.pin = BOARD_USER_KEY;
    pinMode(g.encBtn.pin, INPUT_PULLUP);
    pinMode(g.usrBtn.pin, INPUT_PULLUP);
    pinMode(kEncA, INPUT_PULLUP);
    pinMode(kEncB, INPUT_PULLUP);
    g.encBtn.pressed = (digitalRead(g.encBtn.pin) == LOW);
    g.encBtn.lastChangeMs = millis();
    g.usrBtn.pressed = (digitalRead(g.usrBtn.pin) == LOW);
    g.usrBtn.lastChangeMs = millis();
    g.encPrevAB = ((digitalRead(kEncA) << 1) | digitalRead(kEncB));
    attachInterrupt(digitalPinToInterrupt(kEncA), onEncoderChange, CHANGE);
    attachInterrupt(digitalPinToInterrupt(kEncB), onEncoderChange, CHANGE);

    t_embed::board::deselectSharedSpiDevices();
    const bool expanderReady = t_embed::board::beginExpander(ioExpander);
    if (!expanderReady) {
        Serial.println(F("[MAIN] XL9555 init failed; showing initial hardware page."));
    }

    bool lowPowerReady = false;
    if (expanderReady) {
        lowPowerReady = t_embed::board::setLowPowerEnabled(ioExpander, true);
        if (!lowPowerReady) {
            Serial.println(F("[MAIN] LOW_PWR_3V3 failed; showing initial hardware page."));
        }
    }
    delay(20);

    initBacklightControl();

    if (expanderReady) {
        t_embed::board::setLcdReset(ioExpander, true);  delay(5);
        t_embed::board::setLcdReset(ioExpander, false); delay(20);
        t_embed::board::setLcdReset(ioExpander, true);  delay(120);
    }

    tft.init();
    tft.setRotation(gSettings.rotation);
    tft.fillScreen(TFT_BLACK);
    t_embed::board::deselectSharedSpiDevices();
    g.encLast = g.encRaw;
    g.encActivitySnapshot = g.encRaw;
    noteUserActivity();
    factory_initial_page::begin(expanderReady, lowPowerReady);
    factory_initial_page::render();
}

// ---- loop() ----
void loop()
{
    pollButton(g.encBtn);
    pollButton(g.usrBtn);
    trackUserActivity();

    if (factory_initial_page::isActive()) {
        factory_initial_page::update();
        if (factory_initial_page::isActive()) {
            factory_initial_page::render();
            delay(5);
            return;
        }

        initMainMenuCanvas();
        g.menuDirty = true;
        renderMenu();
    }

    if (g.activePage == PageId::MainMenu) {
        const int32_t cur = g.encRaw;
        const int32_t delta = (cur - g.encLast) / 2;
        if (delta != 0) {
            g.encLast += delta * 2;
            int32_t c = static_cast<int32_t>(g.menuCursor) + delta;
            c %= static_cast<int32_t>(kPageCount);
            if (c < 0) {
                c += kPageCount;
            }
            g.menuCursor = static_cast<int8_t>(c);
            g.menuDirty = true;
        }

        if (g.encBtn.event) {
            g.encBtn.event = false;
            enterSubPage(static_cast<PageId>(g.menuCursor + 1));
            return;
        }

        if (g.usrBtn.event) {
            g.usrBtn.event = false;
            g.menuCursor = (g.menuCursor + 1) % kPageCount;
            g.menuDirty = true;
        }

        if (g.menuCursor == static_cast<int8_t>(static_cast<uint8_t>(PageId::Battery) - 1) &&
            page_battery::updateMenuPreview()) {
            g.menuDirty = true;
        }

        if (g.menuDirty) {
            renderMenu();
        }
    } else {
        if (g.activePage != PageId::Battery &&
            g.activePage != PageId::CC1101 &&
            g.activePage != PageId::IR &&
            g.activePage != PageId::Mic &&
            g.activePage != PageId::NFC &&
            g.activePage != PageId::SD &&
            g.activePage != PageId::Setting &&
            g.activePage != PageId::TFT &&
            g.activePage != PageId::WS2812 &&
            g.activePage != PageId::WiFi &&
            g.usrBtn.event) {
            g.usrBtn.event = false;
            exitSubPage();
            return;
        }

        const uint8_t idx = static_cast<uint8_t>(g.activePage) - 1;
        kPages[idx].update();
        if (g.subPageExitRequested) {
            exitSubPage();
            return;
        }
        kPages[idx].render();
    }

    handleIdlePowerState();
    if (g.systemSleepRequested) {
        enterSystemSleepNow();
        return;
    }

    // NFC card emulation has millisecond-scale response deadlines after the
    // ST25R3916 automatic anti-collision sequence completes.
    delay(g.activePage == PageId::NFC ? 1 : (g.activePage == PageId::Mic ? 2 : 5));
}
