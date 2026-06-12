#pragma once
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>

namespace page_ir {

namespace {
    constexpr uint16_t kCapBufSize = 1024;
    constexpr uint8_t  kTimeoutMs  = 50;
    // NEC test code to send on encoder button press
    constexpr uint64_t kTestCode = 0x20DF10EFULL;
    constexpr uint16_t kTestBits = 32;

    IRsend*  irsend = nullptr;
    IRrecv*  irrecv = nullptr;
    decode_results irResult;

    bool     gInitOk  = false;
    bool     gDirty   = true;

    struct RxInfo { bool valid=false; String proto="--"; String val="--"; uint16_t bits=0; uint32_t cnt=0; };
    struct TxInfo { bool valid=false; String val="--"; uint32_t cnt=0; };
    RxInfo gRx;
    TxInfo gTx;

    void doSend() {
        if (!gInitOk || !irsend) return;
        irrecv->pause();
        irsend->sendNEC(kTestCode, kTestBits);
        gTx.valid = true;
        gTx.val   = "0x" + String(uint64ToString(kTestCode, 16));
        ++gTx.cnt;
        gDirty = true;
        delay(5);
        irrecv->resume();
    }
}  // namespace

void init() {
    gInitOk = false;
    gRx = RxInfo{};
    gTx = TxInfo{};

    if (irsend) { delete irsend; irsend = nullptr; }
    if (irrecv) { delete irrecv; irrecv = nullptr; }

    irsend = new IRsend(BOARD_IR_TX);
    irrecv = new IRrecv(BOARD_IR_RX, kCapBufSize, kTimeoutMs, true);

    irsend->begin();
    irrecv->enableIRIn();
    gInitOk = true;

    tft.fillRect(0, 0, tft.width(), 22, 0x04FF);
    tft.setTextColor(TFT_WHITE, 0x04FF);
    tft.drawCentreString("IR TX / RX", tft.width() / 2, 4, 2);
    tft.fillRect(0, 22, tft.width(), tft.height() - 22, TFT_BLACK);
    gDirty = true;
}

void update() {
    if (!gInitOk) return;

    if (g.encBtn.event) {
        g.encBtn.event = false;
        doSend();
    }

    if (irrecv->decode(&irResult)) {
        gRx.valid = (irResult.decode_type != UNKNOWN) && (irResult.bits > 0);
        gRx.proto = String(typeToString(irResult.decode_type, irResult.repeat));
        gRx.val   = "0x" + String(uint64ToString(irResult.value, 16));
        gRx.bits  = irResult.bits;
        ++gRx.cnt;
        gDirty = true;
        irrecv->resume();
    }
}

void render() {
    if (!gDirty) return;
    gDirty = false;

    const int16_t W = tft.width();
    tft.fillRect(0, 22, W, tft.height() - 22, TFT_BLACK);

    if (!gInitOk) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawCentreString("INIT FAILED", W / 2, 80, 4);
        return;
    }

    // TX panel
    const int16_t txY = 26;
    const int16_t panW = W - 12;
    tft.fillRoundRect(6, txY, panW, 52, 6, 0x18E3);
    tft.drawRoundRect(6, txY, panW, 52, 6, TFT_DARKGREY);
    tft.setTextColor(TFT_ORANGE, 0x18E3);
    tft.drawString("TX", 14, txY + 4, 2);
    tft.setTextColor(TFT_WHITE, 0x18E3);
    tft.drawString(gTx.valid ? "NEC" : "--", 50, txY + 4, 2);
    tft.setTextColor(TFT_LIGHTGREY, 0x18E3);
    tft.drawString(gTx.val.c_str(), 14, txY + 24, 2);
    char buf[24];
    snprintf(buf, sizeof(buf), "x%lu", (unsigned long)gTx.cnt);
    tft.setTextColor(TFT_DARKGREY, 0x18E3);
    tft.drawRightString(buf, W - 14, txY + 28, 1);

    // Hint between panels
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString("ENCBTN=send NEC", W / 2, 84, 1);

    // RX panel
    const int16_t rxY = 94;
    tft.fillRoundRect(6, rxY, panW, 52, 6, 0x12CB);
    tft.drawRoundRect(6, rxY, panW, 52, 6, TFT_DARKGREY);
    tft.setTextColor(TFT_GREENYELLOW, 0x12CB);
    tft.drawString("RX", 14, rxY + 4, 2);
    tft.setTextColor(TFT_WHITE, 0x12CB);
    tft.drawString(gRx.valid ? gRx.proto.c_str() : "waiting...", 50, rxY + 4, 2);
    tft.setTextColor(TFT_LIGHTGREY, 0x12CB);
    tft.drawString(gRx.valid ? gRx.val.c_str() : "--", 14, rxY + 24, 2);
    snprintf(buf, sizeof(buf), "%u bits  x%lu", gRx.bits, (unsigned long)gRx.cnt);
    tft.setTextColor(TFT_DARKGREY, 0x12CB);
    tft.drawRightString(buf, W - 14, rxY + 28, 1);

    // Footer
    tft.fillRect(0, tft.height() - 12, W, 12, 0x2104);
    tft.setTextColor(TFT_DARKGREY, 0x2104);
    tft.drawCentreString("USR=back", W / 2, tft.height() - 11, 1);
}

void deinit() {
    if (irrecv) irrecv->disableIRIn();
    delete irsend; irsend = nullptr;
    delete irrecv; irrecv = nullptr;
    gInitOk = false;
}

}  // namespace page_ir
