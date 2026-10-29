/* screen_feeds_speeds.cpp (Overrides) — neumorphic layout (spec: simulator/js/screens/feeds.js)
 *   feed card     y 38  h 50   big override % + actual feed; tap = dial adjusts it (accent ring)
 *   feed presets  y 96  h 34   50 / 75 / 100 / 125 / 150 (selected = the preset matching the value)
 *   spindle card  y 146 h 50   same, for spindle override + actual RPM
 *   spindle presets y 204 h 34
 *   nav           Home | Reset to 100%
 */
#include "pendant_shared.h"
#include "neu_ui.h"
#include "screen_feeds_speeds.h"

static const int         OVR_PRESETS[5]     = { 50, 75, 100, 125, 150 };
static const char* const OVR_PRESET_TXT[5] = { "50", "75", "100", "125", "150" };
static const int OVR_FEED_Y = 38, OVR_SPIN_Y = 146, OVR_CARD_H = 50, OVR_SEG_OFF = 58, OVR_SEG_H = 34;

enum { NAV_ID_RESET = NAV_ID_FIRST };
static const NeuNavItem kNav[] = { NAV_HOME, { NAV_ID_RESET, "Reset to 100%", ICON_REFRESH } };
static const int        kNavN  = 2;

// Change key per card; pct is also the presets key (they follow the value).
struct OvrKey { int pct; bool focus; int actual; bool inch; };
static OvrKey s_feedKey, s_spinKey;
static bool   s_feedValid, s_spinValid;

void enterFeedsSpeeds() {
    releasePanelSprites();
    s_feedValid = s_spinValid = false;
}

void exitFeedsSpeeds() {
    pendantFeeds.dialMode = 0;
    releasePanelSprites();
}

void drawFeedsSpeedsScreen() {
    neuHeader("Overrides");
    s_feedValid = s_spinValid = false;
    neuInset(10, OVR_FEED_Y, 220, OVR_CARD_H, 12, 3);
    neuInset(10, OVR_SPIN_Y, 220, OVR_CARD_H, 12, 3);
    redrawFeedOverrideButtons();
    redrawSpindleOverrideButtons();
    neuCaption("Tap a card, then turn the dial", 120, 254, NEU_TEXT3, NEU_CENTER);
    neuNav(kNav, kNavN);
}

// The focus ring sits on the background around the card, so it's drawn direct;
// the interior repaints through the panel sprite.
static void ovrCard(int y, const char* cap, int pct, const char* actual, const char* unit, bool focus) {
    neuDrawRoundRect(7, y - 3, 226, OVR_CARD_H + 6, 15, focus ? NEU_ACCENT : NEU_BG);
    neuDrawRoundRect(8, y - 2, 224, OVR_CARD_H + 4, 14, focus ? NEU_ACCENT : NEU_BG);
    char capBuf[32], pctBuf[8];
    snprintf(capBuf, sizeof(capBuf), focus ? "%s - dial" : "%s", cap);
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
    neuPanelBegin(14, y + 4, 213, OVR_CARD_H - 7);
    neuFillRect(14, y + 4, 213, OVR_CARD_H - 7, NEU_WELL);
    neuCaption(capBuf, 22, y + 8, focus ? NEU_ACCENT : NEU_TEXT3);
    neuCaption(unit, 218, y + 8, NEU_TEXT3, NEU_RIGHT);
    neuText(pctBuf, 22, y + 44, NEU_B18, NEU_TEXT);
    neuText(actual, 218, y + 40, NEU_B12, NEU_TEXT2, NEU_RIGHT);
    neuPanelEnd();
}

static int presetIndex(int pct) {
    for (int i = 0; i < 5; i++)
        if (OVR_PRESETS[i] == pct) return i;
    return -1;
}

static void ovrPresets(int y, int pct) {
    neuClear(10, y, 220, OVR_SEG_H);
    neuSegmented(10, y, 220, OVR_SEG_H, OVR_PRESET_TXT, 5, presetIndex(pct));
}

void updateFeedsSpeedsTopDisplay() {
    updateFeedOverrideDisplay();
    updateSpindleOverrideDisplay();
}

void updateFeedOverrideDisplay() {
    if (currentPendantScreen != PSCREEN_FEEDS_SPEEDS) return;
    OvrKey k;
    memset(&k, 0, sizeof(k));
    k.pct    = overrideFeedShown();
    k.focus  = pendantFeeds.dialMode == 1;
    k.actual = pendantMachine.feedRate;
    k.inch   = pendantMachine.inInches;
    if (s_feedValid && memcmp(&k, &s_feedKey, sizeof(k)) == 0) return;
    const bool presetChanged = !s_feedValid || k.pct != s_feedKey.pct;
    s_feedKey   = k;
    s_feedValid = true;
    char actual[12];
    snprintf(actual, sizeof(actual), "%d", k.actual);
    ovrCard(OVR_FEED_Y, "Feed override", k.pct, actual, k.inch ? "in/min" : "mm/min", k.focus);
    if (presetChanged) ovrPresets(OVR_FEED_Y + OVR_SEG_OFF, k.pct);
}

void updateSpindleOverrideDisplay() {
    if (currentPendantScreen != PSCREEN_FEEDS_SPEEDS) return;
    OvrKey k;
    memset(&k, 0, sizeof(k));
    k.pct    = overrideSpindleShown();
    k.focus  = pendantFeeds.dialMode == 2;
    k.actual = pendantMachine.spindleRPM;
    if (s_spinValid && memcmp(&k, &s_spinKey, sizeof(k)) == 0) return;
    const bool presetChanged = !s_spinValid || k.pct != s_spinKey.pct;
    s_spinKey   = k;
    s_spinValid = true;
    char actual[12];
    snprintf(actual, sizeof(actual), "%d", k.actual);
    ovrCard(OVR_SPIN_Y, "Spindle override", k.pct, actual, "rpm", k.focus);
    if (presetChanged) ovrPresets(OVR_SPIN_Y + OVR_SEG_OFF, k.pct);
}

void redrawFeedOverrideButtons()    { s_feedValid = false; updateFeedOverrideDisplay(); }
void redrawSpindleOverrideButtons() { s_spinValid = false; updateSpindleOverrideDisplay(); }

// Request a feed/spindle override %.  The actual real-time bytes are emitted by a
// paced coarse+fine stepper in the periodic loop (see CNC_Pendant_UI.cpp) — not a
// tight burst — so a Modbus VFD isn't flooded and FluidNC's task watchdog can't
// be starved.  The stepper anchors off the reported value (no reset-to-100%).
static void applyFeedOverride(int targetPct) {
    if (!pendantConnected) return;
    overrideSetFeedTarget(targetPct);
}

static void applySpindleOverride(int targetPct) {
    if (!pendantConnected) return;
    overrideSetSpindleTarget(targetPct);
}

void handleFeedsSpeedsTouch(int x, int y) {
    const int fSeg = neuSegHit(10, OVR_FEED_Y + OVR_SEG_OFF, 220, OVR_SEG_H, 5, x, y);
    if (fSeg >= 0) {
        pendantFeeds.dialMode = 0;
        pendantFeeds.selectedFeedOverride = fSeg;
        applyFeedOverride(OVR_PRESETS[fSeg]);
        redrawFeedOverrideButtons();
        updateSpindleOverrideDisplay();   // drop the spindle card's dial ring
        return;
    }
    const int sSeg = neuSegHit(10, OVR_SPIN_Y + OVR_SEG_OFF, 220, OVR_SEG_H, 5, x, y);
    if (sSeg >= 0) {
        pendantFeeds.dialMode = 0;
        pendantFeeds.selectedSpindleOverride = sSeg;
        applySpindleOverride(OVR_PRESETS[sSeg]);
        redrawSpindleOverrideButtons();
        updateFeedOverrideDisplay();
        return;
    }
    // Tapping a card hands it the dial; tapping it again takes the dial back.
    if (isTouchInBounds(x, y, 10, OVR_FEED_Y, 220, OVR_CARD_H)) {
        pendantFeeds.dialMode = pendantFeeds.dialMode == 1 ? 0 : 1;
        updateFeedOverrideDisplay();
        updateSpindleOverrideDisplay();
        return;
    }
    if (isTouchInBounds(x, y, 10, OVR_SPIN_Y, 220, OVR_CARD_H)) {
        pendantFeeds.dialMode = pendantFeeds.dialMode == 2 ? 0 : 2;
        updateFeedOverrideDisplay();
        updateSpindleOverrideDisplay();
        return;
    }
    switch (neuNavHit(kNav, kNavN, x, y)) {
    case NAV_ID_HOME:
        currentPendantScreen = PSCREEN_MAIN_MENU;
        break;
    case NAV_ID_RESET:
        pendantFeeds.dialMode = 0;
        pendantFeeds.selectedFeedOverride    = 2;
        pendantFeeds.selectedSpindleOverride = 2;
        applyFeedOverride(100);
        applySpindleOverride(100);
        updateFeedOverrideDisplay();
        updateSpindleOverrideDisplay();
        break;
    }
}
