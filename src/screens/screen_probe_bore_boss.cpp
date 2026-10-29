/*
 * screen_probe_bore_boss.cpp  —  SCR3a: Bore / SCR3b: Boss, neumorphic layout
 * (spec: simulator/js/screens/probe_bore_boss.js, layout from the probe kit)
 *
 * Both routines find the XY centre of a round feature by probing FOUR points
 * radially at 90° (±X, ±Y) and averaging the two opposed pairs: the centre is
 * simply the midpoint of the +X/−X touches (X) and the +Y/−Y touches (Y).  Each
 * point is a crash-safe two-pass move (fast seek + slow re-probe) approaching the
 * wall RADIALLY, so contact is perpendicular to the curve — no tangential skid.
 * Opposed pairs also cancel the probe tip radius / deflection per axis, so no
 * ball-radius compensation is needed, and the maths is division-free (unlike a
 * three-point circumcentre, which can divide by a near-zero triangle area).
 *
 * The X pair is probed first; the tool then RE-CENTRES in X before the Y pair, so
 * the +Y/−Y probes run through the true vertical diameter and contact each wall
 * head-on even if the operator started well off-centre.
 *
 * Coordinate frames: FluidNC returns G38 probe results (#5061/#5062) in MACHINE
 * coordinates, so the centre maths and the final move are done in machine coords
 * (G53).  The between-probe return uses the saved start (current-position params
 * #5420/#5421, work coords) via G90 — the same convention as the reference macro.
 *
 * BORE (inside circle):
 *   Operator places the probe tip INSIDE the bore at any comfortable depth.  No Z
 *   motion — Z work-zero is handled separately by the Z Surface probe.  Probe 4
 *   points outward, average the pairs, G53 move to the centre, G10 L20 sets X0 Y0.
 *
 * BOSS (outside circle):
 *   Operator starts above the boss centre.
 *   1. Touch the flat top and set Z0 there (the top is the natural Z datum).
 *   2. For each of 4 directions: move clear, plunge beside the boss, two-pass
 *      probe inward.
 *   3. Average the opposed pairs, G53 move to the centre, G10 L20 sets X0 Y0
 *      (Z0 already set).
 *
 * focusedField for bore:  0=boreDia  1=boreOffset
 * focusedField for boss:  0=bossDia  1=bossDepth  2=bossClear
 */

#include "pendant_shared.h"
#include "screen_probe.h"
#include "screen_probe_bore_boss.h"

// Two-pass probe along a unit direction (ux,uy): fast seek to contact, back off,
// slow re-probe.  Comes to rest just past the trigger (deceleration overshoot); the
// trigger point itself is in #5061/#5062, which is what we store.  G91.
static void probeRadial2Pass(float ux, float uy, float seekDist, float seekF, float fineF) {
    const float BACKOFF = 1.5f;
    char b[96];
    snprintf(b, sizeof(b), "G38.2 G91 X%.3f Y%.3f F%.0f", seekDist * ux, seekDist * uy, seekF); send_line(b);
    snprintf(b, sizeof(b), "G0 G91 X%.3f Y%.3f F1000",   -BACKOFF * ux, -BACKOFF * uy);         send_line(b);
    snprintf(b, sizeof(b), "G38.2 G91 X%.3f Y%.3f F%.0f", (BACKOFF + 1.0f) * ux, (BACKOFF + 1.0f) * uy, fineF); send_line(b);
}

// Two-pass probe along (ux,uy), then store the along-axis machine-coord result:
// axisX -> #5061 (X wall), else #5062 (Y wall).  Only the coordinate on the
// probe axis is needed for that pair's midpoint.
static void probeWallStore(float ux, float uy, float seek, float seekF, float fineF,
                           const char* storeVar, bool axisX) {
    probeRadial2Pass(ux, uy, seek, seekF, fineF);
    char b[64];
    snprintf(b, sizeof(b), "%s = #%s", storeVar, axisX ? "5061" : "5062");
    send_line(b);
}

// Move to the found centre (machine #<xc>,#<yc>) and zero X/Y there.
static void emitMoveCentreZero(int pNum) {
    send_line("G53 G0 X#<xc> Y#<yc>");
    char b[64];
    snprintf(b, sizeof(b), "G10 L20 P%d X0 Y0", pNum);
    send_line(b);
}

// ══════════════════════════════════════════════════════════════════════════════
//  SCR3a — BORE PROBE
// ══════════════════════════════════════════════════════════════════════════════

static const NeuRect PB_FIELDS[2] = { { PRK_RX, 70, PRK_RW, 46 }, { PRK_RX, 124, PRK_RW, 46 } };
static const NeuRect PB_RESULT    = { PRK_RX, 178, PRK_RW, 72 };

void enterProbeBore() {
    releasePanelSprites();
    pendantProbeV2.focusedField   = 0;
    pendantProbeV2.confirmActive  = false;
    pendantProbeV2.dialAccelCount = 0;
}

void exitProbeBore() {}

// ── G-code: bore centre finding (4-point radial) ─────────────────────────────
// No Z motion — the tip is pre-placed inside the bore.  Probe the ±X pair, re-
// centre X, then probe the ±Y pair through that centred X (true vertical
// diameter), average each pair, G53-move to the centre and set X0/Y0.

static void runProbeBore() {
    if (!pendantConnected) return;

    int   pNum   = pendantProbing.selectedCoordIndex + 1;
    float seekF  = pendantProbeV2.seekRate;
    float fineF  = pendantProbeV2.probeRate;
    float rad    = pendantProbeV2.boreDia / 2.0f;
    float wallOf = pendantProbeV2.boreOffset;
    // Generous outward seek — a diameter + margin covers an off-centre start
    // (the G38.2 stops on contact, so over-estimating is safe).
    float d = 2.0f * rad + wallOf + 3.0f;

    probeActivateWcs();          // zero into the system shown on screen
    send_line("G21 G90");
    send_line("#<sx> = #5420");   // save start (work coords) to return between probes
    send_line("#<sy> = #5421");

    // ── X pair: probe +X and −X along Y = start, average to the centre X ──
    probeWallStore( 1.0f, 0.0f, d, seekF, fineF, "#<ax>", true);
    send_line("G90 G0 X#<sx> Y#<sy> F1000");                 // back to start
    probeWallStore(-1.0f, 0.0f, d, seekF, fineF, "#<cx>", true);
    send_line("G90 G0 X#<sx> Y#<sy> F1000");                 // back to start
    send_line("#<xc> = [[#<ax> + #<cx>] / 2]");              // machine X of centre
    send_line("G53 G0 X#<xc>");                              // re-centre X (Y stays at start)

    // ── Y pair: probe +Y and −Y through the centred X (true vertical diameter) ──
    probeWallStore(0.0f,  1.0f, d, seekF, fineF, "#<by>", false);
    send_line("G90 G0 Y#<sy> F1000");                        // Y back to start (X stays centred)
    probeWallStore(0.0f, -1.0f, d, seekF, fineF, "#<dy>", false);
    send_line("#<yc> = [[#<by> + #<dy>] / 2]");              // machine Y of centre

    emitMoveCentreZero(pNum);
}

// ── Draw (bore) ──────────────────────────────────────────────────────────────

// Top-down: the bore in the stock, centre mark, outward probe moves.
static void drawBoreDiagram() {
    const int x = PRK_DIAG.x, y = PRK_DIAG.y, cx = x + 55, cy = y + 38;
    neuFillRect(x + 14, y + 12, 82, 52, NEU_TEXT3);
    neuFillCircle(cx, cy, 20, NEU_WELL);
    neuDrawCircle(cx, cy, 20, NEU_TEXT2);
    prkArrowLeft(cx - 17, cy, 14, NEU_JOG);
    prkArrowRight(cx + 3, cy, 14, NEU_JOG);
    neuFillCircle(cx, cy, 3, NEU_ACCENT);
}

// Redraws ONLY the bore settings fields — used by the full draw and by the dial
// handler so a jog-dial edit doesn't trigger a full-screen redraw.
void updateProbeBoreFields() {
    if (currentPendantScreen != PSCREEN_PROBE_BORE) return;
    const int fo = pendantProbeV2.focusedField;
    char a[12], b[12];
    const char* ua = prkLen(pendantProbeV2.boreDia, 1, a, sizeof(a));
    const char* ub = prkLen(pendantProbeV2.boreOffset, 1, b, sizeof(b));
    NeuFieldStyle o;
    o.focus = fo == 0;
    neuFieldRepaint(PB_FIELDS[0].x, PB_FIELDS[0].y, PRK_RW, 46, "Nominal dia", a, ua, o);
    o.focus = fo == 1;
    neuFieldRepaint(PB_FIELDS[1].x, PB_FIELDS[1].y, PRK_RW, 46, "Wall offset", b, ub, o);
}

static const char* const kBoreName = "Bore centre (Z unchanged)";

void drawProbeBoreScreen() {
    static const char* const kSteps[3] = { "Probe 4 walls", "Find centre", "Set X0 Y0" };
    neuHeader("Bore");
    prkPosStrip(true);
    prkSequence(kSteps, 3);
    prkDiagramWell();
    drawBoreDiagram();
    for (const NeuRect& f : PB_FIELDS) neuInset(f.x, f.y, f.w, f.h, 8, 3);
    updateProbeBoreFields();
    prkResultCard(PB_RESULT, "X0 Y0");
    prkHint("Place the tip inside the bore");
    neuNav(PROBE_ROUTINE_NAV, 3);
    if (pendantProbeV2.confirmActive) prkConfirm(kBoreName);
}

void updateProbeBoreScreen() {
    if (currentPendantScreen != PSCREEN_PROBE_BORE || pendantProbeV2.confirmActive) return;
    prkPosStrip(false);
}

void handleProbeBoreTouch(int x, int y) {
    static const PrkTouch t = { kBoreName, PB_FIELDS, 2, &PB_RESULT, "X0 Y0",
                                runProbeBore, drawProbeBoreScreen, updateProbeBoreFields, nullptr };
    prkHandleTouch(x, y, t);
}

// ══════════════════════════════════════════════════════════════════════════════
//  SCR3b — BOSS PROBE
// ══════════════════════════════════════════════════════════════════════════════

void enterProbeBoss() {
    releasePanelSprites();
    pendantProbeV2.focusedField   = 0;
    pendantProbeV2.confirmActive  = false;
    pendantProbeV2.dialAccelCount = 0;
}

void exitProbeBoss() {}

// One boss wall: move clear along (ux,uy) at safe Z, plunge beside the boss,
// two-pass probe INWARD (−ux,−uy), store the along-axis result, then lift back
// to safe Z.  Leaves XY at the probed wall for the caller to return home.
static void bossWallStore(float ux, float uy, float out, float inSeek, float plunge,
                          float seekF, float fineF, const char* storeVar, bool axisX) {
    char b[80];
    snprintf(b, sizeof(b), "G0 G91 X%.3f Y%.3f F1000", out * ux, out * uy); send_line(b);
    snprintf(b, sizeof(b), "G0 G91 Z-%.3f F500", plunge);                   send_line(b);
    probeWallStore(-ux, -uy, inSeek, seekF, fineF, storeVar, axisX);        // inward
    snprintf(b, sizeof(b), "G0 G91 Z%.3f F500", plunge);                    send_line(b);  // lift
}

// ── G-code: boss centre finding (4-point radial) ─────────────────────────────
// Starts above the boss centre.  Touches the flat top and sets Z0 there.  Probes
// the ±X pair, re-centres X, then probes the ±Y pair through that centred X;
// averages each pair, G53-moves to the centre and sets X0/Y0.  Works for a
// circular boss (one diameter) or a rectangular boss (independent X/Y sizes) —
// the only difference is that the per-axis clear/seek distances are sized from
// half the X size for the X pair and half the Y size for the Y pair.  The
// opposed-pair midpoint gives the true centre for either shape.

static void runProbeBoss() {
    if (!pendantConnected) return;

    int   pNum  = pendantProbing.selectedCoordIndex + 1;
    float seekF = pendantProbeV2.seekRate;
    float fineF = pendantProbeV2.probeRate;
    float depth = pendantProbeV2.bossDepth;    // mm below boss top to probe at
    float clear = pendantProbeV2.bossClear;
    float retZ  = pendantProbeV2.retractDist;
    float maxZ  = pendantProbeV2.maxZTravel;
    float platZ = probeIs3D() ? probeTipOffset3D()
                              : pendantProbeV2.plateThick;

    // Half-size per axis.  Circular: both halves = bossDia/2.  Rectangular: X uses
    // bossDia/2, Y uses bossSizeY/2.
    float radX = pendantProbeV2.bossDia / 2.0f;
    float radY = (pendantProbeV2.bossRect ? pendantProbeV2.bossSizeY : pendantProbeV2.bossDia) / 2.0f;

    float outX    = radX + clear;           // clear the boss on X before plunging
    float inSeekX = clear + radX + 5.0f;    // inward seek travel toward an X wall
    float outY    = radY + clear;
    float inSeekY = clear + radY + 5.0f;
    float plunge  = depth + retZ;           // from (top+retZ) down to (top-depth)

    char buf[80];
    probeActivateWcs();          // zero into the system shown on screen
    send_line("G21 G90");
    send_line("#<sx> = #5420");   // save start (work coords)
    send_line("#<sy> = #5421");

    // Touch the top and set Z0 there (the boss top is the Z datum), then lift.
    send_line("G91");
    snprintf(buf, sizeof(buf), "G38.2 Z-%.3f F%.0f", maxZ, fineF);  send_line(buf);
    send_line("G90");
    probeReturnToTrigger("Z");   // undo the deceleration overshoot before zeroing
    snprintf(buf, sizeof(buf), "G10 L20 P%d Z%.3f", pNum, platZ);   send_line(buf);
    send_line("G91");
    snprintf(buf, sizeof(buf), "G0 Z%.3f F500", retZ);             send_line(buf);  // to top+retZ
    send_line("G90");

    // Each wall: move clear (at safe Z), plunge beside the boss, two-pass probe
    // inward, lift straight up, return home.
    // ── X pair along Y = start ──
    bossWallStore( 1.0f, 0.0f, outX, inSeekX, plunge, seekF, fineF, "#<ax>", true);
    send_line("G90 G0 X#<sx> Y#<sy> F1000");                 // back to start
    bossWallStore(-1.0f, 0.0f, outX, inSeekX, plunge, seekF, fineF, "#<cx>", true);
    send_line("G90 G0 X#<sx> Y#<sy> F1000");                 // back to start
    send_line("#<xc> = [[#<ax> + #<cx>] / 2]");              // machine X of centre
    send_line("G53 G0 X#<xc>");                              // re-centre X at safe Z (Y at start)

    // ── Y pair through the centred X ──
    bossWallStore(0.0f,  1.0f, outY, inSeekY, plunge, seekF, fineF, "#<by>", false);
    send_line("G90 G0 Y#<sy> F1000");                        // Y back to start (X stays centred)
    bossWallStore(0.0f, -1.0f, outY, inSeekY, plunge, seekF, fineF, "#<dy>", false);
    send_line("#<yc> = [[#<by> + #<dy>] / 2]");              // machine Y of centre

    emitMoveCentreZero(pNum);   // sets X0 Y0; Z0 already set at the top
}

// ── Draw (boss) ──────────────────────────────────────────────────────────────

static const NeuRect PS_SEQ    = { 10, 70, 110, 72 };
static const NeuRect PS_DIAG   = { 10, 150, 110, 52 };
static const NeuRect PS_RESULT = { 10, 210, 110, 40 };
static const NeuRect PS_SHAPE  = { PRK_RX, 70, PRK_RW, 32 };

// Rectangular mode splits the nominal size into X / Y size: one extra field.
static int bossFields(NeuRect* out) {
    if (pendantProbeV2.bossRect) {
        for (int i = 0; i < 4; i++) out[i] = { PRK_RX, 110 + i * 36, PRK_RW, 34 };
        return 4;
    }
    for (int i = 0; i < 3; i++) out[i] = { PRK_RX, 110 + i * 47, PRK_RW, 42 };
    return 3;
}

// Top-down: round or rectangular boss with inward probe moves.
static void drawBossDiagram() {
    const int cx = PS_DIAG.x + 55, cy = PS_DIAG.y + 26;
    if (pendantProbeV2.bossRect) neuFillRect(cx - 18, cy - 12, 37, 25, NEU_TEXT3);
    else                         neuFillCircle(cx, cy, 14, NEU_TEXT3);
    prkArrowRight(cx - 40, cy, 14, NEU_JOG);
    prkArrowLeft(cx + 26, cy, 14, NEU_JOG);
    neuFillCircle(cx, cy, 3, NEU_ACCENT);
}

// Shape picker: two segments drawn as a circle and a square (no room for words at this width).
static void drawBossShapePicker() {
    static const char* const kBlank[2] = { "", "" };
    const int sel = pendantProbeV2.bossRect ? 1 : 0;
    neuClear(PS_SHAPE.x, PS_SHAPE.y, PS_SHAPE.w, PS_SHAPE.h);
    neuSegmented(PS_SHAPE.x, PS_SHAPE.y, PS_SHAPE.w, PS_SHAPE.h, kBlank, 2, sel, 8);
    NeuSpan segs[2];
    neuSegLayout(PS_SHAPE.x, PS_SHAPE.w, 2, segs);
    const int cy = PS_SHAPE.y + PS_SHAPE.h / 2;
    const int c0 = segs[0].x + segs[0].w / 2, c1 = segs[1].x + segs[1].w / 2;
    const uint16_t col0 = sel == 0 ? NEU_ACCENT : NEU_TEXT3, col1 = sel == 1 ? NEU_ACCENT : NEU_TEXT3;
    neuDrawCircle(c0, cy, 7, col0);
    neuDrawCircle(c0, cy, 6, col0);
    neuDrawRoundRect(c1 - 7, cy - 6, 15, 13, 0, col1);
    neuDrawRoundRect(c1 - 6, cy - 5, 13, 11, 0, col1);
}

// Redraws ONLY the boss settings fields — called by the full draw and, crucially,
// by the dial handler so turning the jog dial updates the focused value without a
// flicker-inducing full-screen redraw.
void updateProbeBossFields() {
    if (currentPendantScreen != PSCREEN_PROBE_BOSS) return;
    const auto& p = pendantProbeV2;
    NeuRect     F[4];
    const int   n = bossFields(F);
    const char* capsRect[4]  = { "X size", "Y size", "Depth", "Clearance" };
    const float valsRect[4]  = { p.bossDia, p.bossSizeY, p.bossDepth, p.bossClear };
    const char* capsRound[3] = { "Boss dia", "Depth", "Clearance" };
    const float valsRound[3] = { p.bossDia, p.bossDepth, p.bossClear };
    for (int i = 0; i < n; i++) {
        char v[12];
        const char*   u = prkLen(p.bossRect ? valsRect[i] : valsRound[i], 1, v, sizeof(v));
        NeuFieldStyle o;
        o.focus = p.focusedField == i;
        o.font  = NEU_B9;
        neuFieldRepaint(F[i].x, F[i].y, PRK_RW, F[i].h, p.bossRect ? capsRect[i] : capsRound[i], v, u, o);
    }
}

static const char* bossName() { return pendantProbeV2.bossRect ? "Rectangular boss" : "Round boss"; }

void drawProbeBossScreen() {
    static const char* const kSteps[3] = { "Touch top: Z0", "Probe 4 sides", "Set X0 Y0" };
    neuHeader("Boss");
    prkPosStrip(true);
    prkSequence(kSteps, 3, PS_SEQ);
    prkDiagramWell(PS_DIAG);
    drawBossDiagram();
    prkResultCard(PS_RESULT, "XYZ");
    drawBossShapePicker();
    NeuRect   F[4];
    const int n = bossFields(F);
    for (int i = 0; i < n; i++) neuInset(F[i].x, F[i].y, F[i].w, F[i].h, 8, 3);
    updateProbeBossFields();
    prkHint("Start above the boss centre");
    neuNav(PROBE_ROUTINE_NAV, 3);
    if (pendantProbeV2.confirmActive) prkConfirm(bossName());
}

void updateProbeBossScreen() {
    if (currentPendantScreen != PSCREEN_PROBE_BOSS || pendantProbeV2.confirmActive) return;
    prkPosStrip(false);
}

// Shape picker: round <-> rectangular.  The field set changes, so a full redraw.
static bool bossShapeTouch(int x, int y) {
    const int seg = neuSegHit(PS_SHAPE.x, PS_SHAPE.y, PS_SHAPE.w, PS_SHAPE.h, 2, x, y);
    if (seg < 0) return false;
    if ((seg == 1) == pendantProbeV2.bossRect) return true;   // already that shape
    pendantProbeV2.bossRect = seg == 1;
    // Clamp focus to the field count of the new mode (rect=4, round=3).
    if (pendantProbeV2.focusedField > (pendantProbeV2.bossRect ? 3 : 2)) pendantProbeV2.focusedField = -1;
    saveProbeSettings();
    drawProbeBossScreen();
    return true;
}

void handleProbeBossTouch(int x, int y) {
    NeuRect   F[4];
    const int n = bossFields(F);
    PrkTouch  t = { bossName(), F, n, &PS_RESULT, "XYZ",
                    runProbeBoss, drawProbeBossScreen, updateProbeBossFields, bossShapeTouch };
    prkHandleTouch(x, y, t);
}
