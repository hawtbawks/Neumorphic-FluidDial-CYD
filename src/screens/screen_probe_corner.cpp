/*
 * screen_probe_corner.cpp  —  SCR2: XYZ Corner Probe, neumorphic layout
 * (spec: simulator/js/screens/probe_corner.js, layout from the probe kit)
 *   left: sequence, top-down diagram of the chosen corner, result card (tap = next WCS)
 *   right: 2x2 corner picker laid out like the corners, then depth / overshoot / XY retract
 *
 * Probes the X, Y and Z axes of a workpiece corner.
 * User pre-positions probe above the corner edge.
 *
 * Corner options (cornerIdx): 0=Bot-Left  1=Bot-Right  2=Top-Left  3=Top-Right
 *
 * focusedField: 0=cornerDepth  1=cornerOver  2=cornerRetXY
 *
 * G-code generated internally using G91 (relative) mode throughout.
 * Ball radius / plate-edge compensation applied to G10 L20.
 * Each axis returns to its G38.2 trigger point (G53 G0 to #5061/#5062/#5063)
 * before being zeroed, so the deceleration overshoot never enters the offset.
 */

#include "pendant_shared.h"
#include "screen_probe.h"
#include "screen_probe_corner.h"

void enterProbeCorner() {
    releasePanelSprites();
    pendantProbeV2.focusedField   = 0;
    pendantProbeV2.confirmActive  = false;
    pendantProbeV2.dialAccelCount = 0;
}

void exitProbeCorner() {}

// ── G-code execution ─────────────────────────────────────────────────────────

static void runProbeCorner() {
    if (!pendantConnected) return;

    int   pNum    = pendantProbing.selectedCoordIndex + 1;
    float rate    = pendantProbeV2.probeRate;   // fine (slow) re-probe feed
    float seekF   = pendantProbeV2.seekRate;    // fast seek feed
    float depth   = pendantProbeV2.cornerDepth;    // positive mm below surface
    float over    = pendantProbeV2.cornerOver;     // overshoot approach distance
    float retXY   = pendantProbeV2.cornerRetXY;
    float retZ    = pendantProbeV2.retractDist;
    float maxZ    = pendantProbeV2.maxZTravel;
    bool  is3D    = probeIs3D();
    float platZ   = is3D ? probeTipOffset3D() : pendantProbeV2.plateThick;
    // XY edge compensation: the contact point sits outside the true workpiece
    // edge by the ball radius (3D probe) or the XYZ plate's wall thickness on
    // each axis (plate).  Both shift the edge the same way in the approach
    // direction, so they use the same sign convention below.
    float edgeOfsX = is3D ? probeTipOffset3D() : pendantProbeV2.plateOffX;
    float edgeOfsY = is3D ? probeTipOffset3D() : pendantProbeV2.plateOffY;

    int   cIdx    = pendantProbeV2.cornerIdx;   // 0=BotL 1=BotR 2=TopL 3=TopR

    // Direction signs: X probe direction (+1 or -1), Y probe direction
    // Bot-Left:  probe toward +X (+1), toward +Y (+1) — we approach from outside (left/below)
    // Bot-Right: probe toward -X (-1), toward +Y (+1)
    // Top-Left:  probe toward +X (+1), toward -Y (-1)
    // Top-Right: probe toward -X (-1), toward -Y (-1)
    int xDir = (cIdx == 0 || cIdx == 2) ? +1 : -1;
    int yDir = (cIdx == 0 || cIdx == 1) ? +1 : -1;

    char buf[80];
    bool doXY = true;   // corner probe always does X/Y/Z
    bool doZ  = true;

    probeActivateWcs();          // zero into the system shown on screen

    // ── Z probe (surface, via plate) ─────────────────────────────────────
    if (doZ) {
        send_line("G91 G21");
        probeSeekFine("Z", -maxZ, seekF, rate);   // two-pass surface touch
        send_line("G90");
        probeReturnToTrigger("Z");   // undo the deceleration overshoot before zeroing
        snprintf(buf, sizeof(buf), "G10 L20 P%d Z%.3f", pNum, platZ);
        send_line(buf);
        send_line("G91");
        snprintf(buf, sizeof(buf), "G0 Z%.3f F1000", retZ);
        send_line(buf);
    }

    // ── XY probe ─────────────────────────────────────────────────────────
    if (doXY) {
        // Lower to XY probe depth (relative from current Z after retract)
        // Net movement: down by (depth + retZ) to reach depth below surface
        // We've already retracted retZ, so go down (depth + retZ) more if Z was probed,
        // or just depth if Z was not probed.
        float dropZ = doZ ? (depth + retZ) : depth;
        snprintf(buf, sizeof(buf), "G91");
        send_line(buf);
        snprintf(buf, sizeof(buf), "G0 Z-%.3f F500", dropZ);
        send_line(buf);

        // ── X probe ──────────────────────────────────────────────────────
        // Move overshoot in opposite direction to X wall (away from workpiece)
        snprintf(buf, sizeof(buf), "G0 X%.3f F1000", -xDir * over);
        send_line(buf);
        // Two-pass probe toward X wall
        probeSeekFine("X", xDir * (over + 20.0f), seekF, rate);
        // Set X WCS: when probing toward +X, edge is +edgeOfs past probe trigger
        //   G10 L20 gives: current machine pos = WCS value supplied
        //   We want WCS X=0 at the workpiece edge:
        //     probing +X: edge is edgeOfs ahead of trigger → supply -edgeOfs
        //     probing -X: edge is edgeOfs behind trigger   → supply +edgeOfs
        send_line("G90");
        probeReturnToTrigger("X");   // undo the deceleration overshoot before zeroing
        float xWCS = xDir > 0 ? -edgeOfsX : +edgeOfsX;
        snprintf(buf, sizeof(buf), "G10 L20 P%d X%.3f", pNum, xWCS);
        send_line(buf);
        // Retract from X wall
        send_line("G91");
        snprintf(buf, sizeof(buf), "G0 X%.3f F1000", -xDir * retXY);
        send_line(buf);

        // ── Y probe ──────────────────────────────────────────────────────
        // Move overshoot in opposite direction to Y wall
        snprintf(buf, sizeof(buf), "G0 Y%.3f F1000", -yDir * over);
        send_line(buf);
        // Two-pass probe toward Y wall
        probeSeekFine("Y", yDir * (over + 20.0f), seekF, rate);
        send_line("G90");
        probeReturnToTrigger("Y");   // undo the deceleration overshoot before zeroing
        float yWCS = yDir > 0 ? -edgeOfsY : +edgeOfsY;
        snprintf(buf, sizeof(buf), "G10 L20 P%d Y%.3f", pNum, yWCS);
        send_line(buf);
        send_line("G91");
        snprintf(buf, sizeof(buf), "G0 Y%.3f F1000", -yDir * retXY);
        send_line(buf);

        // Raise back to safe Z
        snprintf(buf, sizeof(buf), "G0 Z%.3f F500", depth);
        send_line(buf);
        send_line("G90");
    }
}

// ── Draw ─────────────────────────────────────────────────────────────────────

static const char* const cornerLabels[4] = { "Bot-Left", "Bot-Right", "Top-Left", "Top-Right" };

// 2x2 picker laid out like the corners themselves: top row TL TR, bottom row BL BR.
struct CornerCell { int idx; const char* label; };
static const CornerCell CORNER_GRID[4] = { { 2, "TL" }, { 3, "TR" }, { 0, "BL" }, { 1, "BR" } };   // drawing order

static const NeuRect PC_PICK      = { PRK_RX, 70, PRK_RW, 46 };
static const NeuRect PC_FIELDS[3] = { { PRK_RX, 124, PRK_RW, 38 }, { PRK_RX, 168, PRK_RW, 38 }, { PRK_RX, 212, PRK_RW, 38 } };
static const NeuRect PC_SEQ       = { 10, 70, 110, 72 };
static const NeuRect PC_DIAG      = { 10, 150, 110, 52 };
static const NeuRect PC_RESULT    = { 10, 210, 110, 40 };

static NeuRect cornerCell(int k) { return { PC_PICK.x + 5 + (k % 2) * 47, PC_PICK.y + 5 + (k >> 1) * 19, 45, 17 }; }

static void drawCornerPicker() {
    neuInset(PC_PICK.x, PC_PICK.y, PC_PICK.w, PC_PICK.h, 10, 3);
    for (int k = 0; k < 4; k++) {
        const NeuRect c  = cornerCell(k);
        const bool    on = CORNER_GRID[k].idx == pendantProbeV2.cornerIdx;
        if (on) neuRaised(c.x, c.y, c.w, c.h, 6, 1);
        neuSmall(CORNER_GRID[k].label, c.x + c.w / 2, c.y + 5, on ? NEU_ACCENT : NEU_TEXT3, NEU_CENTER);
    }
}

// Top-down view (screen up = +Y): stock, the chosen corner, and the probe
// approaching both edges.  Repainted through the panel sprite when the corner changes.
static void drawCornerDiagram() {
    const int  x = PC_DIAG.x, y = PC_DIAG.y, ci = pendantProbeV2.cornerIdx;
    const bool stockRight = ci == 0 || ci == 2;   // BL / TL: stock lies to +X of the corner
    const bool stockUp    = ci == 0 || ci == 1;   // BL / BR: stock lies to +Y (screen up)
    const int  bx = stockRight ? x + 52 : x + 12, by = stockUp ? y + 8 : y + 22;
    neuPanelBegin(x + 4, y + 4, PC_DIAG.w - 7, PC_DIAG.h - 7);
    neuFillRect(x + 4, y + 4, PC_DIAG.w - 7, PC_DIAG.h - 7, NEU_WELL);
    neuFillRect(bx, by, 46, 22, NEU_TEXT3);
    const int cx = stockRight ? bx : bx + 45, cy = stockUp ? by + 21 : by;
    // X approach along the stock's mid-height, from outside the corner side
    const int my = by + 10;
    if (stockRight) prkArrowRight(cx - 20, my, 14, NEU_JOG);
    else            prkArrowLeft(cx + 6, my, 14, NEU_JOG);
    // Y approach along the stock's mid-width
    const int mx = bx + 22;
    if (stockUp) {
        neuFillRect(mx, cy + 5, 2, 9, NEU_JOG);
        for (int i = 0; i < 4; i++) neuFillRect(mx - 3 + i, cy + 4 - i + 1, 8 - 2 * i, 1, NEU_JOG);
    } else {
        neuFillRect(mx, cy - 14, 2, 9, NEU_JOG);
        for (int i = 0; i < 4; i++) neuFillRect(mx - 3 + i, cy - 6 + i, 8 - 2 * i, 1, NEU_JOG);
    }
    neuFillCircle(cx, cy, 3, NEU_ACCENT);
    neuPanelEnd();
}

// Redraws ONLY the corner settings fields — used by the full draw and the dial
// handler so a jog-dial edit doesn't do a full-screen redraw.  1 decimal: the
// dial steps these in 0.1 mm.
void updateProbeCornerFields() {
    if (currentPendantScreen != PSCREEN_PROBE_CORNER) return;
    const auto& p  = pendantProbeV2;
    const char* caps[3] = { "Depth", "Overshoot", "XY retract" };
    const float vals[3] = { p.cornerDepth, p.cornerOver, p.cornerRetXY };
    for (int i = 0; i < 3; i++) {
        char v[12];
        const char*   u = prkLen(vals[i], 1, v, sizeof(v));
        NeuFieldStyle o;
        o.focus = p.focusedField == i;
        o.font  = NEU_B9;
        neuFieldRepaint(PC_FIELDS[i].x, PC_FIELDS[i].y, PRK_RW, 38, caps[i], v, u, o);
    }
}

static const char* confirmName() {
    static char buf[32];
    snprintf(buf, sizeof(buf), "XYZ Corner, %s", cornerLabels[pendantProbeV2.cornerIdx]);
    return buf;
}

void drawProbeCornerScreen() {
    static const char* const kSteps[3] = { "Touch top: Z0", "Probe X & Y", "Set X0 Y0" };
    neuHeader("XYZ Corner");
    prkPosStrip(true);
    prkSequence(kSteps, 3, PC_SEQ);
    prkDiagramWell(PC_DIAG);
    drawCornerDiagram();
    prkResultCard(PC_RESULT, "XYZ");
    drawCornerPicker();
    for (const NeuRect& f : PC_FIELDS) neuInset(f.x, f.y, f.w, f.h, 8, 3);
    updateProbeCornerFields();
    prkHint("Probe above the corner");
    neuNav(PROBE_ROUTINE_NAV, 3);
    if (pendantProbeV2.confirmActive) prkConfirm(confirmName());
}

void updateProbeCornerScreen() {
    if (currentPendantScreen != PSCREEN_PROBE_CORNER || pendantProbeV2.confirmActive) return;
    prkPosStrip(false);
}

// Corner picker: a tap on a cell picks that corner (and redraws the diagram).
static bool cornerPickerTouch(int x, int y) {
    if (!isTouchInBounds(x, y, PC_PICK.x, PC_PICK.y, PC_PICK.w, PC_PICK.h)) return false;
    for (int k = 0; k < 4; k++) {
        const NeuRect c = cornerCell(k);
        if (isTouchInBounds(x, y, c.x - 1, c.y - 1, c.w + 2, c.h + 2)) {
            pendantProbeV2.cornerIdx = CORNER_GRID[k].idx;
            drawCornerPicker();
            drawCornerDiagram();
            return true;
        }
    }
    return true;   // the well between cells does nothing
}

void handleProbeCornerTouch(int x, int y) {
    PrkTouch t = { confirmName(), PC_FIELDS, 3, &PC_RESULT, "XYZ",
                   runProbeCorner, drawProbeCornerScreen, updateProbeCornerFields, cornerPickerTouch };
    prkHandleTouch(x, y, t);
}
