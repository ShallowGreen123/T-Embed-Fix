#pragma once
#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>

namespace page_nfc {

namespace {
    SPIClass nfcSPI(HSPI);
    m5::unit::UnitUnified  units;
    m5::unit::UnitST25R3916* nfcUnit = nullptr;
    m5::nfc::NFCLayerA*      nfcA    = nullptr;

    constexpr uint32_t kScanIntervalMs   = 180;
    constexpr uint32_t kCardLostMs       = 700;
    constexpr uint32_t kNoCardMessageMs  = 1200;
    constexpr uint32_t kSpiClockHz       = 10000000;

    enum class NfcState : uint8_t { Scanning, CardFound, NoCard, Error };

    bool      gInitOk         = false;
    bool      gDirty          = true;
    NfcState  gState          = NfcState::Scanning;
    bool      gCardPresent    = false;
    uint32_t  gLastPollMs     = 0;
    uint32_t  gLastSeenMs     = 0;
    uint32_t  gStateChangedMs = 0;

    String gUid  = "--";
    String gType = "--";
    uint16_t gAtqa = 0;
    uint8_t  gSak  = 0;

    void setState(NfcState s) {
        if (gState != s) { gState = s; gStateChangedMs = millis(); gDirty = true; }
    }

    void pollNfc() {
        const uint32_t now = millis();
        if (now - gLastPollMs < kScanIntervalMs) return;
        gLastPollMs = now;

        t_embed::board::deselectSharedSpiDevices();
        units.update();

        m5::nfc::a::PICC picc{};
        if (nfcA && nfcA->detect(picc, 100U)) {
            String uid = String(picc.uidAsString().c_str());
            if (!gCardPresent || uid != gUid) {
                if (nfcA->identify(picc)) {
                    gUid  = uid;
                    gType = String(picc.typeAsString().c_str());
                    gAtqa = picc.atqa;
                    gSak  = picc.sak;
                } else {
                    gUid  = uid;
                    gType = "Unknown";
                    gAtqa = 0; gSak = 0;
                }
                Serial.print(F("[NFC] UID: ")); Serial.println(gUid);
                gCardPresent = true;
                setState(NfcState::CardFound);
            }
            gLastSeenMs = now;
            (void)nfcA->deactivate();
        } else {
            if (gCardPresent && (now - gLastSeenMs > kCardLostMs)) {
                gCardPresent = false;
                gUid = "--"; gType = "--"; gAtqa = 0; gSak = 0;
                setState(NfcState::NoCard);
            }
            if (!gCardPresent && gState == NfcState::NoCard &&
                (now - gStateChangedMs > kNoCardMessageMs)) {
                setState(NfcState::Scanning);
            }
        }
    }
}  // namespace

void init() {
    gInitOk = false;
    gDirty  = true;
    gCardPresent = false;
    gUid = "--"; gType = "--"; gAtqa = 0; gSak = 0;
    gLastPollMs = 0; gLastSeenMs = 0; gStateChangedMs = millis();
    gState = NfcState::Scanning;

    t_embed::board::deselectSharedSpiDevices();
    delay(5);

    nfcSPI.begin(BOARD_NFC_SCK, BOARD_NFC_MISO, BOARD_NFC_MOSI);
    delay(20);

    if (nfcUnit) { delete nfcUnit; nfcUnit = nullptr; }
    if (nfcA)    { delete nfcA;    nfcA    = nullptr; }
    nfcUnit = new m5::unit::UnitST25R3916(BOARD_NFC_CS);
    nfcA    = new m5::nfc::NFCLayerA(*nfcUnit);

    auto cfg = nfcUnit->config();
    cfg.mode          = m5::nfc::NFC::A;
    cfg.vdd_voltage_5V = false;
    cfg.using_irq     = true;
    cfg.irq           = BOARD_NFC_IRQ;
    cfg.emulation     = false;
    nfcUnit->config(cfg);

    SPISettings settings{kSpiClockHz, MSBFIRST, SPI_MODE1};
    if (!units.add(*nfcUnit, nfcSPI, settings) || !units.begin()) {
        Serial.println(F("[NFC] Init failed."));
        gState = NfcState::Error;
    } else {
        gInitOk = true;
        Serial.println(F("[NFC] ST25R3916 ready."));
    }

    tft.fillRect(0, 0, tft.width(), 22, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.drawCentreString("NFC ST25R3916", tft.width() / 2, 4, 2);
    tft.fillRect(0, 22, tft.width(), tft.height() - 22, TFT_BLACK);
}

void update() {
    if (!gInitOk) return;
    pollNfc();
}

void render() {
    if (!gDirty) return;
    gDirty = false;

    const int16_t W = tft.width();
    tft.fillRect(0, 22, W, tft.height() - 22, TFT_BLACK);

    if (gState == NfcState::Error) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawCentreString("INIT FAILED", W / 2, 80, 4);
        return;
    }

    // State label
    uint16_t stateColor;
    const char* stateLabel;
    switch (gState) {
        case NfcState::CardFound: stateColor = TFT_GREEN;    stateLabel = "CARD FOUND"; break;
        case NfcState::NoCard:    stateColor = TFT_DARKGREY; stateLabel = "NO CARD";    break;
        default:                  stateColor = TFT_YELLOW;   stateLabel = "SCANNING";   break;
    }
    tft.setTextColor(stateColor, TFT_BLACK);
    tft.drawString(stateLabel, 8, 28, 4);

    // Info rows
    auto drawRow = [](int16_t y, const char* label, const String& val) {
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString(label, 8, y, 1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(val, 90, y, 1);
    };

    drawRow(86,  "UID",  gUid);
    drawRow(100, "Type", gType);
    char buf[12];
    snprintf(buf, sizeof(buf), "%04X / %02X", gAtqa, gSak);
    drawRow(114, "ATQA/SAK", String(buf));

    // Footer
    tft.fillRect(0, tft.height() - 12, W, 12, 0x2104);
    tft.setTextColor(TFT_DARKGREY, 0x2104);
    tft.drawCentreString("Place NFC-A tag on antenna  |  USR=back", W / 2, tft.height() - 11, 1);
}

void deinit() {
    nfcSPI.end();
    t_embed::board::deselectSharedSpiDevices();
    delete nfcUnit; nfcUnit = nullptr;
    delete nfcA;    nfcA    = nullptr;
    gInitOk = false;
}

}  // namespace page_nfc
