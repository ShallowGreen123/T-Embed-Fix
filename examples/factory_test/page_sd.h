#pragma once
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>

namespace page_sd {

namespace {

constexpr uint32_t kUiFrameIntervalMs  = 33;
constexpr uint32_t kBusSettleMs        = 20;
constexpr uint32_t kSdPowerSettleMs    = 120;
constexpr uint32_t kSdMountFrequencies[] = {10000000UL, 4000000UL, 1000000UL};

constexpr int16_t kUiMargin     = 8;
constexpr int16_t kHeaderHeight = 24;
constexpr int16_t kFooterHeight = 18;
constexpr int16_t kBackBtnW     = 58;
constexpr int16_t kBackBtnH     = 14;

constexpr int16_t kStatusX = 8;
constexpr int16_t kStatusY = 34;
constexpr int16_t kStatusW = 304;
constexpr int16_t kStatusH = 38;

constexpr int16_t kStepY   = 80;
constexpr int16_t kStepW   = 72;
constexpr int16_t kStepH   = 22;
constexpr int16_t kStepGap = 8;

constexpr int16_t kMetricTopY    = 110;
constexpr int16_t kMetricBottomY = 132;
constexpr int16_t kMetricLeftX   = 8;
constexpr int16_t kMetricRightX  = 164;
constexpr int16_t kMetricW       = 148;
constexpr int16_t kMetricH       = 18;

constexpr uint16_t kColorBg        = 0x0841;
constexpr uint16_t kColorPanel     = 0x1082;
constexpr uint16_t kColorPanelEdge = 0x31A6;
constexpr uint16_t kColorCard      = 0x18C3;
constexpr uint16_t kColorPassBg    = 0x0A41;
constexpr uint16_t kColorFailBg    = 0x3006;

enum class StepState : uint8_t {
    Pending = 0,
    Pass,
    Fail,
};

enum class FocusItem : uint8_t {
    Summary = 0,
    Back,
    kCount,
};

struct SdTestSummary {
    StepState mount  = StepState::Pending;
    StepState write  = StepState::Pending;
    StepState read   = StepState::Pending;
    StepState result = StepState::Pending;

    uint32_t mountedFrequency = 0;
    uint8_t  cardType         = 0;
    bool     hasCardDetails   = false;
    uint64_t totalMB          = 0;
    uint64_t usedMB           = 0;
    uint16_t rootCount        = 0;
    bool     rootMeasured     = false;

    String title  = "Running SD diagnostics";
    String detail = "Checking mount, capacity and read/write path";
    String footer = "SD TEST WAIT";
};

TFT_eSprite gCanvas(&tft);
SdTestSummary gSummary;
FocusItem   gFocus        = FocusItem::Summary;
bool        gCanvasReady  = false;
bool        gScreenDirty  = true;
bool        gTestQueued   = false;
bool        gTestRunning  = false;
uint32_t    gLastUiDrawMs = 0;
int32_t     gEncSnapshot  = 0;

void markDirty()
{
    gScreenDirty = true;
}

void resetSummaryForRun()
{
    gSummary = SdTestSummary{};
    markDirty();
}

SPIClass& sharedSpi()
{
    // SD shares the same HSPI bus instance with the TFT on this board.
    return tft.getSPIinstance();
}

String cardTypeStr(const uint8_t type)
{
    switch (type) {
        case CARD_MMC:  return "MMC";
        case CARD_SD:   return "SD";
        case CARD_SDHC: return "SDHC";
        default:        return "UNKNOWN";
    }
}

const char* stepStateText(const StepState state)
{
    switch (state) {
        case StepState::Pass:    return "PASS";
        case StepState::Fail:    return "FAIL";
        case StepState::Pending:
        default:                 return "WAIT";
    }
}

uint16_t stepStateColor(const StepState state)
{
    switch (state) {
        case StepState::Pass:    return TFT_GREEN;
        case StepState::Fail:    return TFT_RED;
        case StepState::Pending:
        default:                 return TFT_DARKGREY;
    }
}

uint16_t statusAccentColor()
{
    switch (gSummary.result) {
        case StepState::Pass:    return TFT_GREEN;
        case StepState::Fail:    return TFT_RED;
        case StepState::Pending:
        default:                 return TFT_YELLOW;
    }
}

uint16_t statusFillColor()
{
    switch (gSummary.result) {
        case StepState::Pass:    return kColorPassBg;
        case StepState::Fail:    return kColorFailBg;
        case StepState::Pending:
        default:                 return kColorPanel;
    }
}

String mountFrequencyText()
{
    if (!gSummary.mountedFrequency) {
        return "-";
    }
    return String(gSummary.mountedFrequency / 1000000UL) + " MHz";
}

String cardTypeText()
{
    return gSummary.hasCardDetails ? cardTypeStr(gSummary.cardType) : String("-");
}

String usageText()
{
    if (!gSummary.hasCardDetails) {
        return "-";
    }
    return String(static_cast<uint32_t>(gSummary.usedMB)) + " / " +
           String(static_cast<uint32_t>(gSummary.totalMB)) + " MB";
}

String rootEntriesText()
{
    return gSummary.rootMeasured ? String(gSummary.rootCount) + " entries" : String("-");
}

const char* actionHintText()
{
    if (gFocus == FocusItem::Back) {
        return "BOOT=back";
    }
    return gTestRunning ? "TESTING..." : "USR=retest";
}

template <typename Canvas>
void drawBackButton(Canvas& gfx, const bool selected)
{
    const int16_t x = gfx.width() - kBackBtnW - 6;
    const int16_t y = gfx.height() - kFooterHeight + 2;
    const uint16_t bg = selected ? TFT_WHITE : TFT_DARKGREY;
    const uint16_t fg = selected ? TFT_BLACK : TFT_LIGHTGREY;

    gfx.fillRoundRect(x, y, kBackBtnW, kBackBtnH, 5, bg);
    gfx.drawRoundRect(x, y, kBackBtnW, kBackBtnH, 5,
                      selected ? TFT_YELLOW : 0x52AA);
    gfx.setTextColor(fg, bg);
    gfx.drawCentreString("BACK", x + kBackBtnW / 2, y + 3, 1);
}

template <typename Canvas>
void drawHeader(Canvas& gfx)
{
    gfx.fillRect(0, 0, gfx.width(), kHeaderHeight, TFT_NAVY);
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_WHITE, TFT_NAVY);
    gfx.drawString("SD Card Test", kUiMargin, 5, 2);

    gfx.setTextDatum(TR_DATUM);
    gfx.setTextColor(TFT_CYAN, TFT_NAVY);
    gfx.drawString("Shared SPI", gfx.width() - kUiMargin, 7, 1);
    gfx.setTextDatum(TL_DATUM);
}

template <typename Canvas>
void drawFooter(Canvas& gfx)
{
    const int16_t y = gfx.height() - kFooterHeight;
    gfx.fillRect(0, y, gfx.width(), kFooterHeight, TFT_DARKGREY);
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_WHITE, TFT_DARKGREY);
    gfx.drawString(gSummary.footer, 6, y + 4, 1);
    gfx.drawString(actionHintText(), 102, y + 4, 1);
    drawBackButton(gfx, gFocus == FocusItem::Back);
}

template <typename Canvas>
void drawStatusPanel(Canvas& gfx)
{
    const uint16_t accent = statusAccentColor();
    const uint16_t fill = statusFillColor();

    gfx.fillRoundRect(kStatusX, kStatusY, kStatusW, kStatusH, 8, fill);
    gfx.drawRoundRect(kStatusX, kStatusY, kStatusW, kStatusH, 8, accent);

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(accent, fill);
    gfx.drawString(gSummary.title, kStatusX + 10, kStatusY + 6, 2);

    gfx.fillRoundRect(kStatusX + kStatusW - 58, kStatusY + 7, 46, 16, 6, accent);
    gfx.setTextDatum(MC_DATUM);
    gfx.setTextColor(TFT_BLACK, accent);
    gfx.drawString(stepStateText(gSummary.result), kStatusX + kStatusW - 35, kStatusY + 15, 1);

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_LIGHTGREY, fill);
    gfx.drawString(gSummary.detail, kStatusX + 10, kStatusY + 24, 1);
}

template <typename Canvas>
void drawStepCard(Canvas& gfx, const int16_t x, const char* label, const StepState state)
{
    const uint16_t accent = stepStateColor(state);

    gfx.fillRoundRect(x, kStepY, kStepW, kStepH, 6, kColorCard);
    gfx.drawRoundRect(x, kStepY, kStepW, kStepH, 6, accent);

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_LIGHTGREY, kColorCard);
    gfx.drawString(label, x + 6, kStepY + 5, 1);

    gfx.setTextDatum(TR_DATUM);
    gfx.setTextColor(accent, kColorCard);
    gfx.drawString(stepStateText(state), x + kStepW - 6, kStepY + 5, 1);
    gfx.setTextDatum(TL_DATUM);
}

template <typename Canvas>
void drawMetricCard(Canvas& gfx,
                    const int16_t x,
                    const int16_t y,
                    const char* label,
                    const String& value,
                    const uint16_t borderColor)
{
    gfx.fillRoundRect(x, y, kMetricW, kMetricH, 6, kColorCard);
    gfx.drawRoundRect(x, y, kMetricW, kMetricH, 6, borderColor);

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_CYAN, kColorCard);
    gfx.drawString(label, x + 8, y + 5, 1);

    gfx.setTextColor(TFT_WHITE, kColorCard);
    gfx.drawString(value, x + 52, y + 5, 1);
}

template <typename Canvas>
void drawUi(Canvas& gfx)
{
    gfx.fillRect(0, 0, gfx.width(), gfx.height(), kColorBg);

    drawHeader(gfx);
    drawStatusPanel(gfx);

    drawStepCard(gfx, kUiMargin, "Mount", gSummary.mount);
    drawStepCard(gfx, kUiMargin + (kStepW + kStepGap), "Write", gSummary.write);
    drawStepCard(gfx, kUiMargin + (kStepW + kStepGap) * 2, "Read", gSummary.read);
    drawStepCard(gfx, kUiMargin + (kStepW + kStepGap) * 3, "Result", gSummary.result);

    drawMetricCard(gfx, kMetricLeftX,  kMetricTopY,    "SPI",   mountFrequencyText(), kColorPanelEdge);
    drawMetricCard(gfx, kMetricRightX, kMetricTopY,    "Type",  cardTypeText(),       kColorPanelEdge);
    drawMetricCard(gfx, kMetricLeftX,  kMetricBottomY, "Usage", usageText(),          kColorPanelEdge);
    drawMetricCard(gfx, kMetricRightX, kMetricBottomY, "Root",  rootEntriesText(),    kColorPanelEdge);

    drawFooter(gfx);
}

void redrawScreen()
{
    if (gCanvasReady) {
        drawUi(gCanvas);
        gCanvas.pushSprite(0, 0);
    } else {
        drawUi(tft);
    }
    t_embed::board::deselectSharedSpiDevices();
}

uint16_t listRootToSerial()
{
    File root = SD.open("/");
    if (!root) {
        Serial.println(F("[SD] Failed to open root directory."));
        return 0;
    }

    Serial.println(F("[SD] Root directory:"));
    uint16_t count = 0;
    File entry = root.openNextFile();
    while (entry) {
        String name = String(entry.name());
        if (entry.isDirectory()) {
            name = "[" + name + "]";
        }
        Serial.print(F("  "));
        Serial.println(name);
        entry.close();
        entry = root.openNextFile();
        ++count;
    }

    if (!count) {
        Serial.println(F("  (empty)"));
    }

    root.close();
    return count;
}

bool mountSdCard(uint32_t& mountedFrequency)
{
    pinMode(BOARD_SD_CS, OUTPUT);
    digitalWrite(BOARD_SD_CS, HIGH);
    delay(kSdPowerSettleMs);

    for (const uint32_t frequency : kSdMountFrequencies) {
        SD.end();
        t_embed::board::deselectSharedSpiDevices();
        delay(kBusSettleMs);

        Serial.print(F("[SD] Mount attempt @ "));
        Serial.print(frequency / 1000000UL);
        Serial.println(F(" MHz"));

        if (SD.begin(BOARD_SD_CS, sharedSpi(), frequency)) {
            mountedFrequency = frequency;
            return true;
        }
    }

    mountedFrequency = 0;
    return false;
}

void runSdTest()
{
    gTestRunning = true;
    t_embed::board::deselectSharedSpiDevices();

    uint32_t mountedFrequency = 0;
    if (!mountSdCard(mountedFrequency)) {
        gSummary.mount  = StepState::Fail;
        gSummary.result = StepState::Fail;
        gSummary.title  = "SD card mount failed";
        gSummary.detail = "Unable to mount the card at 10, 4 or 1 MHz.";
        gSummary.footer = "SD TEST FAIL";
        Serial.println(F("[SD] SD.begin() failed."));
        Serial.println(F("[SD] TEST FAIL: mount failed."));
        gTestRunning = false;
        markDirty();
        return;
    }

    gSummary.mount = StepState::Pass;
    gSummary.mountedFrequency = mountedFrequency;
    gSummary.cardType = SD.cardType();
    gSummary.hasCardDetails = true;
    gSummary.totalMB = SD.totalBytes() / (1024ULL * 1024ULL);
    gSummary.usedMB = SD.usedBytes() / (1024ULL * 1024ULL);

    const char* testPath = "/t_embed_sd_test.txt";
    const char* testData = "T-Embed SD test OK\n";
    bool writeOk = false;
    bool readOk = false;

    File file = SD.open(testPath, FILE_WRITE);
    if (!file) {
        gSummary.write = StepState::Fail;
        Serial.println(F("[SD] Open for write failed."));
    } else {
        const size_t written = file.print(testData);
        file.close();
        writeOk = written == strlen(testData);
        gSummary.write = writeOk ? StepState::Pass : StepState::Fail;
        Serial.println(writeOk ? F("[SD] Write OK.") : F("[SD] Write incomplete."));
    }

    file = SD.open(testPath, FILE_READ);
    if (!file) {
        gSummary.read = StepState::Fail;
        Serial.println(F("[SD] Open for read failed."));
    } else {
        String line = file.readStringUntil('\n');
        file.close();
        readOk = line.startsWith("T-Embed SD test OK");
        gSummary.read = readOk ? StepState::Pass : StepState::Fail;
        if (readOk) {
            Serial.println(F("[SD] Read OK."));
        } else {
            Serial.print(F("[SD] Read mismatch: "));
            Serial.println(line);
        }
    }

    SD.remove(testPath);
    gSummary.rootCount = listRootToSerial();
    gSummary.rootMeasured = true;

    if (writeOk && readOk) {
        gSummary.result = StepState::Pass;
        gSummary.title  = "SD card self-check passed";
        gSummary.detail = "Mount, write/readback and directory probe completed.";
        gSummary.footer = "SD TEST PASS";
        Serial.println(F("[SD] TEST PASS."));
    } else {
        gSummary.result = StepState::Fail;
        gSummary.title  = "SD read/write check failed";
        gSummary.detail = writeOk ? "Readback mismatch or open-for-read failed."
                                  : "Could not create the probe file on the card.";
        gSummary.footer = "SD TEST FAIL";
        Serial.println(F("[SD] TEST FAIL: write/read check failed."));
    }

    Serial.println(F("[SD] Test complete."));
    gTestRunning = false;
    markDirty();
}

void runQueuedTest()
{
    if (!gTestQueued || gTestRunning) {
        return;
    }

    gTestQueued = false;
    resetSummaryForRun();
    redrawScreen();
    gScreenDirty = false;
    gLastUiDrawMs = millis();
    runSdTest();
}

void handleEncoder()
{
    const int32_t delta = (g.encRaw - gEncSnapshot) / 2;
    if (delta == 0) {
        return;
    }

    gEncSnapshot += delta * 2;
    int32_t next = static_cast<int32_t>(gFocus) + delta;
    next %= static_cast<int32_t>(FocusItem::kCount);
    if (next < 0) {
        next += static_cast<int32_t>(FocusItem::kCount);
    }

    const FocusItem newFocus = static_cast<FocusItem>(next);
    if (newFocus != gFocus) {
        gFocus = newFocus;
        markDirty();
    }
}

void queueTest()
{
    if (gTestRunning) {
        return;
    }

    gTestQueued = true;
    markDirty();
}

void handleButtons()
{
    if (g.encBtn.event) {
        g.encBtn.event = false;
        if (gFocus == FocusItem::Back) {
            requestExitSubPage();
            return;
        }
    }

    if (g.usrBtn.event) {
        g.usrBtn.event = false;
        queueTest();
    }
}

}  // namespace

void init()
{
    gFocus = FocusItem::Summary;
    gCanvasReady = false;
    gScreenDirty = true;
    gTestQueued = false;
    gTestRunning = false;
    gLastUiDrawMs = 0;
    gEncSnapshot = g.encRaw;

    gCanvas.deleteSprite();
    gCanvas.setColorDepth(16);
    gCanvasReady = (gCanvas.createSprite(tft.width(), tft.height()) != nullptr);
    if (!gCanvasReady) {
        Serial.println(F("[SD] Sprite allocation failed, using direct TFT redraw."));
    }

    SD.end();
    resetSummaryForRun();
    queueTest();
}

void update()
{
    handleEncoder();
    handleButtons();
    if (g.subPageExitRequested) {
        return;
    }
    runQueuedTest();
}

void render()
{
    if (!gScreenDirty) {
        return;
    }

    const uint32_t now = millis();
    if (gLastUiDrawMs != 0 && (now - gLastUiDrawMs) < kUiFrameIntervalMs) {
        return;
    }

    redrawScreen();
    gScreenDirty = false;
    gLastUiDrawMs = now;
}

void deinit()
{
    gCanvas.deleteSprite();
    gCanvasReady = false;
    gTestQueued = false;
    gTestRunning = false;
    SD.end();
    pinMode(BOARD_SD_CS, OUTPUT);
    digitalWrite(BOARD_SD_CS, HIGH);
    t_embed::board::deselectSharedSpiDevices();
}

}  // namespace page_sd
