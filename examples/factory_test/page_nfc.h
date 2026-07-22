#pragma once

#include <SPI.h>
#include <TFT_eSPI.h>

#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>

#include <cstddef>
#include <cstring>

namespace page_nfc {

namespace {

constexpr uint32_t kScanIntervalMs    = 180;
constexpr uint32_t kCardLostTimeoutMs = 700;
constexpr uint32_t kNoCardMessageMs   = 1200;
constexpr uint32_t kSpiClockHz        = 10000000;
constexpr uint32_t kUiFrameIntervalMs = 33;
constexpr uint32_t kHealthCheckIntervalMs = 2500;
constexpr uint32_t kRecoveryRetryMs       = 1500;
constexpr uint32_t kFieldOffGuardMs       = 10;
constexpr uint8_t  kInitAttempts           = 3;
constexpr uint8_t  kHealthFailuresToReset  = 2;

// Listener RF profile. These values follow Flipper's ST25R3916 NFC-A
// listener setup, while retaining the 0x82/0x82 tuning used by this board
// before M5Unit-NFC's emulation adapter overwrites it with 0x00/0xFF.
constexpr uint8_t kEmulationRxConfig1          = 0x08;  // 600 kHz first-stage zero
constexpr uint8_t kEmulationRxConfig2          = 0x2D;  // dynamic squelch + AGC 6:3
constexpr uint8_t kEmulationCorrelatorConfig1  = 0x51;
constexpr uint8_t kEmulationMaskReceiveTimer   = 0x02;
constexpr uint8_t kEmulationFieldActivate      = 0x11;  // 105 mV on both channels
constexpr uint8_t kEmulationFieldDeactivate    = 0x00;  // 75 mV on both channels
constexpr uint8_t kEmulationAuxModulation      = 0x30;  // external + internal load modulation
constexpr uint8_t kEmulationTargetModulation   = 0x0F;  // stronger modulated-state load
constexpr uint8_t kEmulationEmdSuppression     = 0x40;  // RX start on first four bits
constexpr uint8_t kEmulationAntennaTuneA       = 0x82;
constexpr uint8_t kEmulationAntennaTuneB       = 0x82;

constexpr uint8_t  kMaxSavedTags       = 4;
constexpr uint16_t kMaxTagMemoryBytes  = 924;  // NTAG216: 231 pages * 4 bytes
constexpr uint32_t kSavedTagMagic      = 0x4E464331;  // "NFC1"
constexpr uint8_t  kSavedTagVersion    = 1;

constexpr int16_t kUiMargin     = 8;
constexpr int16_t kHeaderHeight = 24;
constexpr int16_t kFooterHeight = 18;

constexpr uint16_t kColorBg        = 0x0841;
constexpr uint16_t kColorPanel     = 0x1082;
constexpr uint16_t kColorPanelEdge = 0x31A6;
constexpr uint16_t kColorCard      = 0x18C3;
constexpr uint16_t kColorPassBg    = 0x0A41;
constexpr uint16_t kColorWarnBg    = 0x5A00;
constexpr uint16_t kColorFailBg    = 0x3006;

enum class PageMode : uint8_t {
    Scan = 0,
    Saved,
    Emulating,
    DeleteConfirm,
};

enum class UiState : uint8_t {
    Init = 0,
    Scanning,
    CardFound,
    IdentifyFail,
    NoCard,
    Copying,
    CopySuccess,
    CopyFail,
    Unsupported,
    LibraryFull,
    FatalError,
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

// Stored as one Preferences blob per slot. Keep checksum last.
struct SavedTag {
    uint32_t magic = 0;
    uint8_t version = 0;
    uint8_t type = 0;
    uint8_t uidSize = 0;
    uint8_t reserved0 = 0;
    uint8_t uid[10]{};
    uint16_t atqa = 0;
    uint16_t memorySize = 0;
    uint8_t sak = 0;
    uint8_t reserved1[3]{};
    uint8_t memory[kMaxTagMemoryBytes]{};
    uint8_t reserved2[2]{};
    uint32_t checksum = 0;
};

m5::unit::UnitUnified*       gUnits   = nullptr;
m5::unit::UnitST25R3916*     gNfcUnit = nullptr;
m5::nfc::NFCLayerA*          gNfcA    = nullptr;
m5::nfc::EmulationLayerA*    gEmuA    = nullptr;
TFT_eSprite                  gCanvas(&tft);

PageMode                 gPageMode = PageMode::Scan;
UiState                  gUiState = UiState::Init;
CardSnapshot             gCurrentCard;
m5::nfc::a::PICC         gDetectedPicc{};
SavedTag                 gSavedTags[kMaxSavedTags]{};
uint8_t                  gEmulationMemory[kMaxTagMemoryBytes]{};
String                   gDetailLine;
String                   gLibraryNotice;
bool                     gScreenDirty = true;
bool                     gCanvasReady = false;
bool                     gInitOk = false;
bool                     gNfcBusAttached = false;
bool                     gCardPresent = false;
uint8_t                  gSavedCount = 0;
int8_t                   gSelectedSlot = -1;
int8_t                   gEmulatingSlot = -1;
uint8_t                  gActionIndex = 0;
uint32_t                 gLastPollAtMs = 0;
uint32_t                 gLastSeenAtMs = 0;
uint32_t                 gStateChangedAtMs = 0;
uint32_t                 gLastUiDrawMs = 0;
uint32_t                 gLastHealthCheckMs = 0;
uint32_t                 gLastRecoveryAttemptMs = 0;
uint8_t                  gHealthFailureCount = 0;
int32_t                  gEncSnapshot = 0;
m5::nfc::EmulationLayerA::State gLastEmulationState =
    m5::nfc::EmulationLayerA::State::None;

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

uint32_t savedTagChecksum(const SavedTag& tag)
{
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&tag);
    uint32_t hash = 2166136261UL;
    for (size_t i = 0; i < offsetof(SavedTag, checksum); ++i) {
        hash ^= bytes[i];
        hash *= 16777619UL;
    }
    return hash;
}

bool isEmulatableType(const m5::nfc::a::Type type, const uint8_t uidSize)
{
    return (uidSize == 4 || uidSize == 7) &&
           (type == m5::nfc::a::Type::MIFARE_Ultralight ||
            (type >= m5::nfc::a::Type::NTAG_203 && type <= m5::nfc::a::Type::NTAG_216));
}

bool validateSavedTag(const SavedTag& tag)
{
    if (tag.magic != kSavedTagMagic || tag.version != kSavedTagVersion ||
        tag.memorySize == 0 || tag.memorySize > kMaxTagMemoryBytes ||
        savedTagChecksum(tag) != tag.checksum) {
        return false;
    }

    const auto type = static_cast<m5::nfc::a::Type>(tag.type);
    if (!isEmulatableType(type, tag.uidSize)) {
        return false;
    }

    m5::nfc::a::PICC picc{};
    return picc.emulate(type, tag.uid, tag.uidSize) &&
           picc.totalSize() == tag.memorySize;
}

void slotKey(const uint8_t slot, char key[8])
{
    snprintf(key, 8, "nfc%u", static_cast<unsigned>(slot));
}

void recountSavedTags()
{
    gSavedCount = 0;
    for (uint8_t i = 0; i < kMaxSavedTags; ++i) {
        if (validateSavedTag(gSavedTags[i])) {
            ++gSavedCount;
        }
    }
}

int8_t firstSavedSlot()
{
    for (uint8_t i = 0; i < kMaxSavedTags; ++i) {
        if (validateSavedTag(gSavedTags[i])) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;
}

int8_t nextSavedSlot(const int8_t current)
{
    if (!gSavedCount) {
        return -1;
    }
    for (uint8_t step = 1; step <= kMaxSavedTags; ++step) {
        const uint8_t slot = static_cast<uint8_t>((current + step + kMaxSavedTags) % kMaxSavedTags);
        if (validateSavedTag(gSavedTags[slot])) {
            return static_cast<int8_t>(slot);
        }
    }
    return firstSavedSlot();
}

int8_t findSavedUid(const m5::nfc::a::PICC& picc)
{
    for (uint8_t i = 0; i < kMaxSavedTags; ++i) {
        const SavedTag& tag = gSavedTags[i];
        if (validateSavedTag(tag) && tag.uidSize == picc.size &&
            memcmp(tag.uid, picc.uid, picc.size) == 0) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;
}

int8_t firstEmptySlot()
{
    for (uint8_t i = 0; i < kMaxSavedTags; ++i) {
        if (!validateSavedTag(gSavedTags[i])) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;
}

bool persistSlot(const uint8_t slot)
{
    SavedTag& tag = gSavedTags[slot];
    tag.checksum = savedTagChecksum(tag);
    if (!gPrefsReady) {
        return false;
    }

    char key[8]{};
    slotKey(slot, key);
    return gPrefs.putBytes(key, &tag, sizeof(tag)) == sizeof(tag);
}

bool eraseSlot(const uint8_t slot)
{
    memset(&gSavedTags[slot], 0, sizeof(gSavedTags[slot]));
    bool persisted = false;
    if (gPrefsReady) {
        char key[8]{};
        slotKey(slot, key);
        persisted = gPrefs.remove(key);
    }
    recountSavedTags();
    return persisted;
}

void loadSavedTags()
{
    memset(gSavedTags, 0, sizeof(gSavedTags));
    if (gPrefsReady) {
        for (uint8_t i = 0; i < kMaxSavedTags; ++i) {
            char key[8]{};
            slotKey(i, key);
            if (gPrefs.getBytesLength(key) != sizeof(SavedTag) ||
                gPrefs.getBytes(key, &gSavedTags[i], sizeof(SavedTag)) != sizeof(SavedTag) ||
                !validateSavedTag(gSavedTags[i])) {
                memset(&gSavedTags[i], 0, sizeof(gSavedTags[i]));
            }
        }
    }
    recountSavedTags();
    gSelectedSlot = firstSavedSlot();
}

String uidString(const uint8_t* uid, const uint8_t size)
{
    char buffer[21]{};
    size_t offset = 0;
    for (uint8_t i = 0; i < size && i < 10 && offset + 2 < sizeof(buffer); ++i) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%02X", uid[i]);
    }
    return String(buffer);
}

String storedTypeText(const SavedTag& tag)
{
    m5::nfc::a::PICC picc{};
    if (!picc.emulate(static_cast<m5::nfc::a::Type>(tag.type), tag.uid, tag.uidSize)) {
        return "Unknown";
    }
    return String(picc.typeAsString().c_str());
}

String fitText(const String& text, const uint8_t maxChars)
{
    if (text.length() <= maxChars) {
        return text;
    }
    return text.substring(0, maxChars - 2) + "..";
}

const char* stateLabel()
{
    switch (gUiState) {
        case UiState::Init:         return "INIT";
        case UiState::Scanning:     return "SCANNING";
        case UiState::CardFound:    return "TAG FOUND";
        case UiState::IdentifyFail: return "IDENTIFY FAIL";
        case UiState::NoCard:       return "NO TAG";
        case UiState::Copying:      return "COPYING";
        case UiState::CopySuccess:  return "TAG COPIED";
        case UiState::CopyFail:     return "COPY FAILED";
        case UiState::Unsupported:  return "NOT SUPPORTED";
        case UiState::LibraryFull:  return "LIBRARY FULL";
        case UiState::FatalError:   return "FATAL ERROR";
    }
    return "?";
}

uint16_t stateColor()
{
    switch (gUiState) {
        case UiState::Init:         return TFT_CYAN;
        case UiState::Scanning:     return TFT_YELLOW;
        case UiState::CardFound:
        case UiState::CopySuccess:  return TFT_GREEN;
        case UiState::Copying:      return TFT_CYAN;
        case UiState::IdentifyFail:
        case UiState::Unsupported:
        case UiState::LibraryFull:  return TFT_ORANGE;
        case UiState::NoCard:       return TFT_LIGHTGREY;
        case UiState::CopyFail:
        case UiState::FatalError:   return TFT_RED;
    }
    return TFT_WHITE;
}

uint16_t statusFillColor()
{
    switch (gUiState) {
        case UiState::CardFound:
        case UiState::CopySuccess:  return kColorPassBg;
        case UiState::IdentifyFail:
        case UiState::Unsupported:
        case UiState::LibraryFull:  return kColorWarnBg;
        case UiState::CopyFail:
        case UiState::FatalError:   return kColorFailBg;
        case UiState::NoCard:       return kColorCard;
        case UiState::Init:
        case UiState::Scanning:
        case UiState::Copying:
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
        case UiState::Scanning:     return "Bring an NFC-A tag close";
        case UiState::CardFound:    return "Press BOOT to copy this tag";
        case UiState::IdentifyFail: return "Re-center tag and try again";
        case UiState::NoCard:       return "Tag removed";
        case UiState::Copying:      return "Reading tag memory";
        case UiState::CopySuccess:  return "Saved and ready to emulate";
        case UiState::CopyFail:     return "Tag moved or protected";
        case UiState::Unsupported:  return "Only Ultralight / NTAG2xx";
        case UiState::LibraryFull:  return "Delete a saved tag first";
        case UiState::FatalError:   return "Check NFC wiring and power";
    }
    return "";
}

String currentUidText()
{
    return gCurrentCard.uid.isEmpty() ? String("-") : gCurrentCard.uid;
}

String currentTypeText()
{
    return gCurrentCard.type.isEmpty() ? String("-") : gCurrentCard.type;
}

String atqaSakText()
{
    if (!gCurrentCard.atqa && !gCurrentCard.sak) {
        return "-";
    }
    char buffer[24]{};
    snprintf(buffer, sizeof(buffer), "%04X / %02X", gCurrentCard.atqa, gCurrentCard.sak);
    return String(buffer);
}

String currentSizeText()
{
    if (!gCurrentCard.totalSize) {
        return "-";
    }
    return String(gCurrentCard.userAreaSize) + " / " + String(gCurrentCard.totalSize) + " bytes";
}

uint8_t actionCount()
{
    switch (gPageMode) {
        case PageMode::Scan:          return 2;  // Copy area, Back
        case PageMode::Saved:         return gSavedCount ? 4 : 2;
        case PageMode::Emulating:     return 2;
        case PageMode::DeleteConfirm: return 2;
    }
    return 1;
}

String footerHint()
{
    switch (gPageMode) {
        case PageMode::Scan:
            return gActionIndex == 1 ? "BOOT=back" : "BOOT=copy tag  USER=saved";
        case PageMode::Saved:
            return gSavedCount > 1 ? "USER=next tag  turn=action" : "Turn=action  BOOT=select";
        case PageMode::Emulating:
            return "Hold near reader  BOOT=select";
        case PageMode::DeleteConfirm:
            return "Confirm deletion  USER=cancel";
    }
    return "";
}

const char* emulationStateText()
{
    using State = m5::nfc::EmulationLayerA::State;
    switch (gLastEmulationState) {
        case State::None:   return "STOPPED";
        case State::Off:    return "WAITING FOR READER";
        case State::Idle:   return "FIELD DETECTED";
        case State::Ready:  return "READER SELECTING";
        case State::Active: return "READER CONNECTED";
        case State::Halt:   return "SESSION COMPLETE";
    }
    return "?";
}

template <typename Canvas>
void drawHeader(Canvas& gfx, const char* rightText)
{
    gfx.fillRect(0, 0, gfx.width(), kHeaderHeight, TFT_NAVY);
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_WHITE, TFT_NAVY);
    gfx.drawString("ST25R3916 NFC", kUiMargin, 5, 2);
    gfx.setTextDatum(TR_DATUM);
    gfx.setTextColor(TFT_CYAN, TFT_NAVY);
    gfx.drawString(rightText, gfx.width() - kUiMargin, 7, 1);
    gfx.setTextDatum(TL_DATUM);
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
void drawActionButton(Canvas& gfx, const int16_t x, const int16_t y, const int16_t w,
                      const char* label, const bool selected, const bool destructive = false)
{
    const uint16_t bg = selected ? (destructive ? TFT_RED : TFT_WHITE)
                                 : (destructive ? kColorFailBg : TFT_DARKGREY);
    const uint16_t fg = selected ? TFT_BLACK : TFT_LIGHTGREY;
    const uint16_t edge = selected ? TFT_YELLOW : (destructive ? TFT_RED : 0x52AA);
    gfx.fillRoundRect(x, y, w, 18, 5, bg);
    gfx.drawRoundRect(x, y, w, 18, 5, edge);
    gfx.setTextDatum(MC_DATUM);
    gfx.setTextColor(fg, bg);
    gfx.drawString(label, x + w / 2, y + 9, 1);
    gfx.setTextDatum(TL_DATUM);
}

template <typename Canvas>
void drawField(Canvas& gfx, const int16_t y, const char* label, const String& value,
               const uint16_t edge = kColorPanelEdge)
{
    gfx.fillRoundRect(8, y, 304, 20, 6, kColorCard);
    gfx.drawRoundRect(8, y, 304, 20, 6, edge);
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(TFT_CYAN, kColorCard);
    gfx.drawString(label, 16, y + 6, 1);
    gfx.setTextColor(TFT_WHITE, kColorCard);
    gfx.drawString(fitText(value, 34), 72, y + 6, 1);
}

template <typename Canvas>
void drawScanView(Canvas& gfx)
{
    char right[24]{};
    snprintf(right, sizeof(right), "SCAN  SAVED %u/%u", gSavedCount, kMaxSavedTags);
    drawHeader(gfx, right);

    const uint16_t accent = stateColor();
    const uint16_t fill = statusFillColor();
    gfx.fillRoundRect(8, 29, 304, 35, 7, fill);
    gfx.drawRoundRect(8, 29, 304, 35, 7,
                      gActionIndex == 0 ? TFT_YELLOW : accent);
    gfx.setTextDatum(TL_DATUM);
    gfx.setTextColor(accent, fill);
    gfx.drawString(stateLabel(), 18, 34, 2);
    gfx.setTextColor(TFT_LIGHTGREY, fill);
    gfx.drawString(fitText(statusDetailText(), 45), 18, 52, 1);

    drawField(gfx, 70, "UID", currentUidText(), accent);
    drawField(gfx, 94, "Type", currentTypeText());

    gfx.fillRoundRect(8, 118, 148, 20, 6, kColorCard);
    gfx.drawRoundRect(8, 118, 148, 20, 6, kColorPanelEdge);
    gfx.setTextColor(TFT_CYAN, kColorCard);
    gfx.drawString("ATQA/SAK", 16, 124, 1);
    gfx.setTextColor(TFT_WHITE, kColorCard);
    gfx.drawRightString(atqaSakText(), 148, 124, 1);

    gfx.fillRoundRect(164, 118, 148, 20, 6, kColorCard);
    gfx.drawRoundRect(164, 118, 148, 20, 6, kColorPanelEdge);
    gfx.setTextColor(TFT_CYAN, kColorCard);
    gfx.drawString("Memory", 172, 124, 1);
    gfx.setTextColor(TFT_WHITE, kColorCard);
    gfx.drawRightString(fitText(currentSizeText(), 16), 304, 124, 1);

    drawFooter(gfx);
    drawActionButton(gfx, 254, gfx.height() - kFooterHeight + 1, 60, "BACK", gActionIndex == 1);
}

template <typename Canvas>
void drawSavedView(Canvas& gfx)
{
    char right[24]{};
    snprintf(right, sizeof(right), "SAVED %u/%u", gSavedCount, kMaxSavedTags);
    drawHeader(gfx, right);

    if (!gSavedCount || gSelectedSlot < 0 || !validateSavedTag(gSavedTags[gSelectedSlot])) {
        gfx.fillRoundRect(20, 42, 280, 70, 9, kColorPanel);
        gfx.drawRoundRect(20, 42, 280, 70, 9, TFT_LIGHTGREY);
        gfx.setTextDatum(MC_DATUM);
        gfx.setTextColor(TFT_LIGHTGREY, kColorPanel);
        gfx.drawString("NO SAVED TAGS", 160, 65, 2);
        gfx.setTextColor(TFT_WHITE, kColorPanel);
        gfx.drawString("Scan a tag and press BOOT to copy", 160, 91, 1);
        drawActionButton(gfx, 72, 124, 82, "SCAN", gActionIndex == 0);
        drawActionButton(gfx, 166, 124, 82, "BACK", gActionIndex == 1);
        drawFooter(gfx);
        return;
    }

    const SavedTag& tag = gSavedTags[gSelectedSlot];
    String slotText = "TAG " + String(gSelectedSlot + 1) + " / SLOT " + String(gSelectedSlot + 1);
    if (!gLibraryNotice.isEmpty()) {
        slotText = gLibraryNotice;
    }
    gfx.fillRoundRect(8, 29, 304, 20, 6, gLibraryNotice.isEmpty() ? kColorPanel : kColorPassBg);
    gfx.drawRoundRect(8, 29, 304, 20, 6, gLibraryNotice.isEmpty() ? TFT_CYAN : TFT_GREEN);
    gfx.setTextDatum(MC_DATUM);
    gfx.setTextColor(gLibraryNotice.isEmpty() ? TFT_CYAN : TFT_GREEN,
                     gLibraryNotice.isEmpty() ? kColorPanel : kColorPassBg);
    gfx.drawString(slotText, 160, 39, 1);

    drawField(gfx, 54, "UID", uidString(tag.uid, tag.uidSize), TFT_GREEN);
    drawField(gfx, 78, "Type", storedTypeText(tag));
    drawField(gfx, 102, "Memory", String(tag.memorySize) + " bytes");

    drawActionButton(gfx, 8, 126, 68, "USE", gActionIndex == 0);
    drawActionButton(gfx, 84, 126, 68, "DELETE", gActionIndex == 1, true);
    drawActionButton(gfx, 160, 126, 68, "SCAN", gActionIndex == 2);
    drawActionButton(gfx, 236, 126, 76, "BACK", gActionIndex == 3);
    drawFooter(gfx);
}

template <typename Canvas>
void drawEmulatingView(Canvas& gfx)
{
    drawHeader(gfx, "CARD EMULATION");
    const bool active = gLastEmulationState == m5::nfc::EmulationLayerA::State::Active;
    const uint16_t accent = active ? TFT_GREEN : TFT_CYAN;

    gfx.fillRoundRect(8, 31, 304, 38, 8, active ? kColorPassBg : kColorPanel);
    gfx.drawRoundRect(8, 31, 304, 38, 8, accent);
    gfx.setTextDatum(MC_DATUM);
    gfx.setTextColor(accent, active ? kColorPassBg : kColorPanel);
    gfx.drawString("EMULATING", 160, 43, 2);
    gfx.setTextColor(TFT_WHITE, active ? kColorPassBg : kColorPanel);
    gfx.drawString(emulationStateText(), 160, 61, 1);

    if (gEmulatingSlot >= 0 && validateSavedTag(gSavedTags[gEmulatingSlot])) {
        const SavedTag& tag = gSavedTags[gEmulatingSlot];
        drawField(gfx, 76, "UID", uidString(tag.uid, tag.uidSize), accent);
        drawField(gfx, 100, "Type", storedTypeText(tag));
    }

    drawActionButton(gfx, 68, 126, 84, "STOP", gActionIndex == 0);
    drawActionButton(gfx, 168, 126, 84, "BACK", gActionIndex == 1);
    drawFooter(gfx);
}

template <typename Canvas>
void drawDeleteConfirmView(Canvas& gfx)
{
    drawSavedView(gfx);
    gfx.fillRoundRect(28, 39, 264, 94, 10, kColorFailBg);
    gfx.drawRoundRect(28, 39, 264, 94, 10, TFT_RED);
    gfx.setTextDatum(MC_DATUM);
    gfx.setTextColor(TFT_RED, kColorFailBg);
    gfx.drawString("DELETE SAVED TAG?", 160, 56, 2);
    gfx.setTextColor(TFT_WHITE, kColorFailBg);
    if (gSelectedSlot >= 0 && validateSavedTag(gSavedTags[gSelectedSlot])) {
        gfx.drawString(uidString(gSavedTags[gSelectedSlot].uid,
                                 gSavedTags[gSelectedSlot].uidSize), 160, 78, 1);
    }
    gfx.drawString("This cannot be undone", 160, 94, 1);
    drawActionButton(gfx, 66, 106, 84, "CANCEL", gActionIndex == 0);
    drawActionButton(gfx, 170, 106, 84, "DELETE", gActionIndex == 1, true);
    drawFooter(gfx);
}

template <typename Canvas>
void drawUi(Canvas& gfx)
{
    gfx.fillRect(0, 0, gfx.width(), gfx.height(), kColorBg);
    switch (gPageMode) {
        case PageMode::Scan:          drawScanView(gfx); break;
        case PageMode::Saved:         drawSavedView(gfx); break;
        case PageMode::Emulating:     drawEmulatingView(gfx); break;
        case PageMode::DeleteConfirm: drawDeleteConfirmView(gfx); break;
    }
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
    return tft.getSPIinstance();
}

void destroyNfcObjects()
{
    if (gEmuA) {
        (void)gEmuA->end();
    }

    // Flipper-style mode teardown: stop the transceiver, remove the RF field,
    // clear/mask stale IRQs, then detach the GPIO ISR before freeing its owner.
    if (gNfcUnit && gNfcBusAttached) {
        using namespace m5::unit::st25r3916::command;
        using namespace m5::unit::st25r3916::regval;
        (void)gNfcUnit->writeDirectCommand(CMD_STOP_ALL_ACTIVITIES);
        uint8_t operation = 0;
        if (gNfcUnit->readOperationControl(operation)) {
            operation &= static_cast<uint8_t>(~(en | rx_en | tx_en | wu | en_fd_mask));
            (void)gNfcUnit->writeOperationControl(operation);
        }
        (void)gNfcUnit->clearInterrupts();
        (void)gNfcUnit->writeMaskInterrupts(0xFFFFFFFFUL);
    }
    detachInterrupt(digitalPinToInterrupt(BOARD_NFC_IRQ));

    delete gEmuA;
    gEmuA = nullptr;
    delete gNfcA;
    gNfcA = nullptr;
    delete gNfcUnit;
    gNfcUnit = nullptr;
    delete gUnits;
    gUnits = nullptr;
    gNfcBusAttached = false;
}

bool initNfcOnce(const bool emulation)
{
    destroyNfcObjects();
    delay(kFieldOffGuardMs);
    t_embed::board::deselectSharedSpiDevices();

    gUnits = new m5::unit::UnitUnified();
    gNfcUnit = new m5::unit::UnitST25R3916(BOARD_NFC_CS);
    if (!gUnits || !gNfcUnit) {
        Serial.println(F("[NFC] Allocation failed."));
        destroyNfcObjects();
        return false;
    }

    if (emulation) {
        gEmuA = new m5::nfc::EmulationLayerA(*gNfcUnit);
        if (!gEmuA) {
            Serial.println(F("[NFC] Emulation layer allocation failed."));
            destroyNfcObjects();
            return false;
        }
    } else {
        gNfcA = new m5::nfc::NFCLayerA(*gNfcUnit);
        if (!gNfcA) {
            Serial.println(F("[NFC] Reader layer allocation failed."));
            destroyNfcObjects();
            return false;
        }
    }

    auto cfg = gNfcUnit->config();
    cfg.mode = m5::nfc::NFC::A;
    cfg.vdd_voltage_5V = false;
    cfg.using_irq = true;
    cfg.irq = BOARD_NFC_IRQ;
    cfg.emulation = emulation;
    gNfcUnit->config(cfg);

    SPISettings settings{kSpiClockHz, MSBFIRST, SPI_MODE1};
    if (!gUnits->add(*gNfcUnit, sharedSpi(), settings)) {
        Serial.println(F("[NFC] Units.add failed."));
        destroyNfcObjects();
        return false;
    }
    gNfcBusAttached = true;
    if (!gUnits->begin()) {
        Serial.println(F("[NFC] Units.begin failed."));
        destroyNfcObjects();
        return false;
    }

    uint8_t chipType = 0;
    uint8_t chipRevision = 0;
    if (!gNfcUnit->readICIdentity(chipType, chipRevision) ||
        chipType != m5::unit::st25r3916::VALID_IDENTIFY_TYPE || chipRevision == 0) {
        Serial.print(F("[NFC] Post-init identity check failed: type=0x"));
        Serial.print(chipType, HEX);
        Serial.print(F(" rev=0x"));
        Serial.println(chipRevision, HEX);
        destroyNfcObjects();
        return false;
    }

    Serial.println(emulation
        ? F("[NFC] ST25R3916 initialized in NFC-A emulation mode.")
        : F("[NFC] ST25R3916 initialized in NFC-A reader mode."));
    return true;
}

bool initNfc(const bool emulation)
{
    for (uint8_t attempt = 1; attempt <= kInitAttempts; ++attempt) {
        if (initNfcOnce(emulation)) {
            gHealthFailureCount = 0;
            gLastHealthCheckMs = millis();
            return true;
        }
        Serial.print(F("[NFC] Init attempt "));
        Serial.print(attempt);
        Serial.print(F("/"));
        Serial.println(kInitAttempts);
        delay(20U * attempt);
    }
    return false;
}

void rememberCard(const m5::nfc::a::PICC& picc)
{
    gDetectedPicc = picc;
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
    gPageMode = PageMode::Scan;
    gActionIndex = 0;
    gCardPresent = false;
    gCurrentCard.clear();
    gDetectedPicc = {};
    gLastSeenAtMs = 0;
    gLastPollAtMs = 0;

    if (!gNfcA || !gNfcUnit || !gUnits) {
        setState(UiState::Init, "Starting NFC-A reader");
        gInitOk = initNfc(false);
        if (!gInitOk) {
            gLastRecoveryAttemptMs = millis();
            setState(UiState::FatalError, "Reader start failed; retrying");
            return;
        }
    }

    gInitOk = true;
    gHealthFailureCount = 0;
    setState(UiState::Scanning, "Waiting for NFC-A tag");
    markDirty();
}

void openSavedLibrary(const String& notice = String())
{
    // The library screen is RF-idle. This mirrors Flipper's low-power mode and
    // guarantees a clean field-off interval before the next poll/listen mode.
    if (gUnits || gNfcUnit || gNfcA || gEmuA) {
        destroyNfcObjects();
    }
    gInitOk = false;
    gCardPresent = false;
    gCurrentCard.clear();
    gDetectedPicc = {};

    recountSavedTags();
    if (gSelectedSlot < 0 || !validateSavedTag(gSavedTags[gSelectedSlot])) {
        gSelectedSlot = firstSavedSlot();
    }
    gPageMode = PageMode::Saved;
    gActionIndex = 0;
    gLibraryNotice = notice;
    markDirty();
}

void logCard()
{
    Serial.print(F("[NFC] UID: "));
    Serial.println(gCurrentCard.uid);
    Serial.print(F("[NFC] Type: "));
    Serial.println(gCurrentCard.type);
    Serial.print(F("[NFC] ATQA/SAK: 0x"));
    Serial.print(gCurrentCard.atqa, HEX);
    Serial.print(F(" / 0x"));
    Serial.println(gCurrentCard.sak, HEX);
}

void handleDetectedPicc()
{
    m5::nfc::a::PICC picc{};
    if (!detectSinglePicc(picc, 100U)) {
        return;
    }

    const String detectedUid = String(picc.uidAsString().c_str());
    if (gCardPresent && gDetectedPicc.valid() && detectedUid == gCurrentCard.uid) {
        gLastSeenAtMs = millis();
        (void)gNfcA->deactivate();
        return;
    }

    if (gNfcA->identify(picc)) {
        rememberCard(picc);
        gCardPresent = true;
        gLastSeenAtMs = millis();
        setState(UiState::CardFound,
                 isEmulatableType(picc.type, picc.size)
                    ? "Press BOOT to copy this tag"
                    : "Detected; emulation is not supported");
        logCard();
    } else {
        gDetectedPicc = {};
        gCurrentCard.clear();
        gCurrentCard.uid = detectedUid;
        gCardPresent = true;
        gLastSeenAtMs = millis();
        setState(UiState::IdentifyFail, "Detected tag but identify failed");
        Serial.print(F("[NFC] Failed to identify PICC: "));
        Serial.println(detectedUid);
    }
    // NFCLayerA::identify() already deactivates the PICC. Calling deactivate()
    // again sends a second HLTA with no active PICC and can poison the next scan.
}

void handleCardTimeout()
{
    const uint32_t now = millis();
    if (gCardPresent && (now - gLastSeenAtMs > kCardLostTimeoutMs)) {
        gCardPresent = false;
        gCurrentCard.clear();
        gDetectedPicc = {};
        setState(UiState::NoCard, "Tag removed");
        Serial.println(F("[NFC] Tag removed."));
        return;
    }
    if (!gCardPresent && gUiState == UiState::NoCard &&
        (now - gStateChangedAtMs > kNoCardMessageMs)) {
        setState(UiState::Scanning, "Waiting for NFC-A tag");
    }
}

void pollNfc()
{
    if (gPageMode != PageMode::Scan || !gInitOk || !gUnits || !gNfcA) {
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

bool readerHealthOk()
{
    if (!gNfcUnit) {
        return false;
    }

    uint8_t chipType = 0;
    uint8_t chipRevision = 0;
    uint8_t operation = 0;
    t_embed::board::deselectSharedSpiDevices();
    return gNfcUnit->readICIdentity(chipType, chipRevision) &&
           chipType == m5::unit::st25r3916::VALID_IDENTIFY_TYPE && chipRevision != 0 &&
           gNfcUnit->readOperationControl(operation) &&
           (operation & m5::unit::st25r3916::regval::en);
}

void recoverReader(const char* reason)
{
    gLastRecoveryAttemptMs = millis();
    gInitOk = false;
    gCardPresent = false;
    gCurrentCard.clear();
    gDetectedPicc = {};
    setState(UiState::Init, String("Recovering reader: ") + reason);
    Serial.print(F("[NFC] Recovering NFC-A reader: "));
    Serial.println(reason);

    gInitOk = initNfc(false);
    if (gInitOk) {
        gHealthFailureCount = 0;
        gLastPollAtMs = 0;
        setState(UiState::Scanning, "Reader recovered; waiting for tag");
        Serial.println(F("[NFC] Reader recovery succeeded."));
    } else {
        setState(UiState::FatalError, "Reader recovery failed; retrying");
        Serial.println(F("[NFC] Reader recovery failed."));
    }
}

void maintainReaderHealth()
{
    if (gPageMode != PageMode::Scan) {
        return;
    }

    const uint32_t now = millis();
    if (!gInitOk || !gUnits || !gNfcUnit || !gNfcA) {
        if (now - gLastRecoveryAttemptMs >= kRecoveryRetryMs) {
            recoverReader("offline");
        }
        return;
    }

    if (now - gLastHealthCheckMs < kHealthCheckIntervalMs) {
        return;
    }
    gLastHealthCheckMs = now;

    if (readerHealthOk()) {
        gHealthFailureCount = 0;
        return;
    }

    ++gHealthFailureCount;
    Serial.print(F("[NFC] Health check failed "));
    Serial.print(gHealthFailureCount);
    Serial.print(F("/"));
    Serial.println(kHealthFailuresToReset);
    if (gHealthFailureCount >= kHealthFailuresToReset) {
        recoverReader("health check");
    }
}

bool readPageWithRetry(const m5::nfc::a::PICC& picc, const uint8_t page, uint8_t out[4])
{
    if (gNfcA->read4(out, page)) {
        return true;
    }
    return gNfcA->reactivate(picc) && gNfcA->read4(out, page);
}

void copyDetectedTag()
{
    if (!gCardPresent || !gDetectedPicc.valid() || !gNfcA) {
        setState(UiState::CopyFail, "No active tag to copy");
        return;
    }
    if (!isEmulatableType(gDetectedPicc.type, gDetectedPicc.size)) {
        setState(UiState::Unsupported, "Only Ultralight / NTAG2xx can be emulated");
        return;
    }
    const uint16_t memorySize = gDetectedPicc.totalSize();
    if (!memorySize || memorySize > kMaxTagMemoryBytes || (memorySize % 4) != 0) {
        setState(UiState::Unsupported, "Tag memory layout is not supported");
        return;
    }

    int8_t slot = findSavedUid(gDetectedPicc);
    const bool replacing = slot >= 0;
    if (slot < 0) {
        slot = firstEmptySlot();
    }
    if (slot < 0) {
        setState(UiState::LibraryFull, "Saved tag library is full");
        return;
    }

    setState(UiState::Copying, "Keep tag still while memory is read");
    redrawScreen();

    SavedTag candidate{};
    candidate.magic = kSavedTagMagic;
    candidate.version = kSavedTagVersion;
    candidate.type = static_cast<uint8_t>(gDetectedPicc.type);
    candidate.uidSize = gDetectedPicc.size;
    memcpy(candidate.uid, gDetectedPicc.uid, gDetectedPicc.size);
    candidate.atqa = gDetectedPicc.atqa;
    candidate.sak = gDetectedPicc.sak;
    candidate.memorySize = memorySize;

    t_embed::board::deselectSharedSpiDevices();
    if (!gNfcA->reactivate(gDetectedPicc)) {
        setState(UiState::CopyFail, "Tag moved before copy started");
        return;
    }

    bool readOk = true;
    for (uint16_t page = 0; page < memorySize / 4; ++page) {
        if (!readPageWithRetry(gDetectedPicc, static_cast<uint8_t>(page),
                               candidate.memory + page * 4)) {
            readOk = false;
            Serial.print(F("[NFC] Failed reading page "));
            Serial.println(page);
            break;
        }
        delay(1);
    }
    (void)gNfcA->deactivate();

    if (!readOk) {
        setState(UiState::CopyFail, "Could not read all tag pages");
        return;
    }

    candidate.checksum = savedTagChecksum(candidate);
    gSavedTags[slot] = candidate;
    const bool persisted = persistSlot(static_cast<uint8_t>(slot));
    recountSavedTags();
    gSelectedSlot = slot;

    String message = replacing ? "Updated saved tag " : "Saved as tag ";
    message += String(slot + 1);
    if (!persisted) {
        message += " (RAM only)";
    }
    setState(UiState::CopySuccess, message);
    Serial.print(F("[NFC] "));
    Serial.println(message);
}

bool configureEmulationRfProfile()
{
    if (!gNfcUnit) {
        return false;
    }

    using namespace m5::unit::st25r3916::command;
    const bool configured =
        gNfcUnit->writeReceiverConfiguration1(kEmulationRxConfig1) &&
        gNfcUnit->writeReceiverConfiguration2(kEmulationRxConfig2) &&
        gNfcUnit->writeReceiverConfiguration3(0x00) &&
        gNfcUnit->writeReceiverConfiguration4(0x00) &&
        gNfcUnit->writeCorrelatorConfiguration1(kEmulationCorrelatorConfig1) &&
        gNfcUnit->writeCorrelatorConfiguration2(0x00) &&
        gNfcUnit->writeMaskReceiveTimer(kEmulationMaskReceiveTimer) &&
        gNfcUnit->writeExternalFieldDetectorActivationThreshold(kEmulationFieldActivate) &&
        gNfcUnit->writeExternalFieldDetectorDeactivationThreshold(kEmulationFieldDeactivate) &&
        gNfcUnit->writeAuxiliaryModulationSetting(kEmulationAuxModulation) &&
        gNfcUnit->writePassiveTargetModulation(kEmulationTargetModulation) &&
        gNfcUnit->writeEMDSuppressionConfiguration(kEmulationEmdSuppression) &&
        gNfcUnit->writeAntennaTuningControl1(kEmulationAntennaTuneA) &&
        gNfcUnit->writeAntennaTuningControl2(kEmulationAntennaTuneB) &&
        gNfcUnit->writeDirectCommand(CMD_RESET_RX_GAIN);

    Serial.println(configured
        ? F("[NFC] Applied stable NFC-A listener RF profile.")
        : F("[NFC] Failed to apply NFC-A listener RF profile."));
    return configured;
}

bool startEmulation()
{
    if (gSelectedSlot < 0 || !validateSavedTag(gSavedTags[gSelectedSlot])) {
        gLibraryNotice = "INVALID SAVED TAG";
        markDirty();
        return false;
    }

    SavedTag& tag = gSavedTags[gSelectedSlot];
    m5::nfc::a::PICC picc{};
    if (!picc.emulate(static_cast<m5::nfc::a::Type>(tag.type), tag.uid, tag.uidSize)) {
        gLibraryNotice = "EMULATION SETUP FAILED";
        markDirty();
        return false;
    }
    // PICC::emulate() fills type defaults; a copied tag should expose the
    // original anti-collision parameters captured by the reader instead.
    picc.atqa = tag.atqa;
    picc.sak = tag.sak;

    gInitOk = false;
    t_embed::board::deselectSharedSpiDevices();
    memcpy(gEmulationMemory, tag.memory, tag.memorySize);
    if (!initNfc(true) || !gEmuA ||
        !gEmuA->begin(picc, gEmulationMemory, tag.memorySize) ||
        !configureEmulationRfProfile()) {
        Serial.println(F("[NFC] Failed to start card emulation."));
        destroyNfcObjects();
        gInitOk = false;
        gLibraryNotice = "EMULATION START FAILED";
        gPageMode = PageMode::Saved;
        markDirty();
        return false;
    }

    gInitOk = true;
    gEmulatingSlot = gSelectedSlot;
    gLastEmulationState = gEmuA->state();
    gPageMode = PageMode::Emulating;
    gActionIndex = 0;
    gCardPresent = false;
    gCurrentCard.clear();
    gDetectedPicc = {};
    gLibraryNotice = "";
    markDirty();

    Serial.print(F("[NFC] Emulating UID "));
    Serial.println(uidString(tag.uid, tag.uidSize));
    return true;
}

void stopEmulation()
{
    if (gEmuA) {
        (void)gEmuA->end();
    }

    destroyNfcObjects();
    gInitOk = false;
    gEmulatingSlot = -1;
    gLastEmulationState = m5::nfc::EmulationLayerA::State::None;
    openSavedLibrary("EMULATION STOPPED");
    Serial.println(F("[NFC] Card emulation stopped."));
}

void updateEmulation()
{
    if (gPageMode != PageMode::Emulating || !gUnits || !gEmuA) {
        return;
    }
    // Card emulation is an intentional active mode; do not auto-sleep mid-session.
    g.lastUserInputMs = millis();
    // Drain up to three already-pending IRQ transitions in one pass. This is
    // important after automatic anti-collision: the reader can issue its first
    // Type 2 command well inside the factory UI's normal loop period.
    for (uint8_t pass = 0; pass < 3; ++pass) {
        t_embed::board::deselectSharedSpiDevices();
        gUnits->update();
        gEmuA->update();
        const auto state = gEmuA->state();
        if (state != gLastEmulationState) {
            gLastEmulationState = state;
            markDirty();
        }
        if (digitalRead(BOARD_NFC_IRQ) == LOW) {
            break;
        }
    }
}

void handleEncoder()
{
    const int32_t delta = (g.encRaw - gEncSnapshot) / 2;
    if (delta == 0) {
        return;
    }
    gEncSnapshot += delta * 2;
    const int32_t count = actionCount();
    int32_t next = static_cast<int32_t>(gActionIndex) + delta;
    next %= count;
    if (next < 0) {
        next += count;
    }
    if (static_cast<uint8_t>(next) != gActionIndex) {
        gActionIndex = static_cast<uint8_t>(next);
        markDirty();
    }
}

void handleBootButton()
{
    switch (gPageMode) {
        case PageMode::Scan:
            if (gActionIndex == 1) {
                requestExitSubPage();
            } else if (gCardPresent) {
                copyDetectedTag();
            }
            break;

        case PageMode::Saved:
            if (!gSavedCount) {
                if (gActionIndex == 0) {
                    restartScan();
                } else {
                    requestExitSubPage();
                }
                break;
            }
            switch (gActionIndex) {
                case 0: (void)startEmulation(); break;
                case 1:
                    gPageMode = PageMode::DeleteConfirm;
                    gActionIndex = 0;
                    markDirty();
                    break;
                case 2: restartScan(); break;
                case 3: requestExitSubPage(); break;
            }
            break;

        case PageMode::Emulating:
            if (gActionIndex == 0) {
                stopEmulation();
            } else {
                requestExitSubPage();
            }
            break;

        case PageMode::DeleteConfirm:
            if (gActionIndex == 0) {
                openSavedLibrary("DELETE CANCELLED");
            } else if (gSelectedSlot >= 0) {
                const int8_t deleted = gSelectedSlot;
                const bool persisted = eraseSlot(static_cast<uint8_t>(deleted));
                gSelectedSlot = firstSavedSlot();
                openSavedLibrary(persisted || !gPrefsReady ? "TAG DELETED" : "DELETED FROM RAM");
                Serial.print(F("[NFC] Deleted saved slot "));
                Serial.println(deleted + 1);
            }
            break;
    }
}

void handleUserButton()
{
    switch (gPageMode) {
        case PageMode::Scan:
            openSavedLibrary();
            break;
        case PageMode::Saved:
            if (gSavedCount > 1) {
                gSelectedSlot = nextSavedSlot(gSelectedSlot);
                gLibraryNotice = "";
                markDirty();
            } else if (!gSavedCount) {
                restartScan();
            }
            break;
        case PageMode::Emulating:
            gActionIndex = 0;
            markDirty();
            break;
        case PageMode::DeleteConfirm:
            openSavedLibrary("DELETE CANCELLED");
            break;
    }
}

void handleButtons()
{
    if (g.encBtn.event) {
        g.encBtn.event = false;
        handleBootButton();
    }
    if (g.usrBtn.event) {
        g.usrBtn.event = false;
        handleUserButton();
    }
}

}  // namespace

void init()
{
    gPageMode = PageMode::Scan;
    gUiState = UiState::Init;
    gCurrentCard.clear();
    gDetectedPicc = {};
    gDetailLine = "";
    gLibraryNotice = "";
    gScreenDirty = true;
    gCanvasReady = false;
    gInitOk = false;
    gNfcBusAttached = false;
    gCardPresent = false;
    gActionIndex = 0;
    gLastPollAtMs = 0;
    gLastSeenAtMs = 0;
    gStateChangedAtMs = millis();
    gLastUiDrawMs = 0;
    gLastHealthCheckMs = 0;
    gLastRecoveryAttemptMs = 0;
    gHealthFailureCount = 0;
    gEncSnapshot = g.encRaw;
    gEmulatingSlot = -1;
    gLastEmulationState = m5::nfc::EmulationLayerA::State::None;

    loadSavedTags();

    gCanvas.deleteSprite();
    gCanvas.setColorDepth(16);
    gCanvasReady = (gCanvas.createSprite(tft.width(), tft.height()) != nullptr);
    if (!gCanvasReady) {
        Serial.println(F("[NFC] Sprite allocation failed, using direct TFT redraw."));
    }

    t_embed::board::deselectSharedSpiDevices();
    setState(UiState::Init, "Power rails and display ready");
    if (!initNfc(false)) {
        gLastRecoveryAttemptMs = millis();
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
    maintainReaderHealth();
    updateEmulation();
}

void render()
{
    if (!gScreenDirty) {
        return;
    }

    // LCD and NFC share the same SPI bus. A full-screen sprite transfer while
    // the external RF field is present can exceed the Type 2 Tag response
    // window. Keep the last screen until the listener returns to Off; also
    // avoid starting a redraw when a field IRQ is already pending.
    if (gPageMode == PageMode::Emulating &&
        (gLastEmulationState != m5::nfc::EmulationLayerA::State::Off ||
         digitalRead(BOARD_NFC_IRQ) == HIGH)) {
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
    gEmulatingSlot = -1;
}

}  // namespace page_nfc
