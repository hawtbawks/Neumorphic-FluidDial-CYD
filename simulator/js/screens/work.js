/* screen_probing_work.cpp (Work Area) — neumorphic layout
 *   WCS         segmented  G54..G57 (selecting it activates it on the controller)
 *   DRO         y 84       work + machine
 *   Set zero    one button per axis + All   -> G10 L20 Pn
 *   Go to zero  XY0 / Z0, each behind a confirm dialog because they move the machine
 *   nav         Home | Jog
 */

const WORK_NAV = [NAV_HOME, { id: "jog", label: "Jog", icon: "jog" }];
const WCS_NAMES = ["G54", "G55", "G56", "G57"];
const WORK_WCS_Y = 38, WORK_WCS_H = 36, WORK_DRO_Y = 84, WORK_ZERO_Y = 176, WORK_ZERO_H = 38, WORK_GO_Y = 236, WORK_GO_H = 34;
let _workKey = "";
let _workConfirm = null;      // null, "xy" or "z": a go-to move awaiting confirmation
let _workDialog = null;

function enterProbingWork() { releasePanelSprites(); _workConfirm = null; }
function exitProbingWork() { releasePanelSprites(); _workConfirm = null; }

function workZeroLayout() {
  const n = pendantMachine.numAxes, count = n + 1, gap = 8;
  const w = ((220 - gap * (count - 1)) / count) | 0, out = [];
  for (let i = 0; i < count; i++) out.push({ x: 10 + i * (w + gap), w: i === count - 1 ? 230 - (10 + i * (w + gap)) : w });
  return out;
}

function drawProbingWorkScreen() {
  neuHeader("Work Area");
  _workKey = "";
  redrawWorkCoordButtons();
  neuInset(10, WORK_DRO_Y, 220, NEU_DRO_H, 12, 3);
  updateWorkAreaPos();
  neuCaption("Set zero here", 12, WORK_ZERO_Y - 12);
  workZeroLayout().forEach((b, i) => {
    const n = pendantMachine.numAxes;
    neuButton(b.x, WORK_ZERO_Y, b.w, WORK_ZERO_H, i < n ? AXIS_NAMES[i] : "All", { color: i < n ? NEU_AX[i] : NEU_TEXT });
  });
  neuCaption("Go to zero", 12, WORK_GO_Y - 12);
  neuButton(10, WORK_GO_Y, 106, WORK_GO_H, "XY0", { icon: NEU_ICON.origin, color: NEU_TEXT2 });
  neuButton(124, WORK_GO_Y, 106, WORK_GO_H, "Z0", { icon: NEU_ICON.up, color: NEU_TEXT2 });
  neuNav(WORK_NAV);
  if (_workConfirm) _drawWorkConfirm();
}

function _drawWorkConfirm() {
  const xy = _workConfirm === "xy";
  _workDialog = neuDialog(xy ? "Go to X0 Y0?" : "Go to Z0?",
    [xy ? "Rapid move in " + pendantProbing.selectedCoordSystem : "Lowers Z to the work surface",
     xy ? "Check Z is clear of clamps" : "in " + pendantProbing.selectedCoordSystem],
    "Cancel", "Move", NEU_RUN);
}

function updateWorkMachinePos() { updateWorkAreaPos(); }
function updateWorkAreaPos() {
  if (currentPendantScreen !== PSCREEN_PROBING_WORK || _workConfirm) return;
  const key = neuDroKey();
  if (key === _workKey) return;
  _workKey = key;
  neuDroLabels(10, WORK_DRO_Y);
  neuDroValues(10, WORK_DRO_Y);
}

function redrawWorkCoordButtons() {
  neuClear(10, WORK_WCS_Y, 220, WORK_WCS_H);
  neuSegmented(10, WORK_WCS_Y, 220, WORK_WCS_H, WCS_NAMES, pendantProbing.selectedCoordIndex);
}

function handleProbingWorkTouch(x, y) {
  if (_workConfirm) {
    if (neuDialogHit(_workDialog.ok, x, y)) {
      if (pendantConnected) {
        if (_workConfirm === "xy") send_line("G90 G0 X0 Y0");
        else send_line("G90 G0 Z0");
        // sim: reflect the move
        for (const a of _workConfirm === "xy" ? [0, 1] : [2]) { pendantMachine.mpos[a] -= pendantMachine.wpos[a]; pendantMachine.wpos[a] = 0; }
        syncControlsFromState();
      }
      _workConfirm = null; drawProbingWorkScreen();
    } else if (neuDialogHit(_workDialog.cancel, x, y)) { _workConfirm = null; drawProbingWorkScreen(); }
    return;
  }
  const seg = neuSegHit(10, WORK_WCS_Y, 220, WORK_WCS_H, 4, x, y);
  if (seg >= 0) {
    pendantProbing.selectedCoordIndex = seg;
    pendantProbing.selectedCoordSystem = WCS_NAMES[seg];
    redrawWorkCoordButtons();
    if (pendantConnected) send_line(WCS_NAMES[seg]);
    return;
  }
  if (y >= WORK_ZERO_Y - 4 && y <= WORK_ZERO_Y + WORK_ZERO_H + 4) {
    const pNum = pendantProbing.selectedCoordIndex + 1, n = pendantMachine.numAxes;
    const lay = workZeroLayout();
    for (let i = 0; i < lay.length; i++) {
      if (x < lay[i].x - 4 || x > lay[i].x + lay[i].w + 4) continue;
      if (!pendantConnected) return;
      if (i < n) send_line(`G10 L20 P${pNum} ${AXIS_NAMES[i]}0`);
      else { let cmd = `G10 L20 P${pNum}`; for (let a = 0; a < n; a++) cmd += ` ${AXIS_NAMES[a]}0`; send_line(cmd); }
      return;
    }
  }
  if (y >= WORK_GO_Y - 4 && y <= WORK_GO_Y + WORK_GO_H + 4 && pendantConnected) {
    _workConfirm = x < 120 ? "xy" : "z";
    _drawWorkConfirm();
    return;
  }
  const n = neuNavHit(WORK_NAV, x, y);
  if (n === "home") currentPendantScreen = PSCREEN_MAIN_MENU;
  else if (n === "jog") currentPendantScreen = PSCREEN_JOG_HOMING;
}
