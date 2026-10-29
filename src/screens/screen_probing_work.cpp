/* screen_probing_work.cpp (Work Area) — neumorphic layout (spec: simulator/js/screens/work.js)
 *   WCS         segmented  G54..G57 (selecting it activates it on the controller)
 *   DRO         y 84       work + machine
 *   Set zero    one button per axis + All   -> G10 L20 Pn
 *   Go to zero  XY0 / Z0, each behind a confirm dialog because they move the machine
 *   nav         Home | Jog
 */
#include "pendant_shared.h"
#include "neu_ui.h"
#include "screen_probing_work.h"

enum { NAV_ID_JOG = NAV_ID_FIRST };
static const NeuNavItem kNav[] = { NAV_HOME, { NAV_ID_JOG, "Jog", ICON_JOG } };
static const int        kNavN  = 2;

static const char* const WCS_NAMES[4] = { "G54", "G55", "G56", "G57" };
static const int WORK_WCS_Y = 38, WORK_WCS_H = 36, WORK_DRO_Y = 84;
static const int WORK_ZERO_Y = 176, WORK_ZERO_H = 38, WORK_GO_Y = 236, WORK_GO_H = 34;

// A go-to move awaiting confirmation.  While one is up the DRO stops repainting
// so it can't draw through the dialog.
enum WorkConfirm { CONFIRM_NONE, CONFIRM_XY, CONFIRM_Z };
static WorkConfirm    s_confirm = CONFIRM_NONE;
static NeuDialogRects s_dialog;

static NeuDroSnap s_droKey;
static bool       s_droValid = false;

void enterProbingWork() {
    releasePanelSprites();
    s_confirm = CONFIRM_NONE;
}

void exitProbingWork() {
    releasePanelSprites();
    s_confirm = CONFIRM_NONE;
}

// One Set-zero button per axis, plus All.
static int workZeroLayout(NeuSpan* out) {
    const int n = constrain(pendantMachine.numAxes, 0, PENDANT_MAX_AXES), count = n + 1, gap = 8;
    const int w = (220 - gap * (count - 1)) / count;
    for (int i = 0; i < count; i++) {
        int x = 10 + i * (w + gap);
        out[i] = { x, i == count - 1 ? 230 - x : w };
    }
    return count;
}

static void drawWorkConfirm() {
    const bool xy = s_confirm == CONFIRM_XY;
    char line1[32], line2[32];
    if (xy) {
        snprintf(line1, sizeof(line1), "Rapid move in %s", pendantProbing.selectedCoordSystem.c_str());
        strlcpy(line2, "Check Z is clear of clamps", sizeof(line2));
    } else {
        strlcpy(line1, "Lowers Z to the work surface", sizeof(line1));
        snprintf(line2, sizeof(line2), "in %s", pendantProbing.selectedCoordSystem.c_str());
    }
    const char* lines[2] = { line1, line2 };
    s_dialog = neuDialog(xy ? "Go to X0 Y0?" : "Go to Z0?", lines, 2, "Cancel", "Move", NEU_RUN);
}

void drawProbingWorkScreen() {
    neuHeader("Work Area");
    s_droValid = false;
    redrawWorkCoordButtons();
    neuInset(10, WORK_DRO_Y, 220, NEU_DRO_H, 12, 3);
    updateWorkAreaPos();
    neuCaption("Set zero here", 12, WORK_ZERO_Y - 12);
    NeuSpan   lay[PENDANT_MAX_AXES + 1];
    const int count = workZeroLayout(lay), n = count - 1;
    for (int i = 0; i < count; i++) {
        NeuBtnStyle o;
        o.color = i < n ? NEU_AX[i] : NEU_TEXT;
        neuButton(lay[i].x, WORK_ZERO_Y, lay[i].w, WORK_ZERO_H, i < n ? AXIS_NAMES[i] : "All", o);
    }
    neuCaption("Go to zero", 12, WORK_GO_Y - 12);
    NeuBtnStyle go;
    go.color = NEU_TEXT2;
    go.icon  = ICON_ORIGIN;
    neuButton(10, WORK_GO_Y, 106, WORK_GO_H, "XY0", go);
    go.icon = ICON_UP;
    neuButton(124, WORK_GO_Y, 106, WORK_GO_H, "Z0", go);
    neuNav(kNav, kNavN);
    if (s_confirm != CONFIRM_NONE) drawWorkConfirm();
}

void updateWorkMachinePos() { updateWorkAreaPos(); }

void updateWorkAreaPos() {
    if (currentPendantScreen != PSCREEN_PROBING_WORK || s_confirm != CONFIRM_NONE) return;
    NeuDroSnap snap;
    if (!neuDroSnapshot(snap)) return;   // lock busy: try again next tick
    if (s_droValid && neuDroSame(snap, s_droKey)) return;
    s_droKey   = snap;
    s_droValid = true;
    neuPanelBegin(10 + 4, WORK_DRO_Y + 4, 212, 61);   // everything neuDroLabels/Values fill
    neuDroLabels(10, WORK_DRO_Y, snap);
    neuDroValues(10, WORK_DRO_Y, snap);
    neuPanelEnd();
}

void redrawWorkCoordButtons() {
    neuClear(10, WORK_WCS_Y, 220, WORK_WCS_H);
    neuSegmented(10, WORK_WCS_Y, 220, WORK_WCS_H, WCS_NAMES, 4, pendantProbing.selectedCoordIndex);
}

void handleProbingWorkTouch(int x, int y) {
    if (s_confirm != CONFIRM_NONE) {
        if (neuDialogHit(s_dialog.ok, x, y)) {
            if (pendantConnected) send_line(s_confirm == CONFIRM_XY ? "G90 G0 X0 Y0" : "G90 G0 Z0");
            s_confirm = CONFIRM_NONE;
            drawProbingWorkScreen();
            suppressTouchBounce();   // the screen under the finger just changed
        } else if (neuDialogHit(s_dialog.cancel, x, y)) {
            s_confirm = CONFIRM_NONE;
            drawProbingWorkScreen();
            suppressTouchBounce();
        }
        return;
    }

    // Coordinate system — selecting one also activates it on the controller
    const int seg = neuSegHit(10, WORK_WCS_Y, 220, WORK_WCS_H, 4, x, y);
    if (seg >= 0) {
        pendantProbing.selectedCoordIndex  = seg;
        pendantProbing.selectedCoordSystem = WCS_NAMES[seg];
        redrawWorkCoordButtons();
        if (pendantConnected) send_line(WCS_NAMES[seg]);
        return;
    }

    // Set zero — P number matches the selected coord system (G54=P1 … G57=P4)
    if (y >= WORK_ZERO_Y - 4 && y <= WORK_ZERO_Y + WORK_ZERO_H + 4) {
        const int pNum = pendantProbing.selectedCoordIndex + 1;
        NeuSpan   lay[PENDANT_MAX_AXES + 1];
        const int count = workZeroLayout(lay), n = count - 1;
        for (int i = 0; i < count; i++) {
            if (x < lay[i].x - 4 || x > lay[i].x + lay[i].w + 4) continue;
            if (!pendantConnected) return;
            char cmd[48];
            int  len = snprintf(cmd, sizeof(cmd), "G10 L20 P%d", pNum);
            if (i < n) {
                snprintf(cmd + len, sizeof(cmd) - len, " %s0", AXIS_NAMES[i]);
            } else {
                for (int a = 0; a < n; a++) len += snprintf(cmd + len, sizeof(cmd) - len, " %s0", AXIS_NAMES[a]);
            }
            send_line(cmd);
            return;
        }
    }

    // Go to zero — moves the machine, so it asks first
    if (y >= WORK_GO_Y - 4 && y <= WORK_GO_Y + WORK_GO_H + 4 && pendantConnected) {
        s_confirm = x < 120 ? CONFIRM_XY : CONFIRM_Z;
        drawWorkConfirm();
        suppressTouchBounce();   // a held tap must not land on the dialog's buttons
        return;
    }

    switch (neuNavHit(kNav, kNavN, x, y)) {
    case NAV_ID_HOME: currentPendantScreen = PSCREEN_MAIN_MENU;  break;
    case NAV_ID_JOG:  currentPendantScreen = PSCREEN_JOG_HOMING; break;
    }
}
