/* screen_jog_homing.cpp — neumorphic layout
 *
 *   axis rows   y 38..      one row per axis: tap a row to jog that axis with the dial.
 *                           The selected row is pressed in; the others are raised.
 *                           Big number = work position, small grey = machine position.
 *   Step        segmented   0.01 / 0.1 / 1 / coarse.  The coarse slot cycles three states:
 *                           tap 1 select · tap 2 the dial picks 10/50/100 · tap 3 back to jogging
 *   Home        buttons     one per axis, plus All when there is room
 *   nav         Home | Feed (tap: dial sets jog feed) | Work
 */

function homeLabel(numAx, i) { return (numAx < 4 && i === numAx) ? "All" : AXIS_NAMES[i]; }

const JOG_COARSE_COUNT = 3;
const kCoarseLabelsMm = ["10", "50", "100"];
const kCoarseValuesMm = [10.0, 50.0, 100.0];
const kCoarseLabelsIn = [".5", "2.0", "4.0"];
const kCoarseValuesIn = [0.5, 2.0, 4.0];

function currentIncrements() {
  const ci = constrain(pendantJog.coarseIdx, 0, JOG_COARSE_COUNT - 1);
  if (pendantMachine.inInches) return { labels: [".001", ".010", ".100", kCoarseLabelsIn[ci]], values: [0.001, 0.010, 0.100, kCoarseValuesIn[ci]] };
  return { labels: ["0.01", "0.1", "1", kCoarseLabelsMm[ci]], values: [0.01, 0.1, 1.0, kCoarseValuesMm[ci]] };
}
function jogApplyCoarseIncrement() {
  if (pendantJog.selectedIncrement !== 3) return;
  pendantJog.increment = currentIncrements().values[3];
}

// ---- geometry: depends on the axis count ----
function jogLayout() {
  const n = pendantMachine.numAxes;
  const rowH = n <= 3 ? 34 : n === 4 ? 28 : 20, pitch = rowH + (n <= 4 ? 6 : 4);
  const rowsEnd = 38 + n * pitch - (pitch - rowH);
  const compact = n > 4;
  const stepCapY = rowsEnd + 8;
  const stepY = compact ? rowsEnd + 18 : rowsEnd + 20, stepH = 34;
  const homeCapY = stepY + stepH + 8;
  const homeY = compact ? stepY + stepH + 10 : homeCapY + 12;
  const homeH = Math.min(34, 270 - homeY);
  return { n, rowH, pitch, stepCapY, stepY, stepH, homeCapY, homeY, homeH, compact };
}
function jogRowY(i) { const L = jogLayout(); return 38 + i * L.pitch; }
function jogHomeLayout() {
  const L = jogLayout(), n = L.n, numHome = n < 4 ? n + 1 : n;
  const gap = 8, w = ((220 - gap * (numHome - 1)) / numHome) | 0;
  const out = [];
  for (let i = 0; i < numHome; i++) out.push({ x: 10 + i * (w + gap), w: i === numHome - 1 ? 230 - (10 + i * (w + gap)) : w });
  return out;
}
function jogNav() {
  const spd = pendantMachine.inInches ? pendantJog.jogSpeedIn : pendantJog.jogSpeedMm;
  return [NAV_HOME, { id: "speed", label: "F " + spd, icon: "gauge", w: 100, on: pendantJog.speedDialMode },
          { id: "work", label: "Work" }];
}

let _jogKey = "";
let _incDialPrevAxis = 0;

function enterJogHoming() {
  releasePanelSprites();
  pendantJog.increment = currentIncrements().values[pendantJog.selectedIncrement];
  if (pendantJog.speedDialMode || pendantJog.incDialMode || pendantJog.selectedAxis < 0) {
    pendantJog.speedDialMode = false;
    pendantJog.incDialMode = false;
    pendantJog.selectedAxis = 0;
  }
}
function exitJogHoming() { releasePanelSprites(); }

function drawJogHomingScreen() {
  neuHeader("Jog");
  _jogKey = "";
  redrawJogAxisButtons();
  redrawJogIncrementLabel();
  redrawJogIncrementButtons();
  const L = jogLayout();
  if (!L.compact) neuCaption("Home", 12, L.homeCapY);
  const hl = jogHomeLayout();
  hl.forEach((b, i) => neuButton(b.x, L.homeY, b.w, L.homeH, homeLabel(L.n, i), { color: i < L.n ? NEU_AX[i] : NEU_HOME }));
  neuNav(jogNav());
}

// One axis row.  Selected = inset with the DIAL marker; otherwise raised with the machine position.
function _jogRow(i, full) {
  const L = jogLayout(), y = jogRowY(i), on = i === pendantJog.selectedAxis;
  const font = L.rowH >= 28 ? "B12" : "B9";
  if (full) { neuClear(10, y, 220, L.rowH); if (on) neuInset(10, y, 220, L.rowH, 10, 3); else neuRaised(10, y, 220, L.rowH, 10, 3); }
  else display.fillRect(on ? 14 : 12, y + (on ? 4 : 2), on ? 211 : 216, L.rowH - (on ? 6 : 4), on ? NEU_WELL : NEU_FACE);
  const base = neuCapBase(y, L.rowH, font);
  const homing = pendantJog.homingAxis === i;
  neuText(AXIS_NAMES[i], 22, base, font, NEU_AX[i]);
  neuText(fmtF(pendantMachine.wpos[i], pendantMachine.inInches ? 4 : 3), 158, base, font, on ? NEU_TEXT : NEU_TEXT2, "R");
  const capY = y + ((L.rowH - 8) / 2 | 0);
  if (homing) neuCaption("Homing", 218, capY, NEU_HOME, "R");
  else if (on) { neuCaption("Dial", 218, capY - (L.rowH >= 28 ? 4 : 0), NEU_ACCENT, "R"); if (L.rowH >= 28) display.fillRoundRect(196, capY + 7, 22, 3, 1, NEU_ACCENT); }
  else neuCaption(fmtF(pendantMachine.mpos[i], 1), 218, capY, NEU_TEXT3, "R");
}

// Called every 100 ms: repaints only the row contents whose numbers changed.
function updateJogAxisDisplay() {
  if (currentPendantScreen !== PSCREEN_JOG_HOMING) return;
  const key = neuDroKey() + pendantJog.homingAxis + pendantJog.selectedAxis;
  if (key === _jogKey) return;
  _jogKey = key;
  for (let i = 0; i < pendantMachine.numAxes; i++) _jogRow(i, false);
}
function redrawJogAxisButtons() {
  if (currentPendantScreen !== PSCREEN_JOG_HOMING) return;
  for (let i = 0; i < pendantMachine.numAxes; i++) _jogRow(i, true);
  _jogKey = neuDroKey() + pendantJog.homingAxis + pendantJog.selectedAxis;
}

// Caption row above the step control: unit, plus what the dial is doing right now.
function redrawJogIncrementLabel() {
  const L = jogLayout(), y = L.stepCapY;
  display.fillRect(10, y - 1, 220, 10, NEU_BG);
  const unit = pendantJog.selectedAxis === 3 ? "deg" : (pendantMachine.inInches ? "in" : "mm");
  neuCaption("Step " + unit, 12, y);
  if (pendantMachine.status.startsWith("Alarm")) neuCaption("Alarm: unlock to jog", 228, y, NEU_ALARM, "R");
  else if (pendantJog.incDialMode) neuCaption("Dial picks step", 228, y, NEU_ACCENT, "R");
  else if (pendantJog.speedDialMode) neuCaption("Dial sets feed", 228, y, NEU_ACCENT, "R");
}

function redrawJogIncrementButtons() {
  if (currentPendantScreen !== PSCREEN_JOG_HOMING) return;
  const L = jogLayout(), incs = currentIncrements();
  neuClear(10, L.stepY, 220, L.stepH);
  if (pendantJog.incDialMode) display.drawRoundRect(7, L.stepY - 3, 226, L.stepH + 6, 13, NEU_ACCENT);
  neuSegmented(10, L.stepY, 220, L.stepH, incs.labels, pendantJog.selectedIncrement);
  // coarse slot: three dots show which of 10/50/100 it holds
  const seg = neuSegLayout(10, 220, 4)[3], cx = seg.x + (seg.w / 2 | 0);
  for (let k = 0; k < JOG_COARSE_COUNT; k++)
    display.fillRect(cx - 7 + k * 6, L.stepY + L.stepH - 9, 3, 2, k === pendantJog.coarseIdx ? NEU_ACCENT : NEU_TEXT3);
  redrawJogIncrementLabel();
}

function redrawJogSpeedButton() {
  if (currentPendantScreen !== PSCREEN_JOG_HOMING) return;
  const lay = neuNavLayout(jogNav());
  neuNavButton(lay[1].x, lay[1].w, lay[1].it);
  redrawJogIncrementLabel();
}

function handleJogHomingTouch(x, y) {
  const L = jogLayout(), numAx = L.n;
  // Home row
  if (y >= L.homeY - 4 && y <= L.homeY + L.homeH + 4) {
    const hl = jogHomeLayout();
    for (let i = 0; i < hl.length; i++) {
      if (x >= hl[i].x - 4 && x <= hl[i].x + hl[i].w + 4) {
        if (!pendantConnected) return;
        if (i === numAx) { send_line("$H"); pendantJog.homingAxis = 0; }
        else { send_line("$H" + AXIS_NAMES[i]); pendantJog.homingAxis = i; }
        redrawJogAxisButtons();
        return;
      }
    }
  }
  // Axis rows
  for (let i = 0; i < numAx; i++) {
    const ry = jogRowY(i);
    if (y >= ry - 3 && y < ry + L.pitch - 3 && x >= 10 && x <= 230) {
      const leavingDial = pendantJog.incDialMode || pendantJog.speedDialMode;
      pendantJog.speedDialMode = false;
      pendantJog.incDialMode = false;
      pendantJog.selectedAxis = i;
      pendantJog.homingAxis = -1;
      redrawJogAxisButtons();
      if (leavingDial) { redrawJogIncrementButtons(); redrawJogSpeedButton(); }
      redrawJogIncrementLabel();
      return;
    }
  }
  // Step segmented control
  const seg = neuSegHit(10, L.stepY, 220, L.stepH, 4, x, y);
  if (seg >= 0) {
    const i = seg, wasSelected = pendantJog.selectedIncrement === i;
    pendantJog.selectedIncrement = i;
    pendantJog.increment = currentIncrements().values[i];
    saveJogPrefs();
    if (i === 3) {
      if (!wasSelected) pendantJog.incDialMode = false;                       // tap 1: select
      else if (pendantJog.incDialMode) {                                      // tap 3: back to jogging
        pendantJog.incDialMode = false;
        pendantJog.selectedAxis = constrain(_incDialPrevAxis, 0, numAx - 1);
      } else {                                                                // tap 2: dial picks the step
        _incDialPrevAxis = pendantJog.selectedAxis >= 0 ? pendantJog.selectedAxis : 0;
        pendantJog.incDialMode = true;
        pendantJog.speedDialMode = false;
        pendantJog.selectedAxis = -1;
      }
      redrawJogAxisButtons();
      redrawJogSpeedButton();
    } else if (pendantJog.incDialMode) {
      pendantJog.incDialMode = false;
      pendantJog.selectedAxis = constrain(_incDialPrevAxis, 0, numAx - 1);
      redrawJogAxisButtons();
    }
    redrawJogIncrementButtons();
    return;
  }
  const n = neuNavHit(jogNav(), x, y);
  if (n === "home") { currentPendantScreen = PSCREEN_MAIN_MENU; return; }
  if (n === "work") { currentPendantScreen = PSCREEN_PROBING_WORK; return; }
  if (n === "speed") {
    // Tap toggles: first tap hands the dial to the feed, second tap gives it back.
    if (pendantJog.speedDialMode) {
      pendantJog.speedDialMode = false;
      pendantJog.selectedAxis = constrain(_incDialPrevAxis, 0, numAx - 1);
    } else {
      _incDialPrevAxis = pendantJog.selectedAxis >= 0 ? pendantJog.selectedAxis : _incDialPrevAxis;
      const leavingInc = pendantJog.incDialMode;
      pendantJog.speedDialMode = true;
      pendantJog.incDialMode = false;
      pendantJog.selectedAxis = -1;
      if (leavingInc) redrawJogIncrementButtons();
    }
    redrawJogAxisButtons();
    redrawJogSpeedButton();
  }
}
