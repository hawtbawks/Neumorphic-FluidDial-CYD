// neu_ui.cpp — the neumorphic UI kit.  Port of simulator/js/neu_ui.js (the spec).
#include "neu_ui.h"
#include "../Comms.h"
#include <math.h>

// ---------- colour ----------
// Expands RGB565 to 888 with rounding, blends, then truncates back — the same
// arithmetic as the simulator, so blended shades match it bit for bit.  Double,
// as in JS: in float, a blend that lands exactly on a whole number can fall a
// hair short and truncate one step darker.  Only init and a few small helpers
// blend, so the software-double cost doesn't matter.
static void neu565to888(uint16_t c, double* out) {
    out[0] = round(((c >> 11) & 31) * 255.0 / 31.0);
    out[1] = round(((c >> 5) & 63) * 255.0 / 63.0);
    out[2] = round((c & 31) * 255.0 / 31.0);
}

uint16_t neuMix(uint16_t a, uint16_t b, double t) {
    double A[3], B[3];
    neu565to888(a, A);
    neu565to888(b, B);
    return neuRgb((uint8_t)(A[0] + (B[0] - A[0]) * t),
                  (uint8_t)(A[1] + (B[1] - A[1]) * t),
                  (uint8_t)(A[2] + (B[2] - A[2]) * t));
}

const uint16_t NEU_AX[PENDANT_MAX_AXES] = {
    neuRgb(255, 112, 104), neuRgb(112, 224, 128), neuRgb(104, 168, 255),
    neuRgb(220, 200, 120), neuRgb(200, 140, 220), neuRgb(140, 210, 230),
};

// Pre-blended shadow ramps, filled by neuInit().  [0] is the faintest (outermost).
static uint16_t s_dark[NEU_DEPTH];
static uint16_t s_light[NEU_DEPTH];
static uint16_t s_rim;
static uint16_t s_insetRamp[NEU_DEPTH + 1][NEU_DEPTH];   // [depth][k], for NEU_WELL

// ---------- fonts ----------
static const lgfx::GFXfont* const kFonts[] = {
    &fonts::FreeSans9pt7b,     &fonts::FreeSans12pt7b,
    &fonts::FreeSansBold9pt7b, &fonts::FreeSansBold12pt7b,
    &fonts::FreeSansBold18pt7b, &fonts::FreeSansBold24pt7b,
};
static uint8_t s_capH[6];   // height of 'H' per font

void neuInit() {
    static bool done = false;
    if (done) return;
    done = true;
    for (int i = 0; i < NEU_DEPTH; i++) {
        s_dark[i]  = neuMix(NEU_BG, NEU_SH_DARK,  (i + 1) / (double)NEU_DEPTH);
        s_light[i] = neuMix(NEU_BG, NEU_SH_LIGHT, (i + 1) / (double)NEU_DEPTH);
    }
    s_rim = neuMix(NEU_BG, NEU_SH_LIGHT, 0.8);
    for (int d = 1; d <= NEU_DEPTH; d++)
        for (int k = 0; k < d; k++) s_insetRamp[d][k] = neuMix(NEU_SH_DARK, NEU_WELL, k / (double)d);
    for (int f = 0; f < 6; f++) s_capH[f] = kFonts[f]->glyph['H' - kFonts[f]->first].height;
}

// ---------- drawing target ----------
static LovyanGFX* s_g     = (LovyanGFX*)&display;
static int        s_dx    = 0, s_dy = 0;
static int        s_depth = 0;
static NeuRect    s_panel;

void neuPanelBegin(int x, int y, int w, int h) {
    if (s_depth++ > 0) return;
    int ox, oy;
    s_g     = beginPanelSprite(w, h, ox, oy, x, y);
    s_dx    = ox - x;
    s_dy    = oy - y;
    s_panel = { x, y, w, h };
}

void neuPanelEnd() {
    if (s_depth == 0 || --s_depth > 0) return;
    endPanelSprite(s_panel.w, s_panel.h, s_panel.x, s_panel.y);
    s_g  = (LovyanGFX*)&display;
    s_dx = s_dy = 0;
}

void neuFillRect(int x, int y, int w, int h, uint16_t c) { s_g->fillRect(x + s_dx, y + s_dy, w, h, c); }
void neuFillRoundRect(int x, int y, int w, int h, int r, uint16_t c) { s_g->fillRoundRect(x + s_dx, y + s_dy, w, h, r, c); }
void neuDrawRoundRect(int x, int y, int w, int h, int r, uint16_t c) { s_g->drawRoundRect(x + s_dx, y + s_dy, w, h, r, c); }
static void hline(int x, int y, int w, uint16_t c) { s_g->drawFastHLine(x + s_dx, y + s_dy, w, c); }
static void vline(int x, int y, int h, uint16_t c) { s_g->drawFastVLine(x + s_dx, y + s_dy, h, c); }
static void line(int x0, int y0, int x1, int y1, uint16_t c) { s_g->drawLine(x0 + s_dx, y0 + s_dy, x1 + s_dx, y1 + s_dy, c); }
static void circle(int x, int y, int r, uint16_t c) { s_g->drawCircle(x + s_dx, y + s_dy, r, c); }
static void fillCircle(int x, int y, int r, uint16_t c) { s_g->fillCircle(x + s_dx, y + s_dy, r, c); }
static void drawRect(int x, int y, int w, int h, uint16_t c) { s_g->drawRect(x + s_dx, y + s_dy, w, h, c); }
static void pixel(int x, int y, uint16_t c) { s_g->drawPixel(x + s_dx, y + s_dy, c); }
void neuFillCircle(int cx, int cy, int r, uint16_t c) { fillCircle(cx, cy, r, c); }
void neuDrawCircle(int cx, int cy, int r, uint16_t c) { circle(cx, cy, r, c); }
void neuDrawLine(int x0, int y0, int x1, int y1, uint16_t c) { line(x0, y0, x1, y1, c); }

// ---------- text ----------
// Width is the sum of xAdvance, as in the simulator.  (LovyanGFX's textWidth()
// measures the last glyph by its ink, which would shift right-aligned numbers.)
int neuTextW(const char* s, NeuFont font) {
    const lgfx::GFXfont* f = kFonts[font];
    int w = 0;
    for (; *s; s++) {
        uint8_t c = (uint8_t)*s;
        if (c >= f->first && c <= f->last) w += f->glyph[c - f->first].xAdvance;
    }
    return w;
}

int neuCapBase(int y, int h, NeuFont font) { return y + (h + s_capH[font]) / 2; }

int neuText(const char* s, int x, int y, NeuFont font, uint16_t color, NeuAlign align) {
    int w  = neuTextW(s, font);
    int x0 = align == NEU_CENTER ? x - w / 2 : align == NEU_RIGHT ? x - w : x;
    s_g->setFont(kFonts[font]);
    s_g->setTextSize(1);
    s_g->setTextDatum(textdatum_t::baseline_left);
    s_g->setTextColor(color);
    s_g->drawString(s, x0 + s_dx, y + s_dy);
    s_g->setTextDatum(textdatum_t::top_left);
    s_g->setFont(&fonts::Font0);
    return w;
}

int neuTextIn(const char* s, int x, int y, int w, int h, NeuFont font, uint16_t color, NeuAlign align) {
    int bx = align == NEU_CENTER ? x + w / 2 : align == NEU_RIGHT ? x + w : x;
    return neuText(s, bx, neuCapBase(y, h, font), font, color, align);
}

const char* neuFit(const char* s, NeuFont font, int maxW) {
    if (neuTextW(s, font) <= maxW) return s;
    static char buf[64];
    size_t len = strlen(s);
    if (len > sizeof(buf) - 3) len = sizeof(buf) - 3;
    memcpy(buf, s, len);
    for (;;) {
        strcpy(buf + len, "..");
        if (len <= 1 || neuTextW(buf, font) <= maxW) break;
        len--;
    }
    return buf;
}

static void glcd(uint16_t color) {
    s_g->setFont(&fonts::Font0);
    s_g->setTextSize(1);
    s_g->setTextDatum(textdatum_t::top_left);
    s_g->setTextColor(color);
}

int neuCaptionW(const char* s) { return (int)strlen(s) * 7 - 1; }

int neuCaption(const char* s, int x, int y, uint16_t color, NeuAlign align) {
    int w  = neuCaptionW(s);
    int cx = align == NEU_CENTER ? x - w / 2 : align == NEU_RIGHT ? x - w : x;
    glcd(color);
    char ch[2] = { 0, 0 };
    for (; *s; s++, cx += 7) {
        ch[0] = (char)toupper((uint8_t)*s);
        s_g->drawString(ch, cx + s_dx, y + s_dy);
    }
    return w;
}

int neuSmall(const char* s, int x, int y, uint16_t color, NeuAlign align) {
    int w  = (int)strlen(s) * 6;
    int x0 = align == NEU_CENTER ? x - w / 2 : align == NEU_RIGHT ? x - w : x;
    glcd(color);
    s_g->drawString(s, x0 + s_dx, y + s_dy);
    return w;
}

// ---------- surfaces ----------
void neuRaised(int x, int y, int w, int h, int r, int depth, uint16_t face) {
    for (int k = depth; k >= 1; k--) {
        int i = NEU_DEPTH - k;
        neuFillRoundRect(x + k, y + k, w, h, r, s_dark[i >= 0 ? i : 0]);
    }
    for (int k = depth; k >= 1; k--) {
        int i = NEU_DEPTH - k;
        neuFillRoundRect(x - k, y - k, w, h, r, s_light[i >= 0 ? i : 0]);
    }
    neuFillRoundRect(x, y, w, h, r, face);
}

void neuInset(int x, int y, int w, int h, int r, int depth, uint16_t well) {
    neuFillRoundRect(x, y, w, h, r, s_rim);
    for (int k = 0; k < depth; k++) {
        uint16_t c = (well == NEU_WELL && depth <= NEU_DEPTH) ? s_insetRamp[depth][k]
                                                               : neuMix(NEU_SH_DARK, well, k / (double)depth);
        neuFillRoundRect(x + k, y + k, w - k - 1, h - k - 1, r, c);
    }
    neuFillRoundRect(x + depth, y + depth, w - depth - 1, h - depth - 1, r, well);
}

void neuClear(int x, int y, int w, int h, int depth) {
    neuFillRect(x - depth, y - depth, w + 2 * depth + 1, h + 2 * depth + 1, NEU_BG);
}

void neuGroove(int y, int x0, int x1) {
    hline(x0, y, x1 - x0, NEU_SH_DARK);
    hline(x0, y + 1, x1 - x0, NEU_SH_LIGHT);
}

void neuLed(int cx, int cy, uint16_t color, int r, uint16_t bg) {
    fillCircle(cx, cy, r + 2, neuMix(bg, color, 0.22));
    fillCircle(cx, cy, r, color);
    neuFillRect(cx - 1, cy - 1, 1, 1, neuMix(color, NEU_TEXT, 0.6));
}

// ---------- state ----------
uint16_t neuStateColor(const char* status) {
    static const struct { const char* name; uint16_t color; } kStates[] = {
        { "Idle", NEU_IDLE }, { "Run", NEU_RUN },   { "Jog", NEU_JOG },     { "Home", NEU_HOME },
        { "Hold", NEU_HOLD }, { "Door", NEU_HOLD }, { "Alarm", NEU_ALARM }, { "Check", NEU_IDLE },
        { "Sleep", NEU_TEXT3 },
    };
    size_t n = strcspn(status, ":");
    for (const auto& s : kStates)
        if (strlen(s.name) == n && strncmp(status, s.name, n) == 0) return s.color;
    return NEU_TEXT2;
}

void neuStatus(char* out, size_t n) {
    static char last[24] = "N/C";
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        strlcpy(last, pendantMachine.status.c_str(), sizeof(last));
        xSemaphoreGive(stateMutex);
    }
    strlcpy(out, last, n);
}

void neuChipState(NeuChip& c) {
    char st[24];
    neuStatus(st, sizeof(st));
    if (!pendantConnected) {
        strlcpy(c.label, "OFFLINE", sizeof(c.label));
        c.color = NEU_TEXT3;
    } else if (!pendantSynced || strcmp(st, "N/C") == 0 || !st[0]) {
        strlcpy(c.label, "SYNC", sizeof(c.label));
        c.color = NEU_HOLD;
    } else {
        size_t n = strcspn(st, ":");
        if (n >= sizeof(c.label)) n = sizeof(c.label) - 1;
        for (size_t i = 0; i < n; i++) c.label[i] = (char)toupper((uint8_t)st[i]);
        c.label[n] = 0;
        c.color = neuStateColor(st);
    }
}

// ---------- header ----------
struct NeuHdrKey {
    NeuChip chip;
    bool    wireless;
    int     bars;
    int     pct;
    bool    charging;
};
static char      s_title[32];
static bool      s_hdrActive = false;
static NeuHdrKey s_hdrKey;

bool neuWireless() { return comms_active_mode() == COMMS_MODE_ESPNOW; }

static void neuHeaderKey(NeuHdrKey& k) {
    memset(&k, 0, sizeof(k));
    neuChipState(k.chip);
    k.wireless = neuWireless();
    k.bars     = pendantMachine.linkSignalBars < 0 ? 0 : pendantMachine.linkSignalBars;
    k.pct      = pendantMachine.batteryPercent;
    k.charging = pendantMachine.batteryCharging;
}

static int neuChip(const NeuHdrKey& k) {
    int w = neuTextW(k.chip.label, NEU_B9) + 26, x = 232 - w;
    int clearX = max(x - 52, 10 + neuTextW(s_title, NEU_B9) + 4);   // chip + icon zone, never the title
    neuFillRect(clearX, 1, 240 - clearX, 28, NEU_BG);
    neuInset(x, 5, w, 20, 10, 2);
    neuLed(x + 11, 15, k.chip.color, 3);
    neuText(k.chip.label, x + 19, neuCapBase(5, 20, NEU_B9), NEU_B9, k.chip.color);
    return x;
}

static int neuLinkIcon(int xr, int bars) {
    int x = xr - 14;
    for (int i = 0; i < 4; i++) {
        int h = 3 + i * 2;
        neuFillRect(x + i * 4, 20 - h, 2, h, i < bars ? NEU_TEXT2 : NEU_SH_LIGHT);
    }
    return x;
}

static int neuBattery(int xr, int pct, bool charging) {
    int x = xr - 20, y = 10;
    if (pct < 0) return xr;
    neuDrawRoundRect(x, y, 17, 10, 2, NEU_TEXT3);
    neuFillRect(x + 17, y + 3, 2, 4, NEU_TEXT3);
    uint16_t col = pct > 50 ? NEU_TEXT2 : pct > 20 ? NEU_HOLD : NEU_ALARM;
    neuFillRect(x + 2, y + 2, (13 * pct + 50) / 100, 6, col);
    if (charging) {
        neuFillRect(x + 7, y + 1, 2, 4, NEU_HOLD);
        neuFillRect(x + 6, y + 4, 4, 2, NEU_HOLD);
        neuFillRect(x + 7, y + 5, 2, 4, NEU_HOLD);
    }
    return x;
}

static void neuHeaderRight(const NeuHdrKey& k) {
    int chipX = neuChip(k);
    if (!k.wireless) return;                                      // wired: no link / battery icons
    int titleEnd = 10 + neuTextW(s_title, NEU_B9) + 8;
    int x = chipX;
    if (x - 26 >= titleEnd) x = neuBattery(x - 6, k.pct, k.charging);   // icons yield to a long title
    if (x - 20 >= titleEnd) neuLinkIcon(x - 6, k.bars);
}

void neuHeader(const char* title) {
    neuInit();
    display.fillScreen(NEU_BG);
    strlcpy(s_title, neuFit(title, NEU_B9, 150), sizeof(s_title));   // as drawn, so widths measure what is on screen
    s_hdrActive = true;
    neuHeaderKey(s_hdrKey);
    neuHeaderRight(s_hdrKey);
    neuText(s_title, 10, neuCapBase(5, 20, NEU_B9), NEU_B9, NEU_TEXT);
    neuGroove(NEU_HDR_H);
}

void neuHeaderUpdate() {
    if (!s_hdrActive) return;
    NeuHdrKey k;
    neuHeaderKey(k);
    if (memcmp(&k, &s_hdrKey, sizeof(k)) == 0) return;
    s_hdrKey = k;
    neuHeaderRight(k);
}

bool neuHeaderActive() { return s_hdrActive; }
void neuHeaderDetach() { s_hdrActive = false; }

// ---------- icons ----------
static void L2(int x0, int y0, int x1, int y1, uint16_t c) {
    line(x0, y0, x1, y1, c);
    line(x0 + 1, y0, x1 + 1, y1, c);
    line(x0, y0 + 1, x1, y1 + 1, c);
}
static int jsRound(double v) { return (int)floor(v + 0.5); }   // Math.round
static void arc(int cx, int cy, int r0, int r1, int a0, int a1, uint16_t c) {
    for (int a = a0; a <= a1; a += 3) {
        double t = a * M_PI / 180.0;
        for (int r = r0; r <= r1; r++) pixel(jsRound(cx + cos(t) * r), jsRound(cy + sin(t) * r), c);
    }
}
static void jogTri(int cx, int cy, int dx, int dy, uint16_t c) {
    for (int i = 0; i < 4; i++) {
        if (dx) vline(cx + dx * (8 - i), cy - i, 2 * i + 1, c);
        else    hline(cx - i, cy + dy * (8 - i), 2 * i + 1, c);
    }
}

void neuIcon(NeuIcon icon, int cx, int cy, uint16_t c) {
    switch (icon) {
    case ICON_NONE: break;
    case ICON_HOME:
        L2(cx - 8, cy, cx, cy - 8, c); L2(cx, cy - 8, cx + 7, cy - 1, c);
        neuFillRect(cx - 6, cy, 2, 8, c); neuFillRect(cx + 4, cy, 2, 8, c);
        neuFillRect(cx - 6, cy + 7, 12, 2, c); neuFillRect(cx - 1, cy + 3, 3, 5, c);
        break;
    case ICON_BACK: L2(cx + 3, cy - 7, cx - 4, cy, c); L2(cx - 4, cy, cx + 3, cy + 7, c); break;
    case ICON_FWD:  L2(cx - 4, cy - 7, cx + 3, cy, c); L2(cx + 3, cy, cx - 4, cy + 7, c); break;
    case ICON_CHEVRON:
        line(cx - 2, cy - 4, cx + 2, cy, c); line(cx + 2, cy, cx - 2, cy + 4, c);
        line(cx - 1, cy - 4, cx + 3, cy, c); line(cx + 3, cy, cx - 1, cy + 4, c);
        break;
    case ICON_PLAY:    for (int i = 0; i < 9; i++) vline(cx - 4 + i, cy - 8 + i, 17 - 2 * i, c); break;
    case ICON_PLAY_SM: for (int i = 0; i < 6; i++) vline(cx - 3 + i, cy - 5 + i, 11 - 2 * i, c); break;
    case ICON_STOP:    neuFillRoundRect(cx - 6, cy - 6, 13, 13, 2, c); break;
    case ICON_JOG:
        jogTri(cx, cy, 1, 0, c); jogTri(cx, cy, -1, 0, c); jogTri(cx, cy, 0, 1, c); jogTri(cx, cy, 0, -1, c);
        neuFillRect(cx - 5, cy - 1, 11, 2, c); neuFillRect(cx - 1, cy - 5, 2, 11, c);
        break;
    case ICON_ORIGIN:
        circle(cx, cy, 7, c); circle(cx, cy, 6, c);
        neuFillRect(cx - 9, cy - 1, 19, 2, c); neuFillRect(cx - 1, cy - 9, 2, 19, c);
        fillCircle(cx, cy, 2, c);
        break;
    case ICON_GAUGE:
        arc(cx, cy + 3, 8, 9, 180, 360, c); L2(cx, cy + 3, cx + 5, cy - 3, c); fillCircle(cx, cy + 3, 2, c);
        break;
    case ICON_SPINDLE:
        neuFillRect(cx - 5, cy - 8, 11, 5, c); neuFillRect(cx - 3, cy - 3, 7, 5, c);
        L2(cx - 1, cy + 2, cx - 1, cy + 9, c); L2(cx - 6, cy + 5, cx - 1, cy + 9, c); L2(cx + 5, cy + 5, cx, cy + 9, c);
        break;
    case ICON_MACRO:
        L2(cx + 2, cy - 9, cx - 4, cy + 1, c); neuFillRect(cx - 4, cy, 9, 2, c); L2(cx + 4, cy + 1, cx - 2, cy + 9, c);
        break;
    case ICON_CARD:
        neuDrawRoundRect(cx - 6, cy - 8, 13, 17, 2, c); neuDrawRoundRect(cx - 5, cy - 7, 11, 15, 2, c);
        for (int i = 0; i < 3; i++) neuFillRect(cx - 3 + i * 3, cy - 6, 1, 4, c);
        break;
    case ICON_PROBE:
        neuFillRect(cx - 4, cy - 9, 9, 5, c); neuFillRect(cx - 1, cy - 4, 3, 9, c);
        fillCircle(cx, cy + 6, 2, c); neuFillRect(cx - 8, cy + 9, 17, 1, c);
        break;
    case ICON_STATUS:
        neuFillRect(cx - 7, cy + 1, 3, 7, c); neuFillRect(cx - 2, cy - 3, 3, 11, c); neuFillRect(cx + 3, cy - 8, 3, 16, c);
        break;
    case ICON_FOLDER:
        neuFillRoundRect(cx - 8, cy - 5, 7, 4, 1, c); neuFillRoundRect(cx - 8, cy - 3, 17, 11, 2, c);
        break;
    case ICON_FILE:
        drawRect(cx - 6, cy - 8, 12, 17, c); drawRect(cx - 5, cy - 7, 10, 15, c);
        neuFillRect(cx - 3, cy - 3, 6, 1, c); neuFillRect(cx - 3, cy, 6, 1, c); neuFillRect(cx - 3, cy + 3, 4, 1, c);
        break;
    case ICON_UP: L2(cx - 7, cy + 3, cx, cy - 4, c); L2(cx, cy - 4, cx + 7, cy + 3, c); break;
    case ICON_REFRESH:
        arc(cx, cy, 6, 7, 0, 290, c);
        for (int i = 0; i < 4; i++) hline(cx + 1, cy - 9 + i, 7 - 2 * i, c);
        break;
    case ICON_GEAR:
        for (int a = 0; a < 8; a++) {
            double t = a * M_PI / 4;
            neuFillRect(jsRound(cx + cos(t) * 7) - 1, jsRound(cy + sin(t) * 7) - 1, 3, 3, c);
        }
        circle(cx, cy, 4, c); circle(cx, cy, 5, c); circle(cx, cy, 6, c);
        break;
    case ICON_LINK:
        neuDrawRoundRect(cx - 9, cy - 3, 10, 7, 3, c); neuDrawRoundRect(cx, cy - 3, 10, 7, 3, c);
        break;
    case ICON_PLUS:  neuFillRect(cx - 6, cy - 1, 13, 3, c); neuFillRect(cx - 1, cy - 6, 3, 13, c); break;
    case ICON_MINUS: neuFillRect(cx - 6, cy - 1, 13, 3, c); break;
    case ICON_TRASH:
        neuFillRect(cx - 6, cy - 6, 13, 2, c); neuFillRect(cx - 2, cy - 8, 5, 2, c); drawRect(cx - 5, cy - 3, 11, 12, c);
        neuFillRect(cx - 2, cy - 1, 1, 8, c); neuFillRect(cx + 2, cy - 1, 1, 8, c);
        break;
    case ICON_UNLOCK:
        neuFillRoundRect(cx - 6, cy - 1, 13, 10, 2, c); arc(cx - 3, cy - 3, 4, 5, 180, 360, c);
        neuFillRect(cx - 8, cy - 4, 2, 4, c);
        break;
    }
}

// ---------- nav bar ----------
const NeuNavItem NAV_HOME = { NAV_ID_HOME, "", ICON_HOME, 40 };
const NeuNavItem NAV_BACK = { NAV_ID_BACK, "", ICON_BACK, 40 };

void neuNavLayout(const NeuNavItem* items, int n, NeuSpan* out) {
    const int gap = 8, x0 = 10, W = 220;
    int fixed = 0, flex = 0;
    for (int i = 0; i < n; i++) {
        fixed += items[i].w;
        if (!items[i].w) flex++;
    }
    int fw = flex ? (W - fixed - gap * (n - 1)) / flex : 0;
    int x  = x0;
    for (int i = 0; i < n; i++) {
        int w = items[i].w ? items[i].w : fw;
        if (i == n - 1) w = x0 + W - x;
        out[i] = { x, w };
        x += w + gap;
    }
}

void neuNav(const NeuNavItem* items, int n) {
    NeuSpan lay[6];
    neuGroove(278);
    neuNavLayout(items, n, lay);
    for (int i = 0; i < n; i++) neuNavButton(lay[i].x, lay[i].w, items[i]);
}

void neuNavReplace(const NeuNavItem* items, int n) {
    neuFillRect(0, NEU_NAV_Y - NEU_DEPTH - 1, 240, 320 - (NEU_NAV_Y - NEU_DEPTH - 1), NEU_BG);
    neuNav(items, n);
}

void neuNavButton(int x, int w, const NeuNavItem& it) {
    NeuBtnStyle o;
    o.color = it.color ? it.color : (it.primary ? NEU_ACCENT : NEU_TEXT2);
    o.icon  = it.icon;
    o.on    = it.on;
    o.face  = it.face ? it.face : NEU_FACE;
    o.bar   = false;
    o.dim   = it.dim;
    neuClear(x, NEU_NAV_Y, w, NEU_NAV_H);
    neuButton(x, NEU_NAV_Y, w, NEU_NAV_H, it.label ? it.label : "", o);
}

int neuNavHit(const NeuNavItem* items, int n, int x, int y) {
    if (y < NEU_NAV_Y - 4) return NAV_ID_NONE;
    NeuSpan lay[6];
    neuNavLayout(items, n, lay);
    for (int i = 0; i < n; i++)
        if (x >= lay[i].x - 4 && x <= lay[i].x + lay[i].w + 4) return items[i].id;
    return NAV_ID_NONE;
}

// ---------- controls ----------
void neuButton(int x, int y, int w, int h, const char* label, const NeuBtnStyle& o) {
    if (o.on) neuInset(x, y, w, h, o.r, 3);
    else      neuRaised(x, y, w, h, o.r, o.depth, o.face);
    uint16_t col = o.color ? o.color : (o.on ? NEU_ACCENT : NEU_TEXT);
    if (o.dim) col = NEU_TEXT3;
    if (o.icon && label[0]) {
        int lw = neuTextW(label, o.font), total = 18 + 6 + lw, sx = x + (w - total) / 2;
        neuIcon(o.icon, sx + 9, y + h / 2, col);
        neuText(label, sx + 24, neuCapBase(y, h, o.font), o.font, col);
    } else if (o.icon) {
        neuIcon(o.icon, x + w / 2, y + h / 2, col);
    } else if (o.sub) {
        neuText(label, x + w / 2, y + h / 2 + 1, o.font, col, NEU_CENTER);
        neuCaption(o.sub, x + w / 2, y + h / 2 + 6, o.on ? neuMix(NEU_ACCENT, NEU_WELL, 0.35) : NEU_TEXT3, NEU_CENTER);
    } else {
        neuTextIn(label, x, y, w, h, o.font, col);
    }
    if (o.on && o.bar) neuFillRoundRect(x + 8, y + h - 6, w - 17, 2, 1, NEU_ACCENT);
}

void neuSegLayout(int x, int w, int n, NeuSpan* out) {
    const int pad = 5, gap = 2, sw = (w - 2 * pad - (n - 1) * gap) / n;
    for (int i = 0; i < n; i++) {
        int sx = x + pad + i * (sw + gap);
        out[i] = { sx, i == n - 1 ? x + w - pad - sx : sw };
    }
}

void neuSegmented(int x, int y, int w, int h, const char* const* items, int n, int sel,
                  int r, NeuFont font, uint16_t selColor, const uint16_t* colors) {
    const int pad = 5;
    NeuSpan segs[8];
    neuInset(x, y, w, h, r, 3);
    neuSegLayout(x, w, n, segs);
    for (int i = 0; i < n; i++) {
        if (i == sel) neuRaised(segs[i].x, y + pad, segs[i].w, h - 2 * pad, r - 3, 2);
        neuTextIn(items[i], segs[i].x, y + pad, segs[i].w, h - 2 * pad, font,
                  i == sel ? selColor : (colors ? colors[i] : NEU_TEXT2));
    }
}

int neuSegHit(int x, int y, int w, int h, int n, int tx, int ty) {
    if (ty < y || ty > y + h || tx < x || tx > x + w) return -1;
    return constrain((tx - x) * n / w, 0, n - 1);
}

void neuField(int x, int y, int w, int h, const char* cap, const char* value, const char* unit,
              const NeuFieldStyle& o) {
    neuInset(x, y, w, h, 8, 3);
    neuFieldValue(x, y, w, h, cap, value, unit, o);
}

void neuFieldValue(int x, int y, int w, int h, const char* cap, const char* value, const char* unit,
                   const NeuFieldStyle& o) {
    neuDrawRoundRect(x - 3, y - 3, w + 6, h + 6, 11, o.focus ? NEU_ACCENT : NEU_BG);
    neuDrawRoundRect(x - 2, y - 2, w + 4, h + 4, 10, o.focus ? NEU_ACCENT : NEU_BG);
    neuFillRect(x + 4, y + 4, w - 7, h - 7, NEU_WELL);
    const int pad = h < 38 ? 5 : 7;   // short fields pull caption and value toward the edges
    neuCaption(cap, x + 9, y + pad, o.focus ? NEU_ACCENT : NEU_TEXT3);
    const int      base = y + h - pad;
    const uint16_t vc   = o.color ? o.color : NEU_TEXT;
    const bool     hasU = unit && unit[0];
    if (o.alignR) {
        int uw = hasU ? neuCaptionW(unit) + 4 : 0;
        if (hasU) neuCaption(unit, x + w - 9, base - 7, NEU_TEXT3, NEU_RIGHT);
        neuText(value, x + w - 9 - uw, base, o.font, vc, NEU_RIGHT);
    } else {
        int vw = neuText(value, x + 9, base, o.font, vc);
        if (hasU) neuCaption(unit, x + 9 + vw + 4, base - 7, NEU_TEXT3);
    }
}

void neuProgressBar(int x, int y, int w, float frac, uint16_t color) {
    neuFillRoundRect(x, y, w, 4, 2, NEU_SH_DARK);
    int fw = jsRound(w * constrain(frac, 0.0f, 1.0f));
    if (fw > 0) neuFillRoundRect(x, y, max(4, fw), 4, 2, color);
}

void neuHint(const char* msg, int y, uint16_t color) {
    int w = (int)strlen(msg) * 6 + 16, x = 120 - w / 2;
    neuFillRect(10, y - 2, 220, 12, NEU_BG);
    for (int i = 0; i < 6; i++) hline(x + 5 - i, y + 1 + i, 1 + 2 * i, color);
    neuFillRect(x + 5, y + 3, 1, 2, NEU_BG);
    neuSmall(msg, x + 16, y, color);
}

NeuDialogRects neuDialog(const char* title, const char* const* lines, int nLines,
                         const char* cancelLabel, const char* okLabel, uint16_t okColor, uint16_t titleColor) {
    // dim the screen behind with a 2px checker — cheap and AA-free
    s_g->startWrite();
    for (int yy = 32; yy < 320; yy += 2)
        for (int xx = (yy >> 1) & 1; xx < 240; xx += 2) pixel(xx, yy, NEU_SH_DARK);
    s_g->endWrite();
    neuRaised(18, 92, 204, 138, 14, 3, NEU_BG);
    neuText(title, 120, 120, NEU_B12, titleColor, NEU_CENTER);
    for (int i = 0; i < nLines; i++) neuSmall(lines[i], 120, 134 + i * 12, NEU_TEXT2, NEU_CENTER);
    NeuDialogRects out = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
    if (okLabel && cancelLabel) {
        out.cancel = { 32, 180, 80, 36 };
        out.ok     = { 124, 180, 84, 36 };
        NeuBtnStyle c;
        c.color = NEU_TEXT2;
        neuButton(out.cancel.x, out.cancel.y, out.cancel.w, out.cancel.h, cancelLabel, c);
    } else if (okLabel) {
        out.ok = { 72, 180, 96, 36 };
    }
    if (out.ok.w) {
        NeuBtnStyle k;
        k.color = okColor;
        neuButton(out.ok.x, out.ok.y, out.ok.w, out.ok.h, okLabel, k);
    }
    return out;
}

bool neuDialogHit(const NeuRect& r, int x, int y) {
    return r.w > 0 && x >= r.x - 4 && x <= r.x + r.w + 4 && y >= r.y - 4 && y <= r.y + r.h + 4;
}

// ---------- DRO ----------
bool neuDroSnapshot(NeuDroSnap& s) {
    memset(&s, 0, sizeof(s));
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(5)) != pdTRUE) return false;
    s.numAxes  = constrain(pendantMachine.numAxes, 0, PENDANT_MAX_AXES);
    s.inInches = pendantMachine.inInches;
    for (int i = 0; i < s.numAxes; i++) {
        s.wpos[i] = pendantMachine.wpos[i];
        s.mpos[i] = pendantMachine.mpos[i];
    }
    strlcpy(s.wcs, pendantMachine.workCoordSystem.c_str(), sizeof(s.wcs));
    xSemaphoreGive(stateMutex);
    return true;
}

bool neuDroSame(const NeuDroSnap& a, const NeuDroSnap& b) { return memcmp(&a, &b, sizeof(a)) == 0; }

void neuFmtPos(char* out, size_t n, float v, bool inInches) { snprintf(out, n, "%.*f", inInches ? 4 : 3, v); }

void neuDroLabels(int x, int y, const NeuDroSnap& s) {
    char cap[16];
    snprintf(cap, sizeof(cap), "Work %s", s.wcs);
    neuFillRect(x + 4, y + 4, 212, 12, NEU_WELL);
    if (s.numAxes > 3) {   // 4-6 axes: two columns of work positions, no machine column
        neuCaption(cap, x + 12, y + 7, NEU_TEXT3);
        return;
    }
    neuCaption(cap, x + 132, y + 7, NEU_TEXT3, NEU_RIGHT);
    neuCaption("Mach", x + 208, y + 7, NEU_TEXT3, NEU_RIGHT);
}

void neuDro(int x, int y, const NeuDroSnap& s) {
    neuInset(x, y, 220, NEU_DRO_H, 12, 3);
    neuDroLabels(x, y, s);
    neuDroValues(x, y, s);
}

void neuDroValues(int x, int y, const NeuDroSnap& s) {
    char buf[16];
    neuPanelBegin(x + 4, y + 16, 212, 49);
    neuFillRect(x + 4, y + 16, 212, 49, NEU_WELL);
    if (s.numAxes > 3) {
        for (int i = 0; i < s.numAxes; i++) {
            int cx = x + (i < 3 ? 12 : 116), b = y + 31 + (i % 3) * 16;
            neuText(AXIS_NAMES[i], cx, b, NEU_B9, NEU_AX[i]);
            neuFmtPos(buf, sizeof(buf), s.wpos[i], s.inInches);
            neuText(buf, cx + 92, b, NEU_B9, NEU_TEXT, NEU_RIGHT);
        }
    } else {
        for (int i = 0; i < s.numAxes; i++) {
            int b = y + 31 + i * 16;
            neuText(AXIS_NAMES[i], x + 12, b, NEU_B9, NEU_AX[i]);
            neuFmtPos(buf, sizeof(buf), s.wpos[i], s.inInches);
            neuText(buf, x + 132, b, NEU_B9, NEU_TEXT, NEU_RIGHT);
            snprintf(buf, sizeof(buf), "%.2f", s.mpos[i]);
            neuCaption(buf, x + 208, b - 7, NEU_TEXT3, NEU_RIGHT);
        }
    }
    neuPanelEnd();
}

// ---------- list screens ----------
int neuListRowHit(int x, int y) {
    if (x < 10 || x > 230 || y < NEU_LIST_WELL_Y || y >= NEU_LIST_WELL_Y + NEU_LIST_WELL_H) return -1;
    int i = (y - NEU_LIST_ROW_Y0 + 2) / NEU_LIST_PITCH;
    return i >= 0 && i < LIST_ROWS ? i : -1;
}

bool neuListPageHit(int x, int y) {
    return y >= NEU_LIST_CRUMB_Y - 4 && y <= NEU_LIST_CRUMB_Y + 30 && x >= 170;
}

void neuListFrame() {
    neuInset(10, NEU_LIST_CRUMB_Y, 220, 26, 8, 2);
    neuInset(10, NEU_LIST_WELL_Y, 220, NEU_LIST_WELL_H, 12, 3);
}

// Fit a path into maxChars by dropping leading segments — the folder you are IN
// matters more than how you got there, so truncation eats the left.
static void neuCrumbText(const char* path, char* out, size_t n, int maxChars) {
    if ((int)strlen(path) <= maxChars) { strlcpy(out, path, n); return; }
    const char* segs[16];
    int         lens[16], count = 0;
    for (const char* p = path; *p && count < 16;) {
        while (*p == '/') p++;
        if (!*p) break;
        const char* e = strchr(p, '/');
        int len = e ? (int)(e - p) : (int)strlen(p);
        segs[count] = p;
        lens[count++] = len;
        p += len;
    }
    if (!count) { strlcpy(out, path, n); return; }
    int first = count - 1, total = lens[first];
    while (first > 0 && lens[first - 1] + 1 + total + 2 <= maxChars) total += lens[--first] + 1;
    snprintf(out, n, ".../%s", segs[first]);
}

void neuListCrumb(NeuIcon icon, const char* text, int page, int pages, bool truncated) {
    char pl[12] = "", crumb[48];
    neuFillRect(13, NEU_LIST_CRUMB_Y + 3, 215, 21, NEU_WELL);
    neuIcon(icon, 24, NEU_LIST_CRUMB_Y + 13, NEU_TEXT3);
    if (pages > 0) snprintf(pl, sizeof(pl), "%d/%d%s", page, pages, truncated ? "+" : "");
    neuCrumbText(text, crumb, sizeof(crumb), pl[0] ? 24 : 30);
    neuSmall(crumb, 38, NEU_LIST_CRUMB_Y + 10, NEU_TEXT2);
    if (pl[0]) neuSmall(pl, 220, NEU_LIST_CRUMB_Y + 10, pages > 1 ? NEU_ACCENT : NEU_TEXT3, NEU_RIGHT);
}

void neuListClear() { neuFillRect(14, NEU_LIST_WELL_Y + 4, 213, NEU_LIST_WELL_H - 7, NEU_WELL); }

void neuListRow(int i, const char* label, NeuRowKind kind, bool sel) {
    const int y = neuListRowY(i);
    if (sel)        neuRaised(18, y, 204, NEU_LIST_ROW_H - 4, 9, 2);
    else if (i > 0) hline(26, y - 3, 188, NEU_SH_DARK);
    const int      cy      = y + (NEU_LIST_ROW_H - 4) / 2;
    const bool     dirish  = kind == NEU_ROW_DIR || kind == NEU_ROW_UP;
    const uint16_t iconCol = sel ? NEU_ACCENT : dirish ? NEU_JOG : NEU_TEXT3;
    switch (kind) {
    case NEU_ROW_DIR:   neuIcon(ICON_FOLDER, 32, cy, iconCol); break;
    case NEU_ROW_UP:    neuIcon(ICON_UP, 32, cy + 2, iconCol); break;
    case NEU_ROW_MACRO: neuIcon(ICON_PLAY_SM, 32, cy, iconCol); break;
    case NEU_ROW_FILE:  neuIcon(ICON_FILE, 32, cy, iconCol); break;
    }
    const NeuFont font = sel ? NEU_B9 : NEU_S9;
    const int     maxW = kind == NEU_ROW_DIR ? 150 : 166;
    neuText(neuFit(label, font, maxW), 48, neuCapBase(y, NEU_LIST_ROW_H - 4, font), font, sel ? NEU_TEXT : NEU_TEXT2);
    if (kind == NEU_ROW_DIR) neuIcon(ICON_CHEVRON, 212, cy, NEU_TEXT3);
}

void neuListPaint(void (*drawBand)(int band)) {
    for (int b = 0; b < LIST_ROWS; b++) {
        // Band 0 starts where neuListClear() does, clear of the inset's ramp.
        const int y = b == 0 ? NEU_LIST_WELL_Y + 4 : neuListRowY(b) - 3;
        const int h = neuListRowY(b + 1) - 3 - y;
        neuPanelBegin(14, y, 213, h);
        neuFillRect(14, y, 213, h, NEU_WELL);
        drawBand(b);
        neuPanelEnd();
    }
    // The strip under the last band never holds content; keep it flat.
    const int tail = neuListRowY(LIST_ROWS) - 3;
    neuFillRect(14, tail, 213, NEU_LIST_WELL_Y + NEU_LIST_WELL_H - 3 - tail, NEU_WELL);
}

void neuListMessage(const char* line1, const char* line2, uint16_t color) {
    neuText(line1, 120, 160, NEU_B9, color, NEU_CENTER);
    if (line2) neuSmall(line2, 120, 172, NEU_TEXT3, NEU_CENTER);
}
