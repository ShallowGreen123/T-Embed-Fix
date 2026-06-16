#pragma once
#include <SPI.h>
#include <TFT_eSPI.h>

#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>

namespace page_nfc {

namespace {

constexpr uint32_t kScanIntervalMs      = 180;
constexpr uint32_t kCardLostTimeoutMs   = 700;
constexpr uint32_t kNoCardMessageMs     = 1200;
constexpr uint32_t kSpiClockHz          = 10000000;
constexpr uint32_t kUiFrameIntervalMs   = 33;

constexpr int16_t kUiMargin     = 8;
constexpr int16_t kHeaderHeight = 24;
constexpr int16_t kFooterHeight = 18;
constexpr int16_t kBackBtnW     = 58;
constexpr int16_t kBackBtnH     = 14;

constexpr int16_t kStatusX = 8;
constexpr int16_t kStatusY = 34;
constexpr int16_t kStatusW = 304;
constexpr int16_t kStatusH = 38;

constexpr int16_t kUidX = 8;
constexpr int16_t kUidY = 80;
constexpr int16_t kUidW = 304;
constexpr int16_t kUidH = 20;

constexpr int16_t kMetaY      = 108;
constexpr int16_t kMetaW      = 148;
constexpr int16_t kMetaH      = 18;
constexpr int16_t kMetaLeftX  = 8;
constexpr int16_t kMetaRightX = 164;

constexpr int16_t kSizeX = 8;
constexpr int16_t kSizeY = 134;
constexpr int16_t kSizeW = 304;
constexpr int16_t kSizeH = 18;

constexpr uint16_t kColorBg        = 0x0841;
constexpr uint16_t kColorPanel     = 0x1082;
constexpr uint16_t kColorPanelEdge = 0x31A6;
constexpr uint16_t kColorCard      = 0x18C3;
constexpr uint16_t kColorPassBg    = 0x0A41;
constexpr uint16_t kColorWarnBg    = 0x5A00;
constexpr uint16_t kColorFailBg    = 0x3006;

enum class UiState : uint8_t {
    Init = 0,
    Scanning,
    CardFound,
    IdentifyFail,
    NoCard,
    FatalError,
};

enum class FocusItem : uint8_t {
    Snapshot = 0,
    Back,
    kCount,
};

struct CardSnapshot {
    String uid;
    String type;
    uint16_t atqa = 0;
    uint8_t  sak = 0;
    uint16_t userAreaSize = 0;
    uint16_t totalSize = 0;

    void clear()
    {
        uid = "";
        type = "";
        atqa = 0;
        sak = 0;
        userAreaSize = 0;
        totalSize = 0;
    }
};

m5::unit::UnitUnified*       gUnits   = nullptr;
m5::unit::UnitST25R3916*     gNfcUnit = nullptr;
m5::nfc::NFCLayerA*          gNfcA    = nullptr;
TFT_eSprite                  gCanvas(&tft);

UiState      gUiState          = UiState::Init;
FocusItem    gFocus            = FocusItem::Snapshot;
CardSnapshot gCurrentCard;
String       gDetailLine;
bool         gScreenDirty      = true;
bool         gCanvasReady      = false;
bool         gInitOk           = false;
bool         gCardPresent      = false;
uint32_t     gLastPollAtMs     = 0;
uint32_t     gLastSeenAtMs     = 0;
uint32_t     gStateChangedAtMs = 0;
uint32_t     gLastUiDrawMs     = 0;
int32_t      gEncSnapshot      = 0;

void markDirty()
{
    gScreenDirty = true;
}

void setState(const UiState next, const String& detail = String())
{
    if (gUiState != next || gDetailLine != detail) {
        gUiState = next;
        gDetailLine = detail;
        gStateChangedAtMs = millis();
        markDirty();
    }
}

const char* stateLabel()
{
    switch (gUiState) {
        case UiState::Init:         return "INIT";
        case UiState::Scanning:     return "SCANNING";
        case UiState::CardFound:    return "CARD FOUND";
        case UiState::IdentifyFail: return "IDENTIFY FAIL";
        case UiState::NoCard:       return "NO CARD";
        case UiState::FatalError:   return "FATAL ERROR";
    }
    return "?";
}

uint16_t stateColor()
{
    switch (gUiState) {
        case UiState::Init:         return TFT_CYAN;
        case UiState::Scanning:     return TFT_YELLOW;
        case UiState::CardFound:    return TFT_GREEN;
        case UiState::IdentifyFail: return TFT_ORANGE;
        case UiState::NoCard:       return TFT_LIGHTGREY;
        case UiState::FatalError:   return TFT_RED;
    }
    return TFT_WHITE;
}

uint16_t statusFillColor()
{
    switch (gUiState) {
        case UiState::CardFound:    return kColorPassBg;
        case UiState::IdentifyFail: return kColorWarnBg;
        case UiState::FatalError:   return kColorFailBg;
        case UiState::NoCard:       return kColorCard;
        case UiState::Init:
        case UiState::Scanning:
        default:                    return kColorPanel;
    }
}

String statusDetailText()
{
    if (!gDetailLine.isEmpty()) {
        return gDetailLine;
    }

    switch (gUiState) {
        case UiState::Init:         return "Bringing up NFC reader";
        case UiState::Scanning:     return "Bring an NFC-A tag close to the antenna";
        case UiState::CardFound:    return "Tag identified successfully";
        case UiState::IdentifyFail: return "Tag detected, but card type could not be resolved";
        case UiState::NoCard:       return "Tag removed";
        case UiState::FatalError:   return "Check NFC wiring and power";
    }
    return "";
}

const char* statusPillText()
{
    switch (gUiState) {
        case UiState::Init:         return "BOOT";
        case UiState::Scanning:     return "SCAN";
        case UiState::CardFound:    return "LIVE";
        case UiState::IdentifyFail: return "WARN";
        case UiState::NoCard:       return "IDLE";
        case UiState::FatalError:   return "STOP";
    }
    return "?";
}

String uidText()
{
    return gCurrentCard.uid.isEmpty() ? String("-") : gCurrentCard.uid;
}

String typeText()
{
    return gCurrentCard.type.isEmpty() ? String("-") : gCurrentCard.type;
}

String atqaSakText()
{
    if (!gCurrentCard.atqa && !gCurrentCard.sak) {
        return "-";
    }

    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%04X / %02X", gCurrentCard.atqa, gCurrentCard.sak);
    return String(buffer);
}

String sizeText()
{
    if (!gCurrentCard.totalSize) {
        return "-";
    }

    return String(gCurrentCard.userAreaSize) + " / " +
           String(gCurrentCard.totalSize) + " bytes";
}

String footerHint()
{
    if (gFocus == FocusItem::Back) {
        return "BOOT returns to menu";
    }

    if (!gInitOk) {
        return "Turn to BACK";
    }

    switch (gUiState) {
        case UiState::CardFound:    return "USR=rescan  turn=BACK";
        case UiState::IdentifyFail: return "Re-center tag  USR=rescan";
        case UiState::NoCard:       return "Tag removed  USR=rescan";
        case UiState::FatalError:   return "Reader init failed";
        case UiState::Init:
        case UiState::Scanning:
        default:                    return "Bring NFC-A tag close  turn=BACK";
    }
}

void drawBackButton(TFT_eSprite& gfx, const bool selected)
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

void drawBackButton(TFT_eSPI& gfx, const bool selected)
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
    gfx.drawString("ST25R3916 NFC Test", kUiMargin, 5, 2);

    gfx.setTextDatum(TR_DATUM);
    gfx.setTextColor(TFT_CYAN, TFT_NAVY);
    gfx.drawString("NFC-A / HSPI", gfx.width() - kUiMargin, 7, 1);
    gfx.setTextDatum(TL_DATUM);
}

template <typename Canvas>
void drawStatusPanel(Canvas& gfx)
{
    const uint16_t accent = stateColor();
    const uint16_t fill = statusFillColor();

    gfx.fillRoundRect(kStatusX, kStatusY, kStatusW, kStatusH, 8, fill);
    gfx.drawRoundRect(kStatusX, kStatusY, kStatusW, kStatusH, 8, accent);

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(accent, fill);
    gfx.drawString(stateLabel(), kStatusX + 10, kStatusY + 6, 2);

    gfx.fillRoundRect(kStatusX + kStatusW - 60, kStatusY + 7, 48, 16, 6, accent);
    gfx.setTextDatum(MC_DATUM);
    gfx.setTextColor(TFT_BLACK, accent);
    gfx.drawString(statusPillText(), kStatusX + kStatusW - 36, kStatusY + 15, 1);

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_LIGHTGREY, fill);
    gfx.drawString(statusDetailText(), kStatusX + 10, kStatusY + 24, 1);
}

template <typename Canvas>
void drawFieldCard(Canvas& gfx,
                   const int16_t x,
                   const int16_t y,
                   const int16_t w,
                   const int16_t h,
                   const char* label,
                   const String& value,
                   const uint16_t borderColor)
{
    gfx.fillRoundRect(x, y, w, h, 6, kColorCard);
    gfx.drawRoundRect(x, y, w, h, 6, borderColor);

    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_CYAN, kColorCard);
    gfx.drawString(label, x + 8, y + 5, 1);

    gfx.setTextColor(TFT_WHITE, kColorCard);
    gfx.drawString(value, x + 56, y + 5, 1);
}

template <typename Canvas>
void drawCardDetails(Canvas& gfx)
{
    gfx.fillRoundRect(kUidX, kUidY, kUidW, kUidH, 6, kColorCard);
    gfx.drawRoundRect(kUidX, kUidY, kUidW, kUidH, 6, stateColor());
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_CYAN, kColorCard);
    gfx.drawString("UID", kUidX + 8, kUidY + 6, 1);
    gfx.setTextColor(TFT_WHITE, kColorCard);
    gfx.drawString(uidText(), kUidX + 56, kUidY + 6, 1);

    drawFieldCard(gfx, kMetaLeftX, kMetaY, kMetaW, kMetaH,
                  "Type", typeText(), kColorPanelEdge);
    drawFieldCard(gfx, kMetaRightX, kMetaY, kMetaW, kMetaH,
                  "ATQA/SAK", atqaSakText(), kColorPanelEdge);
    drawFieldCard(gfx, kSizeX, kSizeY, kSizeW, kSizeH,
                  "User/Total", sizeText(),
                  gCurrentCard.totalSize ? kColorPanelEdge : TFT_DARKGREY);
}

template <typename Canvas>
void drawFooter(Canvas& gfx)
{
    const int16_t y = gfx.height() - kFooterHeight;
    gfx.fillRect(0, y, gfx.width(), kFooterHeight, TFT_DARKGREY);
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_WHITE, TFT_DARKGREY);
    gfx.drawString(footerHint(), 6, y + 4, 1);
}

template <typename Canvas>
void drawUi(Canvas& gfx)
{
    gfx.fillRect(0, 0, gfx.width(), gfx.height(), kColorBg);
    drawHeader(gfx);
    drawStatusPanel(gfx);
    drawCardDetails(gfx);
    drawFooter(gfx);
    drawBackButton(gfx, gFocus == FocusItem::Back);
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

SPIClass& sharedSpi()
{
    // TFT already owns the shared HSPI bus on this board.
    return tft.getSPIinstance();
}

void destroyNfcObjects()
{
    delete gNfcA;
    gNfcA = nullptr;
    delete gNfcUnit;
    gNfcUnit = nullptr;
    delete gUnits;
    gUnits = nullptr;
}

bool initNfc()
{
    destroyNfcObjects();

    gUnits = new m5::unit::UnitUnified();
    gNfcUnit = new m5::unit::UnitST25R3916(BOARD_NFC_CS);
    if (!gUnits || !gNfcUnit) {
        Serial.println(F("[NFC] Allocation failed."));
        destroyNfcObjects();
        return false;
    }

    gNfcA = new m5::nfc::NFCLayerA(*gNfcUnit);
    if (!gNfcA) {
        Serial.println(F("[NFC] NFC layer allocation failed."));
        destroyNfcObjects();
        return false;
    }

    auto cfg = gNfcUnit->config();
    cfg.mode = m5::nfc::NFC::A;
    cfg.vdd_voltage_5V = false;
    cfg.using_irq = true;
    cfg.irq = BOARD_NFC_IRQ;
    cfg.emulation = false;
    gNfcUnit->config(cfg);

    SPISettings settings{kSpiClockHz, MSBFIRST, SPI_MODE1};
    if (!gUnits->add(*gNfcUnit, sharedSpi(), settings)) {
        Serial.println(F("[NFC] Units.add failed."));
        destroyNfcObjects();
        return false;
    }

    if (!gUnits->begin()) {
        Serial.println(F("[NFC] Units.begin failed."));
        destroyNfcObjects();
        return false;
    }

    Serial.println(F("[NFC] ST25R3916 initialized in NFC-A mode."));
    return true;
}

void rememberCard(const m5::nfc::a::PICC& picc)
{
    gCurrentCard.uid = String(picc.uidAsString().c_str());
    gCurrentCard.type = String(picc.typeAsString().c_str());
    gCurrentCard.atqa = picc.atqa;
    gCurrentCard.sak = picc.sak;
    gCurrentCard.userAreaSize = picc.userAreaSize();
    gCurrentCard.totalSize = picc.totalSize();
}

bool detectSinglePicc(m5::nfc::a::PICC& picc, const uint32_t timeoutMs)
{
    if (!gNfcA) {
        return false;
    }

    const uint32_t startMs = millis();
    do {
        picc = {};

        uint16_t atqa = 0;
        bool detected = gNfcA->request(atqa);
        if (!detected) {
            detected = gNfcA->wakeup(atqa);
        }
        if (!detected) {
            delay(1);
            continue;
        }

        picc.atqa = atqa;
        if (!gNfcA->select(picc)) {
            delay(1);
            continue;
        }

        return true;
    } while (millis() - startMs <= timeoutMs);

    return false;
}

void restartScan()
{
    gCardPresent = false;
    gCurrentCard.clear();
    gLastSeenAtMs = 0;
    gLastPollAtMs = 0;
    setState(UiState::Scanning, "Waiting for NFC-A tag");
    markDirty();
}

void logCard()
{
    Serial.print(F("[NFC] UID: "));
    Serial.println(gCurrentCard.uid);
    Serial.print(F("[NFC] Type: "));
    Serial.println(gCurrentCard.type);
    Serial.print(F("[NFC] ATQA: 0x"));
    Serial.println(gCurrentCard.atqa, HEX);
    Serial.print(F("[NFC] SAK: 0x"));
    Serial.println(gCurrentCard.sak, HEX);
    Serial.print(F("[NFC] User/Total: "));
    Serial.print(gCurrentCard.userAreaSize);
    Serial.print(F(" / "));
    Serial.println(gCurrentCard.totalSize);
}

void handleDetectedPicc()
{
    m5::nfc::a::PICC picc{};
    if (!detectSinglePicc(picc, 100U)) {
        return;
    }

    const String detectedUid = String(picc.uidAsString().c_str());
    if (gCardPresent && detectedUid == gCurrentCard.uid) {
        gLastSeenAtMs = millis();
        (void)gNfcA->deactivate();
        return;
    }

    if (gNfcA->identify(picc)) {
        rememberCard(picc);
        gCardPresent = true;
        gLastSeenAtMs = millis();
        setState(UiState::CardFound, "NFC-A PICC detected");
        logCard();
    } else {
        gCurrentCard.clear();
        gCurrentCard.uid = detectedUid;
        gCardPresent = true;
        gLastSeenAtMs = millis();
        setState(UiState::IdentifyFail, "Detected tag but identify failed");
        Serial.print(F("[NFC] Failed to identify PICC: "));
        Serial.println(detectedUid);
    }

    (void)gNfcA->deactivate();
}

void handleCardTimeout()
{
    const uint32_t now = millis();

    if (gCardPresent && (now - gLastSeenAtMs > kCardLostTimeoutMs)) {
        gCardPresent = false;
        gCurrentCard.clear();
        setState(UiState::NoCard, "Card removed");
        Serial.println(F("[NFC] Card removed."));
        return;
    }

    if (!gCardPresent &&
        gUiState == UiState::NoCard &&
        (now - gStateChangedAtMs > kNoCardMessageMs)) {
        setState(UiState::Scanning, "Waiting for NFC-A tag");
    }
}

void pollNfc()
{
    if (!gInitOk || !gUnits) {
        return;
    }

    const uint32_t now = millis();
    if (now - gLastPollAtMs < kScanIntervalMs) {
        return;
    }
    gLastPollAtMs = now;

    t_embed::board::deselectSharedSpiDevices();
    gUnits->update();
    handleDetectedPicc();
    handleCardTimeout();
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
        if (gInitOk) {
            restartScan();
        } else {
            gFocus = FocusItem::Back;
            markDirty();
        }
    }
}

}  // namespace

void init()
{
    gUiState = UiState::Init;
    gFocus = FocusItem::Snapshot;
    gCurrentCard.clear();
    gDetailLine = "";
    gScreenDirty = true;
    gCanvasReady = false;
    gInitOk = false;
    gCardPresent = false;
    gLastPollAtMs = 0;
    gLastSeenAtMs = 0;
    gStateChangedAtMs = millis();
    gLastUiDrawMs = 0;
    gEncSnapshot = g.encRaw;

    gCanvas.deleteSprite();
    gCanvas.setColorDepth(16);
    gCanvasReady = (gCanvas.createSprite(tft.width(), tft.height()) != nullptr);
    if (!gCanvasReady) {
        Serial.println(F("[NFC] Sprite allocation failed, using direct TFT redraw."));
    }

    t_embed::board::deselectSharedSpiDevices();

    setState(UiState::Init, "Power rails and display ready");
    if (!initNfc()) {
        setState(UiState::FatalError, "ST25R3916 init failed");
        return;
    }

    gInitOk = true;
    setState(UiState::Scanning, "Waiting for NFC-A tag");
}

void update()
{
    handleEncoder();
    handleButtons();
    pollNfc();
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
    destroyNfcObjects();
    t_embed::board::deselectSharedSpiDevices();
    gInitOk = false;
}

}  // namespace page_nfc
