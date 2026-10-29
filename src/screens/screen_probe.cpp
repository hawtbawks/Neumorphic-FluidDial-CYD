/*
 * screen_probe.cpp  —  SCR0: Probe Hub, neumorphic layout (spec: simulator/js/screens/probe.js)
 *   probe type   segmented  Z Plate / XYZ / 3D
 *   shared       2x2 fields (tap = dial adjusts it; tap again or elsewhere releases)
 *   routines     1, 2 or 4 raised buttons depending on the probe type
 *   nav          Home | Configure (plate or 3D probe setup)
 *
 * Also holds NVS persistence, the shared G-code helpers and the probe kit
 * (spec: simulator/js/screens/probe_kit.js) used by the other probe screens.
 */

#include "pendant_shared.h"
#include "screen_probe.h"
#include <Preferences.h>

extern Preferences preferences;

// ── NVS persistence ─────────────────────────────────────────────────────────

void saveProbeSettings() {
    preferences.begin("probe", false);
    preferences.putInt  ("probeType",   pendantProbeV2.probeTypeIdx);
    preferences.putFloat("probeRate",   pendantProbeV2.probeRate);
    preferences.putFloat("seekRate",    pendantProbeV2.seekRate);
    preferences.putFloat("retractDist", pendantProbeV2.retractDist);
    preferences.putFloat("maxZTravel",  pendantProbeV2.maxZTravel);
    preferences.putFloat("ballDia",     pendantProbeV2.ballDia);
    preferences.putFloat("deflection",  pendantProbeV2.deflection);
    preferences.putFloat("plateThick",  pendantProbeV2.plateThick);
    preferences.putFloat("plateWidth",  pendantProbeV2.plateWidth);
    preferences.putFloat("plateOffX",   pendantProbeV2.plateOffX);
    preferences.putFloat("plateOffY",   pendantProbeV2.plateOffY);
    preferences.putInt  ("cornerIdx",   pendantProbeV2.cornerIdx);
    preferences.putInt  ("axesIdx",     pendantProbeV2.axesIdx);
    preferences.putFloat("cornerDepth", pendantProbeV2.cornerDepth);
    preferences.putFloat("cornerOver",  pendantProbeV2.cornerOver);
    preferences.putFloat("cornerRetXY", pendantProbeV2.cornerRetXY);
    preferences.putFloat("boreDia",     pendantProbeV2.boreDia);
    preferences.putFloat("boreDepth",   pendantProbeV2.boreDepth);
    preferences.putFloat("boreOffset",  pendantProbeV2.boreOffset);
    preferences.putInt  ("borePasses",  pendantProbeV2.borePasses);
    preferences.putFloat("bossDia",     pendantProbeV2.bossDia);
    preferences.putFloat("bossDepth",   pendantProbeV2.bossDepth);
    preferences.putFloat("bossClear",   pendantProbeV2.bossClear);
    preferences.putInt  ("bossPasses",  pendantProbeV2.bossPasses);
    preferences.putBool ("bossRect",    pendantProbeV2.bossRect);
    preferences.putFloat("bossSizeY",   pendantProbeV2.bossSizeY);
    preferences.putFloat("calGaugeW",   pendantProbeV2.calGaugeWidth);
    preferences.end();
}

void loadProbeSettings() {
    preferences.begin("probe", true);
    pendantProbeV2.probeTypeIdx = preferences.getInt  ("probeType",   0);
    pendantProbeV2.probeRate    = preferences.getFloat("probeRate",   150.0f);
    pendantProbeV2.seekRate     = preferences.getFloat("seekRate",    500.0f);
    pendantProbeV2.retractDist  = preferences.getFloat("retractDist", 20.0f);
    pendantProbeV2.maxZTravel   = preferences.getFloat("maxZTravel",  20.0f);
    pendantProbeV2.ballDia      = preferences.getFloat("ballDia",      2.0f);
    pendantProbeV2.deflection   = preferences.getFloat("deflection",   0.0f);
    pendantProbeV2.plateThick   = preferences.getFloat("plateThick",  10.0f);
    pendantProbeV2.plateWidth   = preferences.getFloat("plateWidth",  50.0f);
    pendantProbeV2.plateOffX    = preferences.getFloat("plateOffX",    0.0f);
    pendantProbeV2.plateOffY    = preferences.getFloat("plateOffY",    0.0f);
    pendantProbeV2.cornerIdx    = preferences.getInt  ("cornerIdx",   0);
    pendantProbeV2.axesIdx      = preferences.getInt  ("axesIdx",     0);
    pendantProbeV2.cornerDepth  = preferences.getFloat("cornerDepth",  5.0f);
    pendantProbeV2.cornerOver   = preferences.getFloat("cornerOver",   2.0f);
    pendantProbeV2.cornerRetXY  = preferences.getFloat("cornerRetXY",  3.0f);
    pendantProbeV2.boreDia      = preferences.getFloat("boreDia",     60.0f);
    pendantProbeV2.boreDepth    = preferences.getFloat("boreDepth",    8.0f);
    pendantProbeV2.boreOffset   = preferences.getFloat("boreOffset",   5.0f);
    pendantProbeV2.borePasses   = preferences.getInt  ("borePasses",  2);
    pendantProbeV2.bossDia      = preferences.getFloat("bossDia",     60.0f);
    pendantProbeV2.bossDepth    = preferences.getFloat("bossDepth",    5.0f);
    pendantProbeV2.bossClear    = preferences.getFloat("bossClear",    5.0f);
    pendantProbeV2.bossPasses   = preferences.getInt  ("bossPasses",  2);
    pendantProbeV2.bossRect     = preferences.getBool ("bossRect",    false);
    pendantProbeV2.bossSizeY    = preferences.getFloat("bossSizeY",   60.0f);
    pendantProbeV2.calGaugeWidth = preferences.getFloat("calGaugeW",  50.8f);
    preferences.end();

    // Clamp integral fields
    pendantProbeV2.probeTypeIdx = constrain(pendantProbeV2.probeTypeIdx, 0, PROBE_TYPE_COUNT - 1);
    pendantProbeV2.cornerIdx    = constrain(pendantProbeV2.cornerIdx,    0, 3);
    pendantProbeV2.axesIdx      = constrain(pendantProbeV2.axesIdx,      0, 2);
    pendantProbeV2.borePasses   = constrain(pendantProbeV2.borePasses,   1, 4);
    pendantProbeV2.bossPasses   = constrain(pendantProbeV2.bossPasses,   1, 4);
}

// ── Shared G-code helpers ────────────────────────────────────────────────────

// Dial acceleration — returns effective step multiplier.
// Accelerates to 10× after 5 rapid same-direction detents within 500 ms.
float probeDialStep(int delta, float baseStep) {
    unsigned long now = millis();
    if ((now - pendantProbeV2.dialLastMs) < 500) {
        pendantProbeV2.dialAccelCount++;
    } else {
        pendantProbeV2.dialAccelCount = 0;
    }
    pendantProbeV2.dialLastMs = now;
    float mult = (pendantProbeV2.dialAccelCount >= 5) ? 10.0f : 1.0f;
    return baseStep * mult;
}

// Crash-safe two-pass approach: fast seek to contact, back off, slow re-probe.
// Ends just PAST the fine trigger — G38.2 records where the probe input fired,
// but the machine keeps moving while it decelerates.  The trigger point itself is
// in #5061/#5062/#5063; callers must probeReturnToTrigger() before zeroing a WCS
// axis, or they bake the overshoot into the work offset.  Stays G91.
void probeSeekFine(const char* axis, float seekDist, float seekF, float fineF) {
    const float BACKOFF = 1.5f;
    int  dir = (seekDist >= 0.0f) ? 1 : -1;
    char b[96];
    snprintf(b, sizeof(b), "G38.2 %s%.3f F%.0f", axis, seekDist, seekF);                send_line(b);
    snprintf(b, sizeof(b), "G0 %s%.3f F1000",    axis, -dir * BACKOFF);                 send_line(b);
    snprintf(b, sizeof(b), "G38.2 %s%.3f F%.0f", axis, dir * (BACKOFF + 1.0f), fineF);  send_line(b);
}

// Move back to the exact G38.2 trigger point before zeroing a WCS axis — see the
// header for why.  Uses the same G53-to-a-probe-parameter idiom as the bore/boss
// centre move.  Caller must already be in G90.
void probeReturnToTrigger(const char* axis) {
    const char* param = (*axis == 'X') ? "5061"
                      : (*axis == 'Y') ? "5062"
                                       : "5063";
    char b[32];
    snprintf(b, sizeof(b), "G53 G0 %s#%s", axis, param);
    send_line(b);
}

static const char* const kCoords[4] = { "G54", "G55", "G56", "G57" };

// Cycle G54 → G55 → G56 → G57 → G54.  Selection only — the card doesn't switch
// the active WCS (no surprise DRO jumps while choosing); the probe routine
// activates and zeroes the chosen system when you actually probe.
void probeCycleWorkArea() {
    pendantProbing.selectedCoordIndex  = (pendantProbing.selectedCoordIndex + 1) % 4;
    pendantProbing.selectedCoordSystem = kCoords[pendantProbing.selectedCoordIndex];
}

// Activate the selected WCS on the controller so the probe's G10 L20 zero is
// immediately in effect in the system shown on screen.
void probeActivateWcs() {
    send_line(kCoords[pendantProbing.selectedCoordIndex]);
}

// ── Probe kit ────────────────────────────────────────────────────────────────

const NeuRect PRK_SEQ  = { 10, 70, 110, 96 };
const NeuRect PRK_DIAG = { 10, 174, 110, 76 };

const NeuNavItem PROBE_ROUTINE_NAV[3] = {
    NAV_HOME, NAV_BACK, { PRK_NAV_PROBE, "Probe", ICON_PLAY, 0, false, false, false, NEU_RUN },
};
const NeuNavItem PROBE_CFG_NAV[3] = {
    NAV_HOME, NAV_BACK, { PRK_NAV_SAVE, "Save", ICON_NONE, 0, false, true },
};

static NeuDialogRects s_prkDialog;

// Live work X / Y / Z; repaints only when a position or the unit changes.
void prkPosStrip(bool force) {
    static float last[3];
    static bool  lastInch;
    float pos[3];
    bool  inch;
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(5)) != pdTRUE) {
        if (!force) return;                       // try again next tick
        memcpy(pos, last, sizeof(pos));
        inch = lastInch;
    } else {
        for (int i = 0; i < 3; i++) pos[i] = pendantMachine.wpos[i];
        inch = pendantMachine.inInches;
        xSemaphoreGive(stateMutex);
    }
    if (!force && memcmp(pos, last, sizeof(pos)) == 0 && inch == lastInch) return;
    memcpy(last, pos, sizeof(pos));
    lastInch = inch;
    if (force) neuInset(10, PRK_POS_Y, 220, PRK_POS_H, 8, 2);
    char buf[16];
    neuPanelBegin(13, PRK_POS_Y + 3, 215, PRK_POS_H - 5);
    neuFillRect(13, PRK_POS_Y + 3, 215, PRK_POS_H - 5, NEU_WELL);
    for (int i = 0; i < 3; i++) {
        const int x = 20 + i * 72;
        snprintf(buf, sizeof(buf), "%.*f", inch ? 3 : 2, pos[i]);
        neuCaption(AXIS_NAMES[i], x, PRK_POS_Y + 8, NEU_AX[i]);
        neuSmall(buf, x + 64, PRK_POS_Y + 8, NEU_TEXT2, NEU_RIGHT);
    }
    neuPanelEnd();
}

// Numbered sequence — describes what the routine will do, in order.
void prkSequence(const char* const* steps, int n, const NeuRect& r) {
    const int pitch = r.h >= 90 ? 22 : 17, top = r.y + (r.h >= 90 ? 28 : 24);
    neuInset(r.x, r.y, r.w, r.h, 10, 3);
    neuCaption("Sequence", r.x + 10, r.y + 9);
    for (int i = 0; i < n; i++) {
        const int  yy  = top + i * pitch;
        const bool cur = i == 0;
        char num[4];
        snprintf(num, sizeof(num), "%d", i + 1);
        neuDrawCircle(r.x + 16, yy + 3, 6, cur ? NEU_ACCENT : NEU_TEXT3);
        neuSmall(num, r.x + 14, yy, cur ? NEU_ACCENT : NEU_TEXT3);
        neuSmall(steps[i], r.x + 27, yy, cur ? NEU_TEXT : NEU_TEXT2);
    }
}

void prkDiagramWell(const NeuRect& r) { neuInset(r.x, r.y, r.w, r.h, 10, 3); }

void prkResultCard(const NeuRect& r, const char* what) {
    neuInset(r.x, r.y, r.w, r.h, 10, 3);
    prkResultValue(r, what);
}

// Tall cards (h >= 60) show "Sets <what> in" above the WCS; short ones put it on one line.
void prkResultValue(const NeuRect& r, const char* what) {
    const char* wcs = pendantProbing.selectedCoordSystem.c_str();
    char        line[24];
    neuPanelBegin(r.x + 4, r.y + 4, r.w - 7, r.h - 7);
    neuFillRect(r.x + 4, r.y + 4, r.w - 7, r.h - 7, NEU_WELL);
    if (r.h >= 60) {
        snprintf(line, sizeof(line), "Sets %s in", what);
        neuCaption("Result", r.x + 10, r.y + 9);
        neuSmall(line, r.x + 10, r.y + 24, NEU_TEXT2);
        neuText(wcs, r.x + 10, r.y + r.h - 9, NEU_B12, NEU_RUN);
        neuCaption("tap", r.x + r.w - 8, r.y + r.h - 16, NEU_TEXT3, NEU_RIGHT);
    } else {
        snprintf(line, sizeof(line), "Sets %s", what);
        neuCaption(line, r.x + 10, r.y + 8);
        neuText(wcs, r.x + 10, r.y + r.h - 7, NEU_B9, NEU_RUN);
        neuCaption("tap", r.x + r.w - 8, r.y + r.h - 15, NEU_TEXT3, NEU_RIGHT);
    }
    neuPanelEnd();
}

void prkHint(const char* msg, bool red) { neuHint(msg, PRK_HINT_Y, red ? NEU_ALARM : NEU_HOLD); }

void prkConfirm(const char* routine) {
    char line2[32];
    snprintf(line2, sizeof(line2), "Zeroes into %s", pendantProbing.selectedCoordSystem.c_str());
    const char* lines[2] = { routine, line2 };
    s_prkDialog = neuDialog("Start probing?", lines, 2, "Cancel", "Probe", NEU_RUN);
}

const char* prkLen(float mm, int decMm, char* out, size_t n) {
    if (pendantMachine.inInches) {
        snprintf(out, n, "%.3f", mm / 25.4f);
        return "in";
    }
    snprintf(out, n, "%.*f", decMm, mm);
    return "mm";
}

bool prkHandleTouch(int x, int y, const PrkTouch& o) {
    auto& p = pendantProbeV2;
    // The confirm dialog owns all input while it is up.
    if (p.confirmActive) {
        if (neuDialogHit(s_prkDialog.cancel, x, y)) {
            p.confirmActive = false;
            o.redraw();
            suppressTouchBounce();
        } else if (neuDialogHit(s_prkDialog.ok, x, y)) {
            p.confirmActive = false;
            o.run();
            currentPendantScreen = PSCREEN_STATUS;
        }
        return true;
    }
    for (int i = 0; i < o.nFields; i++) {
        const NeuRect& f = o.fields[i];
        if (isTouchInBounds(x, y, f.x, f.y, f.w, f.h)) {
            p.focusedField = p.focusedField == i ? -1 : i;
            o.redrawFields();
            return true;
        }
    }
    if (o.result && isTouchInBounds(x, y, o.result->x, o.result->y, o.result->w, o.result->h)) {
        probeCycleWorkArea();
        prkResultValue(*o.result, o.resultWhat);
        return true;
    }
    if (o.extra && o.extra(x, y)) return true;
    switch (neuNavHit(PROBE_ROUTINE_NAV, 3, x, y)) {
    case NAV_ID_HOME:
        currentPendantScreen = PSCREEN_MAIN_MENU;
        return true;
    case NAV_ID_BACK:
        p.returnScreen       = currentPendantScreen;
        currentPendantScreen = PSCREEN_PROBE;
        return true;
    case PRK_NAV_PROBE:
        if (!pendantConnected) {
            prkHint("Not connected", true);
            return true;
        }
        p.confirmActive = true;
        prkConfirm(o.name);
        suppressTouchBounce();   // a held tap must not land on the dialog
        return true;
    }
    return false;
}

void prkProbeTool(int cx, int tipY, int len) {
    neuFillRoundRect(cx - 5, tipY - len - 8, 11, 9, 2, NEU_TEXT3);
    neuFillRect(cx - 1, tipY - len, 3, len - 3, NEU_TEXT2);
    neuFillCircle(cx, tipY - 2, 3, NEU_ACCENT);
}

void prkArrowDown(int x, int y, int len, uint16_t c) {
    neuFillRect(x, y, 2, len, c);
    for (int i = 0; i < 4; i++) neuFillRect(x - 3 + i, y + len + i - 2, 8 - 2 * i, 1, c);
}

void prkArrowLeft(int x, int y, int len, uint16_t c) {
    neuFillRect(x, y, len, 2, c);
    for (int i = 0; i < 4; i++) neuFillRect(x + i - 2, y - 3 + i, 1, 8 - 2 * i, c);
}

void prkArrowRight(int x, int y, int len, uint16_t c) {
    neuFillRect(x, y, len, 2, c);
    for (int i = 0; i < 4; i++) neuFillRect(x + len - i + 1, y - 3 + i, 1, 8 - 2 * i, c);
}

// ── SCR0 probe hub ───────────────────────────────────────────────────────────

static const char* const kProbeTypeLabels[PROBE_TYPE_COUNT] = { "Z Plate", "XYZ", "3D" };
static const int PRB_TYPE_Y = 38, PRB_TYPE_H = 36, PRB_FW = 106, PRB_FH = 40, PRB_ROUT_Y = 208;
static const int PRB_FIELDS[4][2] = { { 10, 96 }, { 124, 96 }, { 10, 144 }, { 124, 144 } };

static const NeuNavItem kHubNav[2] = { NAV_HOME, { PRK_NAV_CFG, "Configure", ICON_GEAR } };

struct ProbeRoutine { NeuRect r; const char* label; PendantScreen to; NeuIcon icon; NeuFont font; };

static int probeRoutines(ProbeRoutine* out) {
    const int n = probeRoutineCount();
    if (n == 1) {
        out[0] = { { 10, PRB_ROUT_Y, 220, 60 }, "Z Surface", PSCREEN_PROBE_Z, ICON_PROBE, NEU_B12 };
    } else if (n == 2) {
        out[0] = { { 10, PRB_ROUT_Y, 104, 60 }, "Z Surface", PSCREEN_PROBE_Z, ICON_NONE, NEU_B9 };
        out[1] = { { 126, PRB_ROUT_Y, 104, 60 }, "Corner", PSCREEN_PROBE_CORNER, ICON_NONE, NEU_B9 };
    } else {
        static const char* const    labels[4] = { "Z Surface", "Corner", "Bore", "Boss" };
        static const PendantScreen  dest[4]   = { PSCREEN_PROBE_Z, PSCREEN_PROBE_CORNER,
                                                  PSCREEN_PROBE_BORE, PSCREEN_PROBE_BOSS };
        for (int i = 0; i < 4; i++)
            out[i] = { { 10 + (i % 2) * 116, PRB_ROUT_Y + (i >> 1) * 34, 104, 28 }, labels[i], dest[i], ICON_NONE, NEU_B9 };
    }
    return n;
}

void enterProbe() {
    releasePanelSprites();
    pendantProbeV2.focusedField   = -1;
    pendantProbeV2.confirmActive  = false;
    pendantProbeV2.dialAccelCount = 0;
}

void exitProbe() {
    pendantProbeV2.focusedField = -1;
}

static void drawProbeTypeRow() {
    neuClear(10, PRB_TYPE_Y, 220, PRB_TYPE_H);
    neuSegmented(10, PRB_TYPE_Y, 220, PRB_TYPE_H, kProbeTypeLabels, PROBE_TYPE_COUNT, pendantProbeV2.probeTypeIdx);
}

// Repaint ONLY the shared-settings fields — the dial handler calls this per detent.
// Values are mm (the probe G-code runs in G21), so they are labelled mm in
// either unit mode.
void updateProbeSharedFields() {
    if (currentPendantScreen != PSCREEN_PROBE) return;
    const auto& p  = pendantProbeV2;
    const int   fo = p.focusedField;
    char v[4][12];
    snprintf(v[0], sizeof(v[0]), "%.0f", p.probeRate);
    snprintf(v[1], sizeof(v[1]), "%.0f", p.seekRate);
    snprintf(v[2], sizeof(v[2]), "%.1f", p.retractDist);
    snprintf(v[3], sizeof(v[3]), "%.1f", p.maxZTravel);
    static const char* const caps[4]  = { "Probe", "Seek", "Retract", "Max Z" };
    static const char* const units[4] = { "mm/m", "mm/m", "mm", "mm" };
    for (int i = 0; i < 4; i++) {
        NeuFieldStyle o;
        o.focus = fo == i;
        neuFieldRepaint(PRB_FIELDS[i][0], PRB_FIELDS[i][1], PRB_FW, PRB_FH, caps[i], v[i], units[i], o);
    }
}

void drawProbeScreen() {
    neuHeader("Probe");
    drawProbeTypeRow();
    neuCaption("Shared settings", 12, 84);
    for (int i = 0; i < 4; i++) neuInset(PRB_FIELDS[i][0], PRB_FIELDS[i][1], PRB_FW, PRB_FH, 8, 3);
    updateProbeSharedFields();
    neuCaption("Routines", 12, 196);
    ProbeRoutine rs[4];
    const int    n = probeRoutines(rs);
    for (int i = 0; i < n; i++) {
        NeuBtnStyle o;
        o.icon = rs[i].icon;
        o.font = rs[i].font;
        neuButton(rs[i].r.x, rs[i].r.y, rs[i].r.w, rs[i].r.h, rs[i].label, o);
    }
    neuNav(kHubNav, 2);
}

void updateProbeScreen() {
    // nothing to live-update on SCR0
}

void handleProbeTouch(int x, int y) {
    auto& p = pendantProbeV2;
    // Type segmented control — re-gates the routine list.
    const int t = neuSegHit(10, PRB_TYPE_Y, 220, PRB_TYPE_H, PROBE_TYPE_COUNT, x, y);
    if (t >= 0) {
        if (p.probeTypeIdx != t) {
            p.probeTypeIdx = t;
            p.focusedField = -1;
            drawProbeScreen();
        }
        return;
    }
    // Shared fields — tap to focus (the dial then adjusts it)
    for (int i = 0; i < 4; i++) {
        if (isTouchInBounds(x, y, PRB_FIELDS[i][0], PRB_FIELDS[i][1], PRB_FW, PRB_FH)) {
            p.focusedField = p.focusedField == i ? -1 : i;
            updateProbeSharedFields();
            return;
        }
    }
    // Routine buttons (only the ones the current type exposes) → routine screen
    ProbeRoutine rs[4];
    const int    n = probeRoutines(rs);
    for (int i = 0; i < n; i++) {
        if (isTouchInBounds(x, y, rs[i].r.x, rs[i].r.y, rs[i].r.w, rs[i].r.h)) {
            p.focusedField       = -1;
            currentPendantScreen = rs[i].to;
            return;
        }
    }
    switch (neuNavHit(kHubNav, 2, x, y)) {
    case NAV_ID_HOME:
        currentPendantScreen = PSCREEN_MAIN_MENU;
        return;
    case PRK_NAV_CFG:
        p.returnScreen       = PSCREEN_PROBE;
        currentPendantScreen = probeIs3D() ? PSCREEN_PROBE_CFG_3D : PSCREEN_PROBE_CFG_PLATE;
        return;
    }
    // Tap elsewhere releases the dial.
    if (p.focusedField >= 0) {
        p.focusedField = -1;
        updateProbeSharedFields();
    }
}
