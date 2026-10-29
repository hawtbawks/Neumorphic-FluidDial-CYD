/*
 * neu_ui.h — the neumorphic UI kit shared by every screen.
 * Port of simulator/js/neu_ui.js, which is the spec: keep the two in step.
 *
 * Rules of the system
 *   • One surface (NEU_BG), light from the top-left.
 *   • neuRaised() = something you can press.   neuInset() = a readout, a field,
 *     or the selected option.  Pressing a toggle moves it from raised to inset.
 *   • NEU_ACCENT (orange) = focus only: selected option, dial target, primary action.
 *   • Machine-state colour lives in the header chip on every screen.
 *
 * Text: neuText(s, x, baselineY, font, color, align) — y is the BASELINE for the
 * FreeSans fonts (drawn with the baseline_left datum).  The GLCD font is only
 * used for captions (neuCaption) and small text (neuSmall), where y is the TOP.
 *
 * Layout grid (240x320):
 *   header   y 0..29, groove at y 30
 *   content  y 36..276, 10 px side margins, 8 px gaps (shadows need 3 px clear)
 *   nav bar  groove at y 278, buttons y 285 h 30; Home always bottom-left
 *
 * Flicker-free repaints: wrap an update*() repaint in neuPanelBegin(x,y,w,h) /
 * neuPanelEnd().  Everything the kit draws in between lands in the shared 16-bit
 * panel scratch (screen coordinates are translated for you) and is pushed in one
 * blit.  Draw shadows once in draw*() straight to the display; repaint only the
 * flat interior of an inset from update*().
 */
#pragma once

#include "pendant_shared.h"

// ---------- colour ----------
constexpr uint16_t neuRgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
}
// Linear blend of two RGB565 colours: t=0 -> a, t=1 -> b.
uint16_t neuMix(uint16_t a, uint16_t b, double t);

// ---------- tokens ----------
constexpr uint16_t NEU_BG        = neuRgb(44, 48, 56);
constexpr uint16_t NEU_FACE      = neuRgb(47, 51, 60);
constexpr uint16_t NEU_WELL      = neuRgb(37, 40, 47);
constexpr uint16_t NEU_SH_DARK   = neuRgb(24, 26, 31);
constexpr uint16_t NEU_SH_LIGHT  = neuRgb(70, 76, 88);
constexpr uint16_t NEU_TEXT      = neuRgb(228, 232, 238);
constexpr uint16_t NEU_TEXT2     = neuRgb(150, 158, 172);
constexpr uint16_t NEU_TEXT3     = neuRgb(100, 108, 122);
constexpr uint16_t NEU_ACCENT    = neuRgb(255, 150, 48);
constexpr uint16_t NEU_IDLE      = neuRgb(96, 208, 200);
constexpr uint16_t NEU_RUN       = neuRgb(92, 220, 128);
constexpr uint16_t NEU_HOLD      = neuRgb(255, 190, 64);
constexpr uint16_t NEU_ALARM     = neuRgb(255, 84, 84);
constexpr uint16_t NEU_JOG       = neuRgb(110, 168, 255);
constexpr uint16_t NEU_HOME      = neuRgb(176, 140, 255);
constexpr uint16_t NEU_GO_FACE   = neuRgb(36, 132, 72);
constexpr uint16_t NEU_STOP_FACE = neuRgb(200, 44, 44);
extern const uint16_t NEU_AX[PENDANT_MAX_AXES];
constexpr int NEU_DEPTH = 3;

// Precomputes the shadow ramps and font metrics.  Call once at boot.
void neuInit();

// ---------- fonts ----------
enum NeuFont : uint8_t { NEU_S9, NEU_S12, NEU_B9, NEU_B12, NEU_B18, NEU_B24 };
enum NeuAlign : uint8_t { NEU_LEFT, NEU_CENTER, NEU_RIGHT };

int         neuTextW(const char* s, NeuFont font);
// Baseline that visually centres capitals in a box of height h starting at y.
int         neuCapBase(int y, int h, NeuFont font);
int         neuText(const char* s, int x, int y, NeuFont font, uint16_t color, NeuAlign align = NEU_LEFT);
int         neuTextIn(const char* s, int x, int y, int w, int h, NeuFont font, uint16_t color, NeuAlign align = NEU_CENTER);
// Shorten s with a trailing ".." until it fits maxW.  Returns s itself when it
// fits, otherwise a static buffer that the next call overwrites.
const char* neuFit(const char* s, NeuFont font, int maxW);
// Caption: GLCD 5x7, upper-case, 1 px extra tracking (7 px pitch).  y = top.
int         neuCaptionW(const char* s);
int         neuCaption(const char* s, int x, int y, uint16_t color = NEU_TEXT3, NeuAlign align = NEU_LEFT);
// Plain GLCD text (mixed case), y = top.
int         neuSmall(const char* s, int x, int y, uint16_t color, NeuAlign align = NEU_LEFT);

// ---------- drawing target ----------
// Calls nest: only the outermost pair allocates and pushes, so a helper that
// wraps itself in a panel can also be called from inside a bigger one.
void neuPanelBegin(int x, int y, int w, int h);
void neuPanelEnd();

// ---------- surfaces ----------
// Convex.  2*depth+1 fillRoundRect.  Needs `depth` px clear around it.
void neuRaised(int x, int y, int w, int h, int r = 10, int depth = NEU_DEPTH, uint16_t face = NEU_FACE);
// Concave.  depth+2 fillRoundRect, entirely inside the box.  The flat interior is
// (x+depth, y+depth) .. (x+w-2, y+h-2): clear that to repaint contents.
void neuInset(int x, int y, int w, int h, int r = 10, int depth = NEU_DEPTH, uint16_t well = NEU_WELL);
// Clear the space a raised/inset element and its shadow occupy.
void neuClear(int x, int y, int w, int h, int depth = NEU_DEPTH);
// Carved line: dark over light.
void neuGroove(int y, int x0 = 8, int x1 = 232);
void neuLed(int cx, int cy, uint16_t color, int r = 3, uint16_t bg = NEU_WELL);
void neuFillRect(int x, int y, int w, int h, uint16_t color);
void neuFillRoundRect(int x, int y, int w, int h, int r, uint16_t color);
void neuDrawRoundRect(int x, int y, int w, int h, int r, uint16_t color);

// ---------- state ----------
uint16_t neuStateColor(const char* status);
// Copies pendantMachine.status under stateMutex.  When the lock is busy it
// returns the last value it read, so a caller always gets something sane.
void     neuStatus(char* out, size_t n);

// ---------- header ----------
constexpr int NEU_HDR_H = 30;
// Clears the screen and draws the header.  Every screen's draw*() starts here.
void neuHeader(const char* title);
// Called from the 100 ms tick: repaints the right side only on change.
void neuHeaderUpdate();
// True while the screen on display was drawn with neuHeader().  Legacy screens
// draw their own title bar; drawTitle() and drawCurrentPendantScreen() detach.
bool neuHeaderActive();
void neuHeaderDetach();
bool neuWireless();

// ---------- icons: 18x18 box centred on (cx,cy), 2 px strokes ----------
enum NeuIcon : uint8_t {
    ICON_NONE, ICON_HOME, ICON_BACK, ICON_FWD, ICON_CHEVRON, ICON_PLAY, ICON_PLAY_SM,
    ICON_STOP, ICON_JOG, ICON_ORIGIN, ICON_GAUGE, ICON_SPINDLE, ICON_MACRO, ICON_CARD,
    ICON_PROBE, ICON_STATUS, ICON_FOLDER, ICON_FILE, ICON_UP, ICON_REFRESH, ICON_GEAR,
    ICON_LINK, ICON_PLUS, ICON_MINUS, ICON_TRASH, ICON_UNLOCK
};
void neuIcon(NeuIcon icon, int cx, int cy, uint16_t c);

// ---------- nav bar ----------
constexpr int NEU_NAV_Y = 285;
constexpr int NEU_NAV_H = 30;
// id 0 means "no item"; screens number their own items from NAV_ID_FIRST.
// color / face 0 = default; w 0 = share the remaining width.
enum { NAV_ID_NONE = 0, NAV_ID_HOME, NAV_ID_BACK, NAV_ID_FIRST };
struct NeuNavItem {
    int         id;
    const char* label;
    NeuIcon     icon;
    int         w;
    bool        on;
    bool        primary;
    bool        dim;
    uint16_t    color;
    uint16_t    face;
};
extern const NeuNavItem NAV_HOME;
extern const NeuNavItem NAV_BACK;
struct NeuSpan { int x, w; };
// Lays out nav items left to right (fixed widths first, the rest share the space).
void neuNavLayout(const NeuNavItem* items, int n, NeuSpan* out);
void neuNav(const NeuNavItem* items, int n);
void neuNavButton(int x, int w, const NeuNavItem& it);
// Returns the id of the nav item under (x,y), or NAV_ID_NONE.
int  neuNavHit(const NeuNavItem* items, int n, int x, int y);

// ---------- controls ----------
struct NeuBtnStyle {
    bool        on    = false;
    uint16_t    color = 0;          // 0 = ACCENT when on, TEXT otherwise
    NeuFont     font  = NEU_B9;
    NeuIcon     icon  = ICON_NONE;
    const char* sub   = nullptr;    // caption under the label
    uint16_t    face  = NEU_FACE;
    int         r     = 10;
    int         depth = NEU_DEPTH;
    bool        bar   = true;       // accent bar along the bottom when on
    bool        dim   = false;
};
void neuButton(int x, int y, int w, int h, const char* label, const NeuBtnStyle& o = NeuBtnStyle());

// Segmented control: inset track, the selected segment is a raised pill.
// sel may be -1 (none selected).  Use neuSegHit() for touch.
void neuSegLayout(int x, int w, int n, NeuSpan* out);
void neuSegmented(int x, int y, int w, int h, const char* const* items, int n, int sel,
                  int r = 10, NeuFont font = NEU_B9, uint16_t selColor = NEU_ACCENT,
                  const uint16_t* colors = nullptr);
int  neuSegHit(int x, int y, int w, int h, int n, int tx, int ty);

// Value field: inset well with caption + value + unit; focus = dial target (accent ring).
struct NeuFieldStyle {
    bool     focus  = false;
    NeuFont  font   = NEU_B12;
    bool     alignR = false;
    uint16_t color  = 0;            // 0 = TEXT
};
void neuField(int x, int y, int w, int h, const char* cap, const char* value, const char* unit,
              const NeuFieldStyle& o = NeuFieldStyle());
// Repaint only the field's contents and focus ring (no shadow redraw).
void neuFieldValue(int x, int y, int w, int h, const char* cap, const char* value, const char* unit,
                   const NeuFieldStyle& o = NeuFieldStyle());
// Thin progress bar for use inside a well.
void neuProgressBar(int x, int y, int w, float frac, uint16_t color = NEU_ACCENT);
// Small amber hint line with a warning triangle (centred).
void neuHint(const char* msg, int y, uint16_t color = NEU_HOLD);

// Modal dialog.  okLabel null = no buttons (progress); cancelLabel null = a
// single centred OK.  Missing buttons come back with w == 0.
struct NeuRect { int x, y, w, h; };
struct NeuDialogRects { NeuRect cancel, ok; };
NeuDialogRects neuDialog(const char* title, const char* const* lines, int nLines,
                         const char* cancelLabel, const char* okLabel,
                         uint16_t okColor = NEU_RUN, uint16_t titleColor = NEU_TEXT);
bool neuDialogHit(const NeuRect& r, int x, int y);

// ---------- DRO (work + machine), used by Main Menu, Status, Work Area ----------
constexpr int NEU_DRO_H = 68;
// Positions are copied under stateMutex once per tick and compared whole, so a
// repaint happens only when something on screen would change.
struct NeuDroSnap {
    float wpos[PENDANT_MAX_AXES];
    float mpos[PENDANT_MAX_AXES];
    int   numAxes;
    bool  inInches;
    char  wcs[8];
};
bool neuDroSnapshot(NeuDroSnap& s);       // false = lock busy, skip this frame
bool neuDroSame(const NeuDroSnap& a, const NeuDroSnap& b);
void neuFmtPos(char* out, size_t n, float v, bool inInches);
void neuDroLabels(int x, int y, const NeuDroSnap& s);
void neuDro(int x, int y, const NeuDroSnap& s);
// Repaint just the numbers (from the 100 ms tick when positions change).
void neuDroValues(int x, int y, const NeuDroSnap& s);

// ---------- list screens (Files, Macros, Machines) ----------
// crumb strip y 38 h 26 (path left, page readout right — tap it to advance a page)
// list well   y 72 h 198, LIST_ROWS rows at a 38 px pitch; the selected row is a raised pill
constexpr int NEU_LIST_CRUMB_Y = 38;
constexpr int NEU_LIST_WELL_Y  = 72;
constexpr int NEU_LIST_WELL_H  = 198;
constexpr int NEU_LIST_ROW_Y0  = 77;
constexpr int NEU_LIST_PITCH   = 38;
constexpr int NEU_LIST_ROW_H   = 34;
enum NeuRowKind : uint8_t { NEU_ROW_DIR, NEU_ROW_UP, NEU_ROW_FILE, NEU_ROW_MACRO };
inline int neuListRowY(int i) { return NEU_LIST_ROW_Y0 + i * NEU_LIST_PITCH; }
// Index of the row under (x,y), or -1.
int  neuListRowHit(int x, int y);
bool neuListPageHit(int x, int y);
void neuListFrame();
// Repaint the crumb strip contents.  page/pages 1-based; pages 0 = nothing listed.
void neuListCrumb(NeuIcon icon, const char* text, int page, int pages, bool truncated);
void neuListClear();
void neuListRow(int i, const char* label, NeuRowKind kind, bool sel);
void neuListMessage(const char* line1, const char* line2, uint16_t color = NEU_TEXT2);
