/*
Thanks to thoses developers for their projects:
* @7h30th3r0n3 : https://github.com/7h30th3r0n3/Evil-M5Core2 and https://github.com/7h30th3r0n3/PwnGridSpam
* @viniciusbo : https://github.com/viniciusbo/m5-palnagotchi
* @sduenasg : https://github.com/sduenasg/pio_palnagotchi

Thanks to @bmorcelli for his help doing a better code.
*/
#include "ui.h"
#include "../wifi/sniffer.h"

#define ROW_SIZE 40
#define PADDING 10

int32_t display_w;
int32_t display_h;
int32_t canvas_h;
int32_t canvas_center_x;
int32_t canvas_top_h;
int32_t canvas_bot_h;
int32_t canvas_peers_menu_h;
int32_t canvas_peers_menu_w;

uint8_t menu_current_cmd = 0;
uint8_t menu_current_opt = 0;

// ── Custom gold accent (warm golden yellow, not harsh red) ──
static uint16_t goldAccent() {
    return tft.color565(255, 195, 30);
}

static uint16_t warmDim(const uint16_t c, uint8_t amount = 30) {
    // Warm-dim: reduce blue more than red/green for a warm tint
    uint8_t r = (c >> 8) & 0xF8;
    uint8_t g = (c >> 3) & 0xFC;
    uint8_t b = (c << 3) & 0xF8;
    r = (r > amount) ? r - amount : 0;
    g = (g > amount + 5) ? g - (amount + 5) : 0;
    b = (b > amount + 15) ? b - (amount + 15) : 0;
    return tft.color565(r, g, b);
}

// ── Color helpers ──
static uint16_t dim(const uint16_t c, uint8_t amount = 40) {
    // Dim a color by reducing R/G/B components
    uint8_t r = (c >> 8) & 0xF8;
    uint8_t g = (c >> 3) & 0xFC;
    uint8_t b = (c << 3) & 0xF8;
    r = (r > amount) ? r - amount : 0;
    g = (g > amount) ? g - amount : 0;
    b = (b > amount) ? b - amount : 0;
    return tft.color565(r, g, b);
}

void initUi() {
    tft.setTextSize(1);
    tft.fillScreen(buttergotchiConfig.bgColor);
    tft.setTextColor(buttergotchiConfig.priColor);

    display_w = tftWidth;
    display_h = tftHeight;
    canvas_center_x = display_w / 2;
    // Top bar: 2 lines of text (CH, HS) + separator = 26px
    canvas_top_h = 26;
    // Bottom bar: 1 line of branding + separator = 16px
    canvas_bot_h = display_h - 16;
    // Mood area between bars
    canvas_h = canvas_bot_h - canvas_top_h;
    canvas_peers_menu_h = canvas_h;
    canvas_peers_menu_w = display_w * .8;
}

String getRssiBars(signed int rssi) {
    if (rssi == -1000) return "";
    if (rssi >= -67) return "[####]";
    if (rssi >= -70) return "[### ]";
    if (rssi >= -80) return "[##  ]";
    return "[#   ]";
}

// ── Top HUD bar: CH / HS / Uptime / Battery ──
void drawTopCanvas() {
    uint16_t bg = buttergotchiConfig.bgColor;
    uint16_t fg = buttergotchiConfig.priColor;
    uint16_t gold = goldAccent();

    tft.fillRect(0, 0, display_w, canvas_top_h, bg);

    // Left: Channel + Handshakes
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(fg, bg);
    tft.drawString("CH " + String(ch), 4, 2);

    // Handshake count in gold if > 0, dimmed fg otherwise
    tft.setTextColor(num_HS > 0 ? gold : dim(fg, 50), bg);
    tft.drawString("HS " + String(num_HS), 4, 14);

    // Right: Uptime + Battery
    tft.setTextDatum(TR_DATUM);
    unsigned long elapsed = millis() / 1000;
    int h = elapsed / 3600;
    int m = (elapsed % 3600) / 60;
    int s = elapsed % 60;

    tft.setTextColor(fg, bg);
    char upStr[16];
    snprintf(upStr, sizeof(upStr), "%02d:%02d:%02d", h, m, s);
    tft.drawString(upStr, display_w - 4, 2);

    // Battery with percentage
    int bat = getBattery();
    uint16_t batColor = (bat > 20) ? fg : gold;
    tft.setTextColor(batColor, bg);
    tft.drawString(String(bat) + "%", display_w - 4, 14);

    // Warm golden separator line
    tft.drawLine(0, canvas_top_h - 1, display_w, canvas_top_h - 1, warmDim(gold, 60));
}

// ── Bottom status bar: branding + friend count ──
void drawBottomCanvas() {
    uint16_t bg = buttergotchiConfig.bgColor;
    uint16_t fg = buttergotchiConfig.priColor;
    uint16_t gold = goldAccent();

    tft.fillRect(0, canvas_bot_h, display_w, display_h - canvas_bot_h, bg);

    // Branding on left — warm gold
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(warmDim(gold, 40), bg);
    tft.drawString("Buttergotchi", 4, canvas_bot_h + 4);

    // Friend count on right
    uint8_t total = getPwngridTotalPeers();
    uint8_t active = getPwngridRunTotalPeers();
    if (total > 0) {
        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(fg, bg);
        tft.drawString(String(active) + "/" + String(total) + " pals", display_w - 4, canvas_bot_h + 4);
    }

    // Warm golden separator line
    tft.drawLine(0, canvas_bot_h, display_w, canvas_bot_h, warmDim(gold, 60));
}

// ── Mood hero display ──
void drawMood(String face, String phrase, bool broken) {
    uint16_t bg = buttergotchiConfig.bgColor;
    uint16_t fg = broken ? tft.color565(255, 0, 0) : buttergotchiConfig.priColor;
    uint16_t phraseColor = dim(fg, 50);

    // Clear mood area (between top and bottom bars)
    tft.fillRect(0, canvas_top_h + 1, display_w, canvas_bot_h - canvas_top_h - 1, bg);

    // Mood face — hero, large, centered
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(FG + 1);
    tft.setTextColor(fg, bg);
    tft.drawCentreString(face, canvas_center_x, canvas_top_h + (canvas_bot_h - canvas_top_h) / 3, SMOOTH_FONT);

    // Mood phrase — below face with breathing room
    if (phrase.length() > 0) {
        tft.setTextDatum(BC_DATUM);
        tft.setTextSize(1);
        tft.setTextColor(phraseColor, bg);
        tft.drawCentreString(phrase, canvas_center_x, canvas_bot_h - 28, SMOOTH_FONT);
    }
}

// ── Friend panel (up to 3 peers) ──
void drawFriendPanel() {
    auto peers = getPwngridPeers();
    uint16_t bg = buttergotchiConfig.bgColor;
    uint16_t fg = buttergotchiConfig.priColor;
    uint16_t sec = buttergotchiConfig.secColor;

    // Count visible peers
    int visible = 0;
    for (auto &p : peers) { if (!p.gone) visible++; }

    // If no active peers, show "looking" text
    if (visible == 0) {
        tft.setTextSize(1);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(dim(fg, 60), bg);
        tft.drawCentreString("~ looking for friends ~", canvas_center_x, canvas_bot_h - 18, SMOOTH_FONT);
        return;
    }

    // Draw up to 3 peers
    int y = canvas_bot_h - 22;
    int lineH = 10;
    int count = 0;
    for (auto &p : peers) {
        if (p.gone) continue;
        if (count >= 3) break;

        tft.fillRect(4, y, display_w - 8, lineH, bg);

        // Peer name in secondary color
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(sec, bg);
        String name = p.name.substring(0, 12);
        tft.drawString(name, 4, y);

        // RSSI signal bars (visual, not text)
        int numBars;
        if (p.rssi >= -67) numBars = 4;
        else if (p.rssi >= -70) numBars = 3;
        else if (p.rssi >= -80) numBars = 2;
        else numBars = 1;

        for (int b = 0; b < numBars; b++) {
            int x = display_w - 4 - (numBars - b) * 8;
            int barH = 2 + b * 2;
            uint16_t barColor = (b < numBars / 2) ? dim(fg, 50) : fg;
            tft.fillRect(x, y + lineH - 2 - barH, 5, barH, barColor);
        }

        y += lineH;
        count++;
    }
}

// ── Time string (for HUD) ──
void drawTime() {
    // Time is now embedded in drawTopCanvas() — this is a no-op passthrough
    // kept for API compatibility
    drawTopCanvas();
}

// ── Footer data (legacy, now embedded in drawBottomCanvas) ──
void drawFooterData(uint8_t friends_run, uint8_t friends_tot, String last_friend_name, signed int rssi) {
    // Legacy compatibility — calls modern version
    drawBottomCanvas();
}

// ── Main UI update ──
void updateUi(bool show_toolbars) {
    uint8_t mood_id = getCurrentMoodId();
    String mood_face = getCurrentMoodFace();
    String mood_phrase = getCurrentMoodPhrase();
    bool mood_broken = isCurrentMoodBroken();

    if (show_toolbars) {
        drawTopCanvas();
        if (tftHeight > 150) drawTime();
        drawBottomCanvas();
    }

    drawMood(mood_face, mood_phrase, mood_broken);
    drawFriendPanel();

#if defined(HAS_TOUCH)
    TouchFooter();
#endif
}
