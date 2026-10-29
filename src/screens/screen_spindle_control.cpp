/* screen_spindle_control.cpp — neumorphic layout (spec: simulator/js/screens/spindle.js)
 *   RPM card     y 38  h 58   actual RPM (left) | target RPM (right; tap = dial adjusts it)
 *   direction    y 104 h 36   Fwd M3 / Rev M4
 *   presets      y 162 h 34   quarter / half / max of the spindle's range
 *   Start / Stop y 208 h 60   the only solid-filled buttons in the UI; Start reads
 *                             "Running" (pressed in) while the spindle is on
 *   nav          Home | Overrides
 */
#include "pendant_shared.h"
#include "neu_ui.h"
#include "screen_spindle_control.h"
#include "../CNC_Pendant_UI.h"

static const int SPN_CARD_Y = 38, SPN_CARD_H = 58, SPN_DIR_Y = 104, SPN_PRE_Y = 162;
static const int SPN_BTN_Y = 208, SPN_BTN_H = 60;

enum { NAV_ID_OVERRIDES = NAV_ID_FIRST };
static const NeuNavItem kNav[] = { NAV_HOME, { NAV_ID_OVERRIDES, "Overrides", ICON_GAUGE } };
static const int        kNavN  = 2;

// Compute dynamic presets: 25%, 50%, 100% of max, floored to nearest 100 RPM
static void getSpindlePresets(int presets[3]) {
    int maxRPM = pendantMachine.spindleMaxRPM > 0 ? pendantMachine.spindleMaxRPM : 24000;
    int minRPM = pendantMachine.spindleMinRPM;
    presets[0] = max(minRPM, (maxRPM / 4 / 100) * 100);
    presets[1] = max(minRPM, (maxRPM / 2 / 100) * 100);
    presets[2] = maxRPM;
}

// Format RPM as "Xk" if divisible by 1000, else plain number
static void fmtRPM(int rpm, char* out, size_t n) {
    if (rpm >= 1000 && rpm % 1000 == 0) snprintf(out, n, "%dk", rpm / 1000);
    else                                snprintf(out, n, "%d", rpm);
}

// Change keys: the card, and the Start button's running state (-1 = not drawn).
struct SpnKey { int actual, target; bool dial; };
static SpnKey s_cardKey;
static bool   s_cardValid = false;
static int    s_runKey    = -1;

static void spnStartStop();

void enterSpindleControl() {
    releasePanelSprites();

    // Defensive re-fetch of $30/$31 on entry. The connect-edge fetch is the
    // primary source, but if it was dropped (UART contention at connect time)
    // we'd silently fall back to default 24000/0 — observable as preset
    // buttons reverting to defaults after a Start/Stop cycle. Re-querying
    // here costs two short bytes and guarantees current values.
    requestSpindleConfig();

    // Initialise targetRPM from selected preset on first entry
    if (pendantSpindle.targetRPM == 0) {
        int presets[3];
        getSpindlePresets(presets);
        pendantSpindle.targetRPM = presets[pendantSpindle.selectedPreset];
    }
}

void exitSpindleControl() {
    pendantSpindle.dialMode = false;
    releasePanelSprites();
}

void drawSpindleControlScreen() {
    neuHeader("Spindle");
    s_cardValid = false;
    s_runKey    = -1;
    neuInset(10, SPN_CARD_Y, 220, SPN_CARD_H, 12, 3);
    updateSpindleRPMDisplay();
    redrawSpindleDirectionButtons();
    redrawSpindlePresetButtons();
    spnStartStop();
    neuNav(kNav, kNavN);
}

void updateSpindleRPMDisplay() {
    if (currentPendantScreen != PSCREEN_SPINDLE_CONTROL) return;
    SpnKey k;
    memset(&k, 0, sizeof(k));
    k.actual = pendantMachine.spindleRPM;
    k.target = pendantSpindle.targetRPM;
    k.dial   = pendantSpindle.dialMode;
    if (!s_cardValid || memcmp(&k, &s_cardKey, sizeof(k)) != 0) {
        s_cardKey   = k;
        s_cardValid = true;
        const int y = SPN_CARD_Y;
        // The focus ring sits on the background, so it's drawn direct; the
        // interior repaints through the panel sprite.
        neuDrawRoundRect(7, y - 3, 226, SPN_CARD_H + 6, 15, k.dial ? NEU_ACCENT : NEU_BG);
        neuDrawRoundRect(8, y - 2, 224, SPN_CARD_H + 4, 14, k.dial ? NEU_ACCENT : NEU_BG);
        char actual[12], target[12];
        snprintf(actual, sizeof(actual), "%d", k.actual);
        snprintf(target, sizeof(target), "%d", k.target);
        neuPanelBegin(14, y + 4, 213, SPN_CARD_H - 7);
        neuFillRect(14, y + 4, 213, SPN_CARD_H - 7, NEU_WELL);
        neuCaption("Actual", 22, y + 9);
        neuText(actual, 22, y + 50, k.actual >= 10000 ? NEU_B18 : NEU_B24, NEU_TEXT);
        neuFillRect(124, y + 10, 1, 38, NEU_SH_DARK);
        neuFillRect(125, y + 10, 1, 38, NEU_SH_LIGHT);
        neuCaption(k.dial ? "Target - dial" : "Target", 136, y + 9, k.dial ? NEU_ACCENT : NEU_TEXT3);
        neuText(target, 218, y + 44, NEU_B12, NEU_ACCENT, NEU_RIGHT);
        neuFillRoundRect(136, y + 49, 82, 2, 1, k.dial ? NEU_ACCENT : NEU_SH_LIGHT);
        neuPanelEnd();
    }
    // The spindle can be stopped from elsewhere (E-stop, a job ending).
    if (s_runKey != -1 && s_runKey != (int)pendantMachine.spindleRunning) spnStartStop();
}

void redrawSpindleDirectionButtons() {
    if (currentPendantScreen != PSCREEN_SPINDLE_CONTROL) return;
    static const char* const kDirs[2] = { "Fwd  M3", "Rev  M4" };
    neuClear(10, SPN_DIR_Y, 220, 36);
    neuSegmented(10, SPN_DIR_Y, 220, 36, kDirs, 2, pendantSpindle.directionFwd ? 0 : 1);
}

void redrawSpindlePresetButtons() {
    if (currentPendantScreen != PSCREEN_SPINDLE_CONTROL) return;
    int  presets[3];
    char txt[3][12], range[24];
    getSpindlePresets(presets);
    int sel = -1;
    for (int i = 0; i < 3; i++) {
        fmtRPM(presets[i], txt[i], sizeof(txt[i]));
        if (sel < 0 && presets[i] == pendantSpindle.targetRPM) sel = i;
    }
    const char* const items[3] = { txt[0], txt[1], txt[2] };
    snprintf(range, sizeof(range), "%d - %d", pendantMachine.spindleMinRPM, pendantMachine.spindleMaxRPM);
    neuFillRect(10, SPN_PRE_Y - 13, 220, 10, NEU_BG);
    neuCaption("Preset rpm", 12, SPN_PRE_Y - 12);
    neuCaption(range, 228, SPN_PRE_Y - 12, NEU_TEXT3, NEU_RIGHT);
    neuClear(10, SPN_PRE_Y, 220, 34);
    neuSegmented(10, SPN_PRE_Y, 220, 34, items, 3, sel);
    updateSpindleRPMDisplay();
}

static void spnStartStop() {
    const bool running = pendantMachine.spindleRunning;
    s_runKey = running;
    neuClear(10, SPN_BTN_Y, 104, SPN_BTN_H);
    NeuBtnStyle start;
    if (running) {
        start.on    = true;
        start.color = NEU_RUN;
        start.bar   = false;
        neuButton(10, SPN_BTN_Y, 104, SPN_BTN_H, "Running", start);
    } else {
        start.face  = NEU_GO_FACE;
        start.color = NEU_TEXT;
        start.icon  = ICON_PLAY;
        start.font  = NEU_B12;
        neuButton(10, SPN_BTN_Y, 104, SPN_BTN_H, "Start", start);
    }
    NeuBtnStyle stop;
    stop.face  = NEU_STOP_FACE;
    stop.color = NEU_TEXT;
    stop.icon  = ICON_STOP;
    stop.font  = NEU_B12;
    neuClear(126, SPN_BTN_Y, 104, SPN_BTN_H);
    neuButton(126, SPN_BTN_Y, 104, SPN_BTN_H, "Stop", stop);
}

static void setSpindleRunning(bool on) {
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        pendantMachine.spindleRunning = on;
        xSemaphoreGive(stateMutex);
    }
}

void handleSpindleControlTouch(int x, int y) {
    const int dir = neuSegHit(10, SPN_DIR_Y, 220, 36, 2, x, y);
    if (dir >= 0) {
        pendantSpindle.directionFwd = dir == 0;
        if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            pendantMachine.spindleDir = dir == 0 ? "Fwd" : "Rev";
            xSemaphoreGive(stateMutex);
        }
        redrawSpindleDirectionButtons();
        return;
    }

    const int pre = neuSegHit(10, SPN_PRE_Y, 220, 34, 3, x, y);
    if (pre >= 0) {
        int presets[3];
        getSpindlePresets(presets);
        pendantSpindle.dialMode       = false;
        pendantSpindle.selectedPreset = pre;
        pendantSpindle.targetRPM      = presets[pre];
        redrawSpindlePresetButtons();
        return;
    }

    // RPM card — tap toggles dial mode (the dial then steps the target)
    if (isTouchInBounds(x, y, 10, SPN_CARD_Y, 220, SPN_CARD_H)) {
        pendantSpindle.dialMode = !pendantSpindle.dialMode;
        if (pendantSpindle.dialMode) {
            int maxRPM = pendantMachine.spindleMaxRPM > 0 ? pendantMachine.spindleMaxRPM : 24000;
            pendantSpindle.targetRPM = constrain(pendantSpindle.targetRPM, pendantMachine.spindleMinRPM, maxRPM);
        }
        updateSpindleRPMDisplay();
        return;
    }

    if (isTouchInBounds(x, y, 10, SPN_BTN_Y, 104, SPN_BTN_H)) {
        if (pendantConnected) {
            char cmd[32];
            // M3 = clockwise (Fwd), M4 = counterclockwise (Rev)
            snprintf(cmd, sizeof(cmd), "%s S%d", pendantSpindle.directionFwd ? "M3" : "M4", pendantSpindle.targetRPM);
            send_line(cmd);
        }
        setSpindleRunning(true);
        spnStartStop();
        return;
    }

    if (isTouchInBounds(x, y, 126, SPN_BTN_Y, 104, SPN_BTN_H)) {
        if (pendantConnected) send_line("M5");
        setSpindleRunning(false);
        spnStartStop();
        return;
    }

    switch (neuNavHit(kNav, kNavN, x, y)) {
    case NAV_ID_HOME:      currentPendantScreen = PSCREEN_MAIN_MENU;    break;
    case NAV_ID_OVERRIDES: currentPendantScreen = PSCREEN_FEEDS_SPEEDS; break;
    }
}
