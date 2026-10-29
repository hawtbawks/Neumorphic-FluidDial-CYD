/*
 * screen_probe_z.cpp  —  SCR1: Z Surface Probe, neumorphic layout
 * (spec: simulator/js/screens/probe_z.js, layout from the probe kit)
 *
 * Probes Z only, using the selected probe type (3D probe or touch plate).
 * G-code generated internally:
 *   G91 G21
 *   G38.2 Z-{maxZTravel} F{probeRate}
 *   G90
 *   G53 G0 Z#5063                ; back to the trigger point (kills decel overshoot)
 *   G10 L20 P{n} Z{plateThick}   ; for touch plate
 *   G10 L20 P{n} Z{ballDia/2}    ; for 3D probe (centre offset)
 *   G91
 *   G0 Z{retractDist}
 *   G90
 *
 * focusedField: 0 = Max Z travel   1 = Retract dist
 */

#include "pendant_shared.h"
#include "screen_probe.h"
#include "screen_probe_z.h"

static const NeuRect PZ_FIELDS[2] = { { PRK_RX, 70, PRK_RW, 46 }, { PRK_RX, 124, PRK_RW, 46 } };
static const NeuRect PZ_RESULT    = { PRK_RX, 178, PRK_RW, 72 };

void enterProbeZ() {
    releasePanelSprites();
    pendantProbeV2.focusedField   = 0;
    pendantProbeV2.confirmActive  = false;
    pendantProbeV2.dialAccelCount = 0;
}

void exitProbeZ() {
    pendantProbeV2.focusedField = -1;
}

// ── G-code execution ─────────────────────────────────────────────────────────

static void runProbeZ() {
    if (!pendantConnected) return;

    // Coordinate system P number: G54=1, G55=2, G56=3, G57=4
    int pNum = pendantProbing.selectedCoordIndex + 1;

    // Z offset at trigger: ball radius (3D probe) or plate thickness (either plate)
    float zOffset = probeIs3D() ? probeTipOffset3D()
                                : pendantProbeV2.plateThick;

    char buf[64];
    probeActivateWcs();          // zero into the system shown on screen
    send_line("G91 G21");
    // Two-pass: fast seek down to the surface, then slow re-probe for precision.
    probeSeekFine("Z", -pendantProbeV2.maxZTravel,
                  pendantProbeV2.seekRate, pendantProbeV2.probeRate);
    send_line("G90");
    probeReturnToTrigger("Z");   // undo the deceleration overshoot before zeroing
    snprintf(buf, sizeof(buf), "G10 L20 P%d Z%.3f", pNum, zOffset);
    send_line(buf);
    send_line("G91");
    snprintf(buf, sizeof(buf), "G0 Z%.3f", pendantProbeV2.retractDist);
    send_line(buf);
    send_line("G90");
}

// ── Draw ─────────────────────────────────────────────────────────────────────

// Redraws ONLY the Z-Surface settings fields — used by the full draw and the dial
// handler so a jog-dial edit doesn't do a full-screen redraw.  Whole mm (or the
// inch equivalent): the dial steps these in 1 mm.
void updateProbeZFields() {
    if (currentPendantScreen != PSCREEN_PROBE_Z) return;
    const int fo = pendantProbeV2.focusedField;
    char a[12], b[12];
    const char* ua = prkLen(pendantProbeV2.maxZTravel, 0, a, sizeof(a));
    const char* ub = prkLen(pendantProbeV2.retractDist, 0, b, sizeof(b));
    NeuFieldStyle o;
    o.focus = fo == 0;
    neuFieldRepaint(PZ_FIELDS[0].x, PZ_FIELDS[0].y, PRK_RW, 46, "Max travel", a, ua, o);
    o.focus = fo == 1;
    neuFieldRepaint(PZ_FIELDS[1].x, PZ_FIELDS[1].y, PRK_RW, 46, "Retract", b, ub, o);
}

void drawProbeZScreen() {
    static const char* const kSteps[3] = { "Fast seek -Z", "Slow probe", "Set Z0" };
    neuHeader("Z Surface");
    prkPosStrip(true);
    prkSequence(kSteps, 3);
    prkDiagramWell();
    // Side view: the work (and the plate on it), the probe tip on top, probe direction.
    neuFillRect(22, 228, 86, 14, NEU_TEXT3);
    if (!probeIs3D()) neuFillRect(40, 222, 50, 6, NEU_ACCENT);   // the plate
    prkProbeTool(65, probeIs3D() ? 228 : 222, 24);
    prkArrowDown(36, 186, 26, NEU_JOG);
    for (const NeuRect& f : PZ_FIELDS) neuInset(f.x, f.y, f.w, f.h, 8, 3);
    updateProbeZFields();
    prkResultCard(PZ_RESULT, "Z0");
    prkHint(probeIs3D() ? "Probe connected?" : "Plate clip connected?");
    neuNav(PROBE_ROUTINE_NAV, 3);
    if (pendantProbeV2.confirmActive) prkConfirm("Z Surface");
}

void updateProbeZScreen() {
    if (currentPendantScreen != PSCREEN_PROBE_Z || pendantProbeV2.confirmActive) return;
    prkPosStrip(false);
}

void handleProbeZTouch(int x, int y) {
    static const PrkTouch t = { "Z Surface", PZ_FIELDS, 2, &PZ_RESULT, "Z0",
                                runProbeZ, drawProbeZScreen, updateProbeZFields, nullptr };
    prkHandleTouch(x, y, t);
}
