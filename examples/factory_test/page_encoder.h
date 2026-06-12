#pragma once

namespace page_encoder {

namespace {
    bool     gDirty       = true;
    int32_t  gTurnCount   = 0;
    uint32_t gEncPress    = 0;
    uint32_t gUsrPress    = 0;
    int32_t  gEncSnapshot = 0;

    void drawButtonPill(int16_t cx, int16_t cy, bool pressed, const char* label) {
        const uint16_t bg = pressed ? TFT_GREEN  : 0x2104;
        const uint16_t fg = pressed ? TFT_BLACK  : TFT_WHITE;
        tft.fillRoundRect(cx - 58, cy - 14, 116, 28, 8, bg);
        tft.drawRoundRect(cx - 58, cy - 14, 116, 28, 8,
                          pressed ? TFT_WHITE : TFT_DARKGREY);
        tft.setTextColor(fg, bg);
        tft.drawCentreString(label, cx, cy - 6, 2);
    }
}  // namespace

void init() {
    gTurnCount   = 0;
    gEncPress    = 0;
    gUsrPress    = 0;
    gEncSnapshot = g.encRaw;

    tft.fillRect(0, 0, tft.width(), 22, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.drawCentreString("Encoder / Keys", tft.width() / 2, 4, 2);
    tft.fillRect(0, 22, tft.width(), tft.height() - 22, TFT_BLACK);
    gDirty = true;
}

void update() {
    const int32_t cur   = g.encRaw;
    const int32_t delta = (cur - gEncSnapshot) / 2;
    if (delta != 0) {
        gEncSnapshot += delta * 2;
        gTurnCount   += delta;
        gDirty = true;
    }
    if (g.encBtn.event) {
        g.encBtn.event = false;
        ++gEncPress;
        gDirty = true;
    }
    // usrBtn event consumed by main loop for back navigation —
    // just track state for the pill display
    static bool lastUsr = false;
    if (g.usrBtn.pressed != lastUsr) {
        lastUsr = g.usrBtn.pressed;
        if (g.usrBtn.pressed) ++gUsrPress;
        gDirty = true;
    }
}

void render() {
    if (!gDirty) return;
    gDirty = false;

    const int16_t W  = tft.width();
    const int16_t MX = W / 2;
    tft.fillRect(0, 22, W, tft.height() - 22, TFT_BLACK);

    // Turn count (large)
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", (long)gTurnCount);
    tft.drawCentreString(buf, MX, 28, 6);

    // Divider
    tft.drawFastVLine(MX, 90, 60, TFT_DARKGREY);

    // Left: encoder button
    drawButtonPill(MX / 2, 115, g.encBtn.pressed,
                   g.encBtn.pressed ? "ENC PRESSED" : "ENC idle");
    snprintf(buf, sizeof(buf), "x%lu", (unsigned long)gEncPress);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString(buf, MX / 2, 138, 1);

    // Right: user button (note: USR=back, show count anyway)
    drawButtonPill(MX + MX / 2, 115, g.usrBtn.pressed,
                   g.usrBtn.pressed ? "USR PRESSED" : "USR idle");
    snprintf(buf, sizeof(buf), "x%lu", (unsigned long)gUsrPress);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString(buf, MX + MX / 2, 138, 1);

    // Footer
    tft.fillRect(0, tft.height() - 12, W, 12, 0x2104);
    tft.setTextColor(TFT_DARKGREY, 0x2104);
    tft.drawCentreString("USR=back (also counted above)", W / 2, tft.height() - 11, 1);
}

void deinit() {}

}  // namespace page_encoder
