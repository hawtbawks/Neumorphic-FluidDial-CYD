/*
 * screen_probe_cfg.cpp  —  SCR0a 3D probe setup / SCR0b touch plate setup, neumorphic layout
 * (spec: simulator/js/screens/probe_cfg.js)
 *
 * Plate setup (type comes from the probe hub):
 *   Z plate    one large thickness field + dimensioned side view
 *   XYZ plate  2x2 fields (thickness, width, X/Y wall offsets) + plan view
 * 3D probe setup:
 *   stylus     ball diameter + deflection fields
 *   deflection calibration: gauge width field + Calibrate (confirm, progress, result dialogs)
 * nav  Home | Back (to the hub, unsaved) | Save
 */

#include "pendant_shared.h"
#include "screen_probe.h"
#include "screen_probe_cfg.h"

static bool cfgNavTouch(int x, int y) {
    switch (neuNavHit(PROBE_CFG_NAV, 3, x, y)) {
    case NAV_ID_HOME:
        currentPendantScreen = PSCREEN_MAIN_MENU;
        return true;
    case NAV_ID_BACK:   // return to the hub without saving
        currentPendantScreen = PSCREEN_PROBE;
        return true;
    case PRK_NAV_SAVE:  // persist + return to the hub
        saveProbeSettings();
        currentPendantScreen = PSCREEN_PROBE;
        return true;
    }
    return false;
}

static bool cfgFieldTouch(const NeuRect* fields, int n, int x, int y, void (*redrawFields)()) {
    for (int i = 0; i < n; i++) {
        if (isTouchInBounds(x, y, fields[i].x, fields[i].y, fields[i].w, fields[i].h)) {
            pendantProbeV2.focusedField = pendantProbeV2.focusedField == i ? -1 : i;
            redrawFields();
            return true;
        }
    }
    return false;
}

// ══════════════════════════════════════════════════════════════════════════════
//  SCR0a — 3D PROBE SETUP
// ══════════════════════════════════════════════════════════════════════════════

// focusedField: 0 = ball dia   1 = deflection   2 = gauge width (deflection-cal reference)
static const NeuRect P3_FIELDS[3] = { { 10, 52, 106, 44 }, { 124, 52, 106, 44 }, { 10, 120, 106, 44 } };
static const NeuRect P3_CAL       = { 124, 120, 106, 44 };
static const NeuRect P3_DIAG      = { 10, 174, 220, 96 };

// calState: 0 idle, 4 confirm, 1 running, 2 result, 3 failed — the dialog shown.
static NeuDialogRects s_calDialog;

void enterProbeCfg3D() {
    releasePanelSprites();
    pendantProbeV2.focusedField   = 0;   // default focus on ball dia
    pendantProbeV2.confirmActive  = false;
    pendantProbeV2.dialAccelCount = 0;
    pendantProbeV2.calState       = 0;   // never resume a calibration across visits
    g_calCapture                  = false;
}

void exitProbeCfg3D() {
    g_calCapture = false;
    pendantProbeV2.focusedField = -1;
}

// Redraws ONLY the 3D-setup fields — used by the full draw and the dial handler
// so a jog-dial edit doesn't do a full-screen redraw.  Ball dia. steps in 0.1 mm
// → 1 decimal.  Deflection keeps 3: its dial step is 0.001 mm and the
// calibration routine writes a measured value, so those digits are real.
void updateProbeCfg3DFields() {
    if (currentPendantScreen != PSCREEN_PROBE_CFG_3D) return;
    const auto& p = pendantProbeV2;
    char v[3][12];
    snprintf(v[0], sizeof(v[0]), "%.1f", p.ballDia);
    snprintf(v[1], sizeof(v[1]), "%.3f", p.deflection);
    snprintf(v[2], sizeof(v[2]), "%.1f", p.calGaugeWidth);
    static const char* const caps[3] = { "Ball dia", "Deflection", "Gauge width" };
    for (int i = 0; i < 3; i++) {
        NeuFieldStyle o;
        o.focus = p.focusedField == i;
        o.font  = i == 1 ? NEU_B9 : NEU_B12;
        neuFieldRepaint(P3_FIELDS[i].x, P3_FIELDS[i].y, P3_FIELDS[i].w, P3_FIELDS[i].h, caps[i], v[i], "mm", o);
    }
}

static void drawProbe3DGraphic() {
    const int x = P3_DIAG.x, y = P3_DIAG.y, cx = x + P3_DIAG.w / 2;
    char dim[16];
    snprintf(dim, sizeof(dim), "%.1f mm", pendantProbeV2.ballDia);
    neuInset(x, y, P3_DIAG.w, P3_DIAG.h, 12, 3);
    neuFillRect(cx - 7, y + 10, 14, 8, NEU_TEXT3);              // collet / shank
    neuFillRoundRect(cx - 17, y + 18, 34, 18, 4, NEU_TEXT2);    // probe body
    neuFillRect(cx - 1, y + 36, 3, 30, NEU_TEXT2);              // stylus
    neuFillCircle(cx, y + 70, 5, NEU_ACCENT);                   // ball
    neuFillRect(cx - 70, y + 78, 140, 6, NEU_TEXT3);            // work surface
    neuFillRect(cx + 12, y + 70, 30, 1, NEU_TEXT3);             // dimension: ball diameter
    neuSmall(dim, cx + 46, y + 66, NEU_ACCENT);
}

void drawProbeCfg3DScreen() {
    neuHeader("Probe Setup");
    neuCaption("Stylus  3D touch probe", 12, 40);
    for (const NeuRect& f : P3_FIELDS) neuInset(f.x, f.y, f.w, f.h, 8, 3);
    neuCaption("Deflection calibration", 12, 108);
    updateProbeCfg3DFields();
    NeuBtnStyle cal;
    cal.color = NEU_ACCENT;
    neuButton(P3_CAL.x, P3_CAL.y, P3_CAL.w, P3_CAL.h, "Calibrate", cal);
    drawProbe3DGraphic();
    neuNav(PROBE_CFG_NAV, 3);

    // ── Calibration dialogs ──
    const auto& p = pendantProbeV2;
    if (p.calState == 4) {
        const char* l[] = { "The probe will move.", "Centre it over the gauge block." };
        s_calDialog = neuDialog("Calibrate?", l, 2, "Cancel", "Start", NEU_RUN);
    } else if (p.calState == 1) {
        const char* l[] = { "Probing both faces" };
        s_calDialog = neuDialog("Calibrating...", l, 1, nullptr, nullptr);
    } else if (p.calState == 2) {
        char title[20];
        snprintf(title, sizeof(title), "%.3f mm", p.calResult);
        const char* l[] = { "Measured deflection", "Apply it to the stylus?" };
        s_calDialog = neuDialog(title, l, 2, "Cancel", "Apply", NEU_RUN, NEU_RUN);
    } else if (p.calState == 3) {
        const char* l[] = { "Check the setup and the", "probe connection" };
        s_calDialog = neuDialog("Calibration failed", l, 2, nullptr, "OK", NEU_TEXT, NEU_ALARM);
    }
}

// ── Deflection-calibration probe program ─────────────────────────────────────
// Side-effect-free: probes the gauge top (for a safe lateral depth) then the two
// X faces from OUTSIDE (two-pass each) — never touches WCS (no G10).  The two
// face positions come back via show_probe()/g_calProbeXe4; updateProbeCfg3DScreen
// reads them.  Start with the probe roughly centred over the gauge, above it.
static void runProbeCalibration() {
    if (!pendantConnected) return;

    float W     = pendantProbeV2.calGaugeWidth;
    float seekF = pendantProbeV2.seekRate;
    float fineF = pendantProbeV2.probeRate;
    float maxZ  = pendantProbeV2.maxZTravel;
    float retZ  = pendantProbeV2.retractDist;
    const float CLEAR = 10.0f;                 // move this far past a face before lowering
    const float DEPTH = 5.0f;                  // probe this far below the gauge top
    float toClear = W / 2.0f + CLEAR;          // centre → clear position (past a face)
    float plunge  = retZ + DEPTH;              // top+retZ → top-DEPTH

    // Arm capture (show_probe records each [PRB:] machine X into g_calProbeXe4).
    g_calCount = 0; g_calAllOk = true; g_calCapture = true;

    char buf[80];
    send_line("G21 G90");
    send_line("#<sx> = #5420");                // save start (work coords) to return between faces
    send_line("#<sy> = #5421");

    // ── Z: find the gauge top, then lift to a safe height above it ──
    send_line("G91");
    snprintf(buf, sizeof(buf), "G38.2 Z-%.3f F%.0f", maxZ, fineF); send_line(buf);   // PRB[0] (Z)
    snprintf(buf, sizeof(buf), "G0 Z%.3f F500", retZ);            send_line(buf);    // to top+retZ
    send_line("G90");

    // ── LEFT face → x1: clear left (safe Z), lower beside, two-pass probe +X ──
    send_line("G91");
    snprintf(buf, sizeof(buf), "G0 X-%.3f F1000", toClear);       send_line(buf);
    snprintf(buf, sizeof(buf), "G0 Z-%.3f F500", plunge);         send_line(buf);
    snprintf(buf, sizeof(buf), "G38.2 X%.3f F%.0f", CLEAR + 3.0f, seekF); send_line(buf); // PRB[1] fast
    send_line("G0 X-1.5 F1000");
    snprintf(buf, sizeof(buf), "G38.2 X2.5 F%.0f", fineF);        send_line(buf);    // PRB[2] slow = x1
    snprintf(buf, sizeof(buf), "G0 Z%.3f F500", plunge);          send_line(buf);    // lift to top+retZ
    send_line("G90 G0 X#<sx> Y#<sy> F1000");                                          // return to centre

    // ── RIGHT face → x2: clear right, lower, two-pass probe -X ──
    send_line("G91");
    snprintf(buf, sizeof(buf), "G0 X%.3f F1000", toClear);        send_line(buf);
    snprintf(buf, sizeof(buf), "G0 Z-%.3f F500", plunge);         send_line(buf);
    snprintf(buf, sizeof(buf), "G38.2 X-%.3f F%.0f", CLEAR + 3.0f, seekF); send_line(buf); // PRB[3] fast
    send_line("G0 X1.5 F1000");
    snprintf(buf, sizeof(buf), "G38.2 X-2.5 F%.0f", fineF);       send_line(buf);    // PRB[4] slow = x2
    snprintf(buf, sizeof(buf), "G0 Z%.3f F500", plunge);          send_line(buf);
    send_line("G90 G0 X#<sx> Y#<sy> F1000");
}

// Poll the calibration flow: fired from the periodic update while calState==1.
void updateProbeCfg3DScreen() {
    if (currentPendantScreen != PSCREEN_PROBE_CFG_3D) return;
    if (pendantProbeV2.calState != 1) return;

    if (!g_calAllOk) {                                      // a probe missed contact
        g_calCapture = false;
        pendantProbeV2.calState = 3;
        drawProbeCfg3DScreen();
        return;
    }
    if (millis() - pendantProbeV2.calStartMs > 90000UL) {   // safety timeout
        g_calCapture = false;
        pendantProbeV2.calState = 3;
        drawProbeCfg3DScreen();
        return;
    }
    if (g_calCount >= 5) {                                  // Z + 2 two-pass faces done
        g_calCapture = false;
        int c = g_calCount;
        float x1u = g_calProbeXe4[c - 3] / 10000.0f;        // left slow re-probe
        float x2u = g_calProbeXe4[c - 1] / 10000.0f;        // right slow re-probe
        float x1  = pendantMachine.inInches ? x1u * 25.4f : x1u;   // → mm
        float x2  = pendantMachine.inInches ? x2u * 25.4f : x2u;
        // Outer span exceeds (width + ballDia) by 2×deflection.  Signed: a probe
        // that under-reads yields a negative deflection (handled by probeTipOffset3D).
        float defl = ((x2 - x1) - pendantProbeV2.calGaugeWidth - pendantProbeV2.ballDia) / 2.0f;
        pendantProbeV2.calResult = constrain(defl, -1.0f, 1.0f);
        pendantProbeV2.calState  = 2;                       // → result/confirm overlay
        drawProbeCfg3DScreen();
    }
}

void handleProbeCfg3DTouch(int x, int y) {
    auto& p = pendantProbeV2;
    // ── Calibration dialog is modal ──
    if (p.calState == 4) {                                  // confirm: Cancel | Start
        if (neuDialogHit(s_calDialog.cancel, x, y)) {
            p.calState = 0;
            drawProbeCfg3DScreen();
            suppressTouchBounce();
        } else if (neuDialogHit(s_calDialog.ok, x, y)) {
            p.calState   = 1;
            p.calStartMs = millis();
            drawProbeCfg3DScreen();       // show the Calibrating dialog first
            runProbeCalibration();
        }
        return;
    }
    if (p.calState == 2) {                                  // result: Cancel | Apply
        if (neuDialogHit(s_calDialog.cancel, x, y)) {
            p.calState = 0;
            drawProbeCfg3DScreen();
            suppressTouchBounce();
        } else if (neuDialogHit(s_calDialog.ok, x, y)) {
            p.deflection = p.calResult;
            saveProbeSettings();
            p.calState = 0;
            drawProbeCfg3DScreen();
            suppressTouchBounce();
        }
        return;
    }
    if (p.calState == 3) {                                  // failed: OK dismisses
        if (neuDialogHit(s_calDialog.ok, x, y)) {
            p.calState = 0;
            drawProbeCfg3DScreen();
            suppressTouchBounce();
        }
        return;
    }
    if (p.calState == 1) return;                            // busy — ignore taps

    if (cfgFieldTouch(P3_FIELDS, 3, x, y, updateProbeCfg3DFields)) return;

    // Calibrate → confirm first (motion safety)
    if (isTouchInBounds(x, y, P3_CAL.x, P3_CAL.y, P3_CAL.w, P3_CAL.h)) {
        p.calState = pendantConnected ? 4 : 3;
        drawProbeCfg3DScreen();
        suppressTouchBounce();   // a held tap must not land on the dialog
        return;
    }
    cfgNavTouch(x, y);
}

// ══════════════════════════════════════════════════════════════════════════════
//  SCR0b — TOUCH PLATE SETUP  (Z-Height plate OR XYZ plate)
// ══════════════════════════════════════════════════════════════════════════════

// focusedField: 0 = thickness  1 = width  2 = XY offset X  3 = XY offset Y
static bool plateIsXYZ() { return pendantProbeV2.probeTypeIdx == PROBE_TYPE_XYZPLATE; }

static int plateFields(NeuRect* out) {
    if (plateIsXYZ()) {
        out[0] = { 10, 52, 106, 42 };
        out[1] = { 124, 52, 106, 42 };
        out[2] = { 10, 102, 106, 42 };
        out[3] = { 124, 102, 106, 42 };
        return 4;
    }
    out[0] = { 10, 52, 220, 54 };
    return 1;
}

void enterProbeCfgPlate() {
    releasePanelSprites();
    pendantProbeV2.focusedField   = 0;   // thickness — valid for both plate types
    pendantProbeV2.confirmActive  = false;
    pendantProbeV2.dialAccelCount = 0;
}

void exitProbeCfgPlate() {
    pendantProbeV2.focusedField = -1;
}

// Redraws ONLY the plate fields (and, for the Z plate, the diagram's dimension
// label) — used by the full draw and the dial handler.  1 decimal on the XYZ
// plate (all four step in 0.1 mm); 2 on the single large thickness field.
void updateProbeCfgPlateFields() {
    if (currentPendantScreen != PSCREEN_PROBE_CFG_PLATE) return;
    const auto& p  = pendantProbeV2;
    const int   fo = p.focusedField;
    NeuRect     F[4];
    const int   n = plateFields(F);
    char        v[12];
    if (n == 1) {
        snprintf(v, sizeof(v), "%.2f", p.plateThick);
        NeuFieldStyle o;
        o.focus = fo == 0;
        o.font  = NEU_B18;
        neuFieldRepaint(F[0].x, F[0].y, F[0].w, F[0].h, "Plate thickness", v, "mm", o);
        neuPanelBegin(192, 231, 34, 10);                 // the diagram's dimension label
        neuFillRect(192, 231, 34, 10, NEU_WELL);
        neuSmall(v, 192, 232, NEU_ACCENT);
        neuPanelEnd();
        return;
    }
    static const char* const caps[4] = { "Thickness", "Width", "Wall X", "Wall Y" };
    const float vals[4] = { p.plateThick, p.plateWidth, p.plateOffX, p.plateOffY };
    for (int i = 0; i < 4; i++) {
        snprintf(v, sizeof(v), "%.1f", vals[i]);
        NeuFieldStyle o;
        o.focus = fo == i;
        neuFieldRepaint(F[i].x, F[i].y, F[i].w, F[i].h, caps[i], v, "mm", o);
    }
}

static void drawPlateDiagram() {
    const bool    xyz = plateIsXYZ();
    const NeuRect d   = xyz ? NeuRect{ 10, 154, 220, 116 } : NeuRect{ 10, 116, 220, 154 };
    neuInset(d.x, d.y, d.w, d.h, 12, 3);
    const int cx = d.x + 110, by = d.y + d.h - 24;
    if (xyz) {   // plan view: L-shaped plate sitting on the stock corner
        neuFillRect(cx - 30, by - 50, 90, 52, NEU_TEXT3);
        neuFillRect(cx - 42, by - 62, 80, 12, NEU_ACCENT);
        neuFillRect(cx - 42, by - 62, 12, 64, NEU_ACCENT);
        neuCaption("Plan view", d.x + 12, d.y + 10);
    } else {     // side view: plate on the stock, probe tip on the plate, thickness dimension
        char v[12];
        snprintf(v, sizeof(v), "%.2f", pendantProbeV2.plateThick);
        neuFillRect(cx - 80, by, 160, 18, NEU_TEXT3);
        neuFillRect(cx - 50, by - 20, 100, 20, NEU_ACCENT);
        prkProbeTool(cx, by - 20, 50);
        const int dx = cx + 66;
        neuFillRect(dx, by - 20, 1, 20, NEU_TEXT2);
        neuFillRect(dx - 4, by - 20, 9, 1, NEU_TEXT2);
        neuFillRect(dx - 4, by - 1, 9, 1, NEU_TEXT2);
        neuSmall(v, dx + 6, by - 14, NEU_ACCENT);
        neuCaption("Turn the dial to adjust", cx, d.y + 12, NEU_TEXT3, NEU_CENTER);
    }
}

void drawProbeCfgPlateScreen() {
    neuHeader("Plate Setup");
    neuCaption(plateIsXYZ() ? "XYZ touch plate" : "Z touch plate", 12, 40);
    NeuRect   F[4];
    const int n = plateFields(F);
    for (int i = 0; i < n; i++) neuInset(F[i].x, F[i].y, F[i].w, F[i].h, 8, 3);
    drawPlateDiagram();
    updateProbeCfgPlateFields();   // after the diagram: it repaints the diagram's label too
    neuNav(PROBE_CFG_NAV, 3);
}

void handleProbeCfgPlateTouch(int x, int y) {
    NeuRect   F[4];
    const int n = plateFields(F);
    if (cfgFieldTouch(F, n, x, y, updateProbeCfgPlateFields)) return;
    cfgNavTouch(x, y);
}
