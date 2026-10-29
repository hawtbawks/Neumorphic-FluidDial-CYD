/* screen_jog_homing.cpp — neumorphic layout (spec: simulator/js/screens/jog.js)
 *
 *   axis rows   y 38..      one row per axis: tap a row to jog that axis with the dial.
 *                           The selected row is pressed in; the others are raised.
 *                           Big number = work position, small grey = machine position.
 *   Step        segmented   0.01 / 0.1 / 1 / coarse.  The coarse slot cycles three states:
 *                           tap 1 select · tap 2 the dial picks 10/50/100 · tap 3 back to jogging
 *   Home        buttons     one per axis, plus All when there is room
 *   nav         Home | Feed (tap: dial sets jog feed) | Work
 */
#include "pendant_shared.h"
#include "neu_ui.h"
#include "screen_jog_homing.h"

// ===== Increment sets =====
struct IncrementSet {
    const char* labels[4];
    float       values[4];
};

// Slots 0-2 are fixed fine increments; slot 3 is the dial box, holding whichever
// coarse value pendantJog.coarseIdx selects.  Imperial mirrors metric one-for-one
// in real terms (.001"≈0.01mm … 4.0"≈100mm) using round inch numbers, so the row
// behaves identically in either unit.
static const char* kCoarseLabelsMm[JOG_COARSE_COUNT] = { "10", "50", "100" };
static const float kCoarseValuesMm[JOG_COARSE_COUNT] = { 10.0f, 50.0f, 100.0f };
static const char* kCoarseLabelsIn[JOG_COARSE_COUNT] = { ".5", "2.0", "4.0" };
static const float kCoarseValuesIn[JOG_COARSE_COUNT] = { 0.5f, 2.0f, 4.0f };

static IncrementSet currentIncrements() {
    IncrementSet s;
    int ci = constrain(pendantJog.coarseIdx, 0, JOG_COARSE_COUNT - 1);
    if (pendantMachine.inInches) {
        // fine imperial: .001 .010 .100  (≈ 0.025 / 0.25 / 2.5 mm)
        s.labels[0]=".001";  s.labels[1]=".010"; s.labels[2]=".100";
        s.values[0]=0.001f;  s.values[1]=0.010f; s.values[2]=0.100f;
        s.labels[3]=kCoarseLabelsIn[ci];
        s.values[3]=kCoarseValuesIn[ci];
    } else {
        // fine metric: 0.01 0.1 1
        s.labels[0]="0.01"; s.labels[1]="0.1"; s.labels[2]="1";
        s.values[0]=0.01f;  s.values[1]=0.1f;  s.values[2]=1.0f;
        s.labels[3]=kCoarseLabelsMm[ci];
        s.values[3]=kCoarseValuesMm[ci];
    }
    return s;
}

void jogApplyCoarseIncrement() {
    if (pendantJog.selectedIncrement != 3) return;
    IncrementSet incs = currentIncrements();
    pendantJog.increment = incs.values[3];
}

// ===== Layout: depends on the axis count =====
struct JogLayout {
    int  n, rowH, pitch;
    int  stepCapY, stepY, stepH;
    int  homeCapY, homeY, homeH;
    bool compact;
};

static JogLayout jogLayout() {
    JogLayout L;
    L.n       = constrain(pendantMachine.numAxes, 1, PENDANT_MAX_AXES);
    L.rowH    = L.n <= 3 ? 34 : L.n == 4 ? 28 : 20;
    L.pitch   = L.rowH + (L.n <= 4 ? 6 : 4);
    int rowsEnd = 38 + L.n * L.pitch - (L.pitch - L.rowH);
    L.compact  = L.n > 4;
    L.stepCapY = rowsEnd + 8;
    L.stepY    = L.compact ? rowsEnd + 18 : rowsEnd + 20;
    L.stepH    = 34;
    L.homeCapY = L.stepY + L.stepH + 8;
    L.homeY    = L.compact ? L.stepY + L.stepH + 10 : L.homeCapY + 12;
    L.homeH    = min(34, 270 - L.homeY);
    return L;
}

static int jogRowY(int i) { return 38 + i * jogLayout().pitch; }

// One Home button per axis, plus All when there is room for it.
static int jogHomeLayout(NeuSpan* out) {
    const int n = jogLayout().n, numHome = n < 4 ? n + 1 : n;
    const int gap = 8, w = (220 - gap * (numHome - 1)) / numHome;
    for (int i = 0; i < numHome; i++) {
        int x = 10 + i * (w + gap);
        out[i] = { x, i == numHome - 1 ? 230 - x : w };
    }
    return numHome;
}

static const char* homeLabel(int numAx, int i) { return (numAx < 4 && i == numAx) ? "All" : AXIS_NAMES[i]; }

enum { NAV_ID_SPEED = NAV_ID_FIRST, NAV_ID_WORK };
static char s_speedLabel[16];

static int jogNav(NeuNavItem* items) {
    snprintf(s_speedLabel, sizeof(s_speedLabel), "F %d",
             pendantMachine.inInches ? pendantJog.jogSpeedIn : pendantJog.jogSpeedMm);
    items[0] = NAV_HOME;
    items[1] = { NAV_ID_SPEED, s_speedLabel, ICON_GAUGE, 100, pendantJog.speedDialMode };
    items[2] = { NAV_ID_WORK, "Work" };
    return 3;
}

// Change key for the axis rows: positions plus which row is selected / homing.
static NeuDroSnap s_rowsSnap;
static int        s_rowsHoming = -2, s_rowsSel = -2;

// Axis to restore when the coarse dial box or the feed hands the encoder back to jogging.
static int incDialPrevAxis = 0;

// ===== Lifecycle =====

void enterJogHoming() {
    releasePanelSprites();

    // Re-apply the current increment in case units changed since the last visit
    {
        IncrementSet incs = currentIncrements();
        pendantJog.increment = incs.values[pendantJog.selectedIncrement];
    }

    // Ensure a valid axis is selected on entry — leave either dial mode if active
    if (pendantJog.speedDialMode || pendantJog.incDialMode || pendantJog.selectedAxis < 0) {
        pendantJog.speedDialMode = false;
        pendantJog.incDialMode   = false;
        pendantJog.selectedAxis  = 0;
    }

    // $110/$130-$133 are cached on connect — no UART query here.
}

void exitJogHoming() {
    releasePanelSprites();
}

// ===== Draw =====

static void redrawJogIncrementLabel();

void drawJogHomingScreen() {
    // Units may have changed under us: keep the live increment in step with the
    // labels about to be drawn, so the step you see is the step you get.
    pendantJog.increment = currentIncrements().values[pendantJog.selectedIncrement];
    neuHeader("Jog");
    redrawJogAxisButtons();
    redrawJogIncrementButtons();
    const JogLayout L = jogLayout();
    if (!L.compact) neuCaption("Home", 12, L.homeCapY);
    NeuSpan hl[PENDANT_MAX_AXES + 1];
    const int numHome = jogHomeLayout(hl);
    for (int i = 0; i < numHome; i++) {
        NeuBtnStyle o;
        o.color = i < L.n ? NEU_AX[i] : NEU_HOME;
        neuButton(hl[i].x, L.homeY, hl[i].w, L.homeH, homeLabel(L.n, i), o);
    }
    NeuNavItem nav[3];
    neuNav(nav, jogNav(nav));
}

// One axis row.  Selected = inset with the DIAL marker; otherwise raised with the
// machine position.  full = redraw the surface too (selection changed); otherwise
// repaint only the flat face, through the panel sprite so the numbers never flicker.
static void jogRow(int i, bool full, const NeuDroSnap& s) {
    const JogLayout L = jogLayout();
    const int     y    = jogRowY(i);
    const bool    on   = i == pendantJog.selectedAxis;
    const NeuFont font = L.rowH >= 28 ? NEU_B12 : NEU_B9;
    if (full) {
        neuClear(10, y, 220, L.rowH);
        if (on) neuInset(10, y, 220, L.rowH, 10, 3);
        else    neuRaised(10, y, 220, L.rowH, 10, 3);
    } else {
        const int fx = on ? 14 : 12, fy = y + (on ? 4 : 2), fw = on ? 211 : 216, fh = L.rowH - (on ? 6 : 4);
        neuPanelBegin(fx, fy, fw, fh);
        neuFillRect(fx, fy, fw, fh, on ? NEU_WELL : NEU_FACE);
    }
    char buf[16];
    const int base = neuCapBase(y, L.rowH, font);
    neuText(AXIS_NAMES[i], 22, base, font, NEU_AX[i]);
    snprintf(buf, sizeof(buf), "%.*f", s.inInches ? 4 : 3, s.wpos[i]);
    neuText(buf, 158, base, font, on ? NEU_TEXT : NEU_TEXT2, NEU_RIGHT);
    const int capY = y + (L.rowH - 8) / 2;
    if (pendantJog.homingAxis == i) {
        neuCaption("Homing", 218, capY, NEU_HOME, NEU_RIGHT);
    } else if (on) {
        neuCaption("Dial", 218, capY - (L.rowH >= 28 ? 4 : 0), NEU_ACCENT, NEU_RIGHT);
        if (L.rowH >= 28) neuFillRoundRect(196, capY + 7, 22, 3, 1, NEU_ACCENT);
    } else {
        snprintf(buf, sizeof(buf), "%.1f", s.mpos[i]);
        neuCaption(buf, 218, capY, NEU_TEXT3, NEU_RIGHT);
    }
    if (!full) neuPanelEnd();
}

// Called every 100 ms: repaints only the row contents, and only when a number changed.
void updateJogAxisDisplay() {
    if (currentPendantScreen != PSCREEN_JOG_HOMING) return;
    NeuDroSnap snap;
    if (!neuDroSnapshot(snap)) return;   // lock busy: try again next tick
    // The controller can report a different axis count or units while we're up
    // (first $G after connect).  Both change the whole layout, so redraw it all —
    // the simulator's bench does the same with repaint().
    if (snap.numAxes != s_rowsSnap.numAxes || snap.inInches != s_rowsSnap.inInches) {
        drawJogHomingScreen();
        return;
    }
    if (neuDroSame(snap, s_rowsSnap) && s_rowsHoming == pendantJog.homingAxis
        && s_rowsSel == pendantJog.selectedAxis) return;
    s_rowsSnap   = snap;
    s_rowsHoming = pendantJog.homingAxis;
    s_rowsSel    = pendantJog.selectedAxis;
    for (int i = 0; i < jogLayout().n; i++) jogRow(i, false, snap);
}

void redrawJogAxisButtons() {
    if (currentPendantScreen != PSCREEN_JOG_HOMING) return;
    NeuDroSnap snap;
    if (!neuDroSnapshot(snap)) snap = s_rowsSnap;   // lock busy: last known positions
    for (int i = 0; i < jogLayout().n; i++) jogRow(i, true, snap);
    s_rowsSnap   = snap;
    s_rowsHoming = pendantJog.homingAxis;
    s_rowsSel    = pendantJog.selectedAxis;
}

// Caption row above the step control: unit, plus what the dial is doing right now.
static void redrawJogIncrementLabel() {
    const int y = jogLayout().stepCapY;
    char st[24], cap[12];
    neuStatus(st, sizeof(st));
    neuFillRect(10, y - 1, 220, 10, NEU_BG);
    // A axis (rotary) → label the increments in degrees, not mm/in.
    snprintf(cap, sizeof(cap), "Step %s",
             pendantJog.selectedAxis == 3 ? "deg" : (pendantMachine.inInches ? "in" : "mm"));
    neuCaption(cap, 12, y);
    if (strncmp(st, "Alarm", 5) == 0)  neuCaption("Alarm: unlock to jog", 228, y, NEU_ALARM, NEU_RIGHT);
    else if (pendantJog.incDialMode)   neuCaption("Dial picks step", 228, y, NEU_ACCENT, NEU_RIGHT);
    else if (pendantJog.speedDialMode) neuCaption("Dial sets feed", 228, y, NEU_ACCENT, NEU_RIGHT);
}

void redrawJogIncrementButtons() {
    if (currentPendantScreen != PSCREEN_JOG_HOMING) return;
    const JogLayout    L    = jogLayout();
    const IncrementSet incs = currentIncrements();
    neuClear(10, L.stepY, 220, L.stepH);
    if (pendantJog.incDialMode) neuDrawRoundRect(7, L.stepY - 3, 226, L.stepH + 6, 13, NEU_ACCENT);
    neuSegmented(10, L.stepY, 220, L.stepH, incs.labels, 4, pendantJog.selectedIncrement);
    // coarse slot: three dots show which of 10/50/100 it holds
    NeuSpan seg[4];
    neuSegLayout(10, 220, 4, seg);
    const int cx = seg[3].x + seg[3].w / 2;
    for (int k = 0; k < JOG_COARSE_COUNT; k++)
        neuFillRect(cx - 7 + k * 6, L.stepY + L.stepH - 9, 3, 2, k == pendantJog.coarseIdx ? NEU_ACCENT : NEU_TEXT3);
    redrawJogIncrementLabel();
}

void redrawJogSpeedButton() {
    if (currentPendantScreen != PSCREEN_JOG_HOMING) return;
    NeuNavItem nav[3];
    NeuSpan    lay[3];
    neuNavLayout(nav, jogNav(nav), lay);
    neuNavButton(lay[1].x, lay[1].w, nav[1]);
    redrawJogIncrementLabel();
}

// ===== Touch handler =====

void handleJogHomingTouch(int x, int y) {
    const JogLayout L = jogLayout();
    const int numAx = L.n;

    // Home row
    if (y >= L.homeY - 4 && y <= L.homeY + L.homeH + 4) {
        NeuSpan hl[PENDANT_MAX_AXES + 1];
        const int numHome = jogHomeLayout(hl);
        for (int i = 0; i < numHome; i++) {
            if (x >= hl[i].x - 4 && x <= hl[i].x + hl[i].w + 4) {
                if (!pendantConnected) return;
                if (i == numAx) {
                    send_line("$H");
                    // Home-All: start on X; onDROChange() then walks the
                    // "Homing" marker through whichever axis is actively homing.
                    pendantJog.homingAxis = 0;
                } else {
                    char cmd[16];
                    snprintf(cmd, sizeof(cmd), "$H%s", AXIS_NAMES[i]);
                    send_line(cmd);
                    // Transient homingAxis, NOT selectedAxis — once homing
                    // finishes the marker goes and the jog selection is untouched.
                    pendantJog.homingAxis = i;
                }
                redrawJogAxisButtons();
                return;
            }
        }
    }

    // Axis rows — picking one also hands the dial back to jogging
    for (int i = 0; i < numAx; i++) {
        const int ry = jogRowY(i);
        if (y >= ry - 3 && y < ry + L.pitch - 3 && x >= 10 && x <= 230) {
            const bool leavingDial = pendantJog.incDialMode || pendantJog.speedDialMode;
            pendantJog.speedDialMode = false;
            pendantJog.incDialMode   = false;
            pendantJog.selectedAxis  = i;
            pendantJog.homingAxis    = -1;   // manual selection cancels the homing marker
            redrawJogAxisButtons();
            if (leavingDial) {
                redrawJogIncrementButtons();
                redrawJogSpeedButton();
            }
            redrawJogIncrementLabel();       // A↔X/Y/Z changes the unit (deg vs mm/in)
            return;
        }
    }

    // Step segmented control.  Slots 0-2 are plain picks.  Slot 3 is the coarse
    // dial box, which walks three states so a tap can never surprise you by
    // taking the encoder away mid-jog:
    //   tap 1 (not yet selected) — just SELECT the coarse value; you keep jogging
    //   tap 2 (already selected) — hand the encoder over to stepping 10/50/100
    //   tap 3 (adjusting)        — hand it back to jogging at the value just set,
    //                              restoring the axis you were on
    const int seg = neuSegHit(10, L.stepY, 220, L.stepH, 4, x, y);
    if (seg >= 0) {
        const bool wasSelected = pendantJog.selectedIncrement == seg;
        pendantJog.selectedIncrement = seg;
        pendantJog.increment = currentIncrements().values[seg];
        saveJogPrefs();
        if (seg == 3) {
            if (!wasSelected) {
                pendantJog.incDialMode = false;
            } else if (pendantJog.incDialMode) {
                pendantJog.incDialMode  = false;
                pendantJog.selectedAxis = constrain(incDialPrevAxis, 0, numAx - 1);
            } else {
                incDialPrevAxis = pendantJog.selectedAxis >= 0 ? pendantJog.selectedAxis : 0;
                pendantJog.incDialMode   = true;
                pendantJog.speedDialMode = false;
                pendantJog.selectedAxis  = -1;
            }
            redrawJogAxisButtons();
            redrawJogSpeedButton();
        } else if (pendantJog.incDialMode) {
            pendantJog.incDialMode  = false;   // leaving the dial box
            pendantJog.selectedAxis = constrain(incDialPrevAxis, 0, numAx - 1);
            redrawJogAxisButtons();
        }
        redrawJogIncrementButtons();
        return;
    }

    NeuNavItem nav[3];
    switch (neuNavHit(nav, jogNav(nav), x, y)) {
    case NAV_ID_HOME: currentPendantScreen = PSCREEN_MAIN_MENU;    return;
    case NAV_ID_WORK: currentPendantScreen = PSCREEN_PROBING_WORK; return;
    case NAV_ID_SPEED:
        // Tap toggles: first tap hands the dial to the feed, second tap gives it back.
        if (pendantJog.speedDialMode) {
            pendantJog.speedDialMode = false;
            pendantJog.selectedAxis  = constrain(incDialPrevAxis, 0, numAx - 1);
        } else {
            if (pendantJog.selectedAxis >= 0) incDialPrevAxis = pendantJog.selectedAxis;
            const bool leavingInc = pendantJog.incDialMode;
            pendantJog.speedDialMode = true;
            pendantJog.incDialMode   = false;
            pendantJog.selectedAxis  = -1;
            if (leavingInc) redrawJogIncrementButtons();
        }
        redrawJogAxisButtons();
        redrawJogSpeedButton();
        return;
    }
}
