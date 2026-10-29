/* screen_spindle_control.cpp — neumorphic layout
 *   RPM card     y 38  h 58   actual RPM (left) | target RPM (right; tap = dial adjusts it)
 *   direction    y 104 h 36   Fwd M3 / Rev M4
 *   presets      y 162 h 34   quarter / half / max of the spindle's range
 *   Start / Stop y 208 h 60   the only solid-filled buttons in the UI; Start reads
 *                             "Running" (pressed in) while the spindle is on
 *   nav          Home | Overrides
 */

function getSpindlePresets() {
  const maxRPM = pendantMachine.spindleMaxRPM > 0 ? pendantMachine.spindleMaxRPM : 24000;
  const minRPM = pendantMachine.spindleMinRPM;
  return [maxi(minRPM, ((maxRPM / 4 / 100) | 0) * 100), maxi(minRPM, ((maxRPM / 2 / 100) | 0) * 100), maxRPM];
}
function fmtRPM(rpm) { return rpm >= 1000 && rpm % 1000 === 0 ? rpm / 1000 + "k" : String(rpm); }

const SPN_CARD_Y = 38, SPN_CARD_H = 58, SPN_DIR_Y = 104, SPN_PRE_Y = 162, SPN_BTN_Y = 208, SPN_BTN_H = 60;
const SPINDLE_NAV = [NAV_HOME, { id: "ovr", label: "Overrides", icon: "gauge" }];
let _spnKey = "", _spnRunKey = null;

function enterSpindleControl() {
  releasePanelSprites();
  requestSpindleConfig();
  if (pendantSpindle.targetRPM === 0) pendantSpindle.targetRPM = getSpindlePresets()[pendantSpindle.selectedPreset];
}
function exitSpindleControl() { pendantSpindle.dialMode = false; releasePanelSprites(); }

function drawSpindleControlScreen() {
  neuHeader("Spindle");
  _spnKey = ""; _spnRunKey = null;
  neuInset(10, SPN_CARD_Y, 220, SPN_CARD_H, 12, 3);
  updateSpindleRPMDisplay();
  redrawSpindleDirectionButtons();
  redrawSpindlePresetButtons();
  _spnStartStop();
  neuNav(SPINDLE_NAV);
}

function updateSpindleRPMDisplay() {
  if (currentPendantScreen !== PSCREEN_SPINDLE_CONTROL) return;
  const dial = pendantSpindle.dialMode;
  const key = pendantMachine.spindleRPM + "|" + pendantSpindle.targetRPM + "|" + dial;
  if (key !== _spnKey) {
    _spnKey = key;
    const y = SPN_CARD_Y;
    display.drawRoundRect(7, y - 3, 226, SPN_CARD_H + 6, 15, dial ? NEU_ACCENT : NEU_BG);
    display.drawRoundRect(8, y - 2, 224, SPN_CARD_H + 4, 14, dial ? NEU_ACCENT : NEU_BG);
    display.fillRect(14, y + 4, 213, SPN_CARD_H - 7, NEU_WELL);
    neuCaption("Actual", 22, y + 9);
    neuText(String(Math.round(pendantMachine.spindleRPM)), 22, y + 50, pendantMachine.spindleRPM >= 10000 ? "B18" : "B24", NEU_TEXT);
    display.drawFastVLine(124, y + 10, 38, NEU_SH_DARK);
    display.drawFastVLine(125, y + 10, 38, NEU_SH_LIGHT);
    neuCaption(dial ? "Target - dial" : "Target", 136, y + 9, dial ? NEU_ACCENT : NEU_TEXT3);
    neuText(String(pendantSpindle.targetRPM), 218, y + 44, "B12", NEU_ACCENT, "R");
    display.fillRoundRect(136, y + 49, 82, 2, 1, dial ? NEU_ACCENT : NEU_SH_LIGHT);
  }
  if (_spnRunKey !== null && _spnRunKey !== pendantMachine.spindleRunning) _spnStartStop();
}

function redrawSpindleDirectionButtons() {
  if (currentPendantScreen !== PSCREEN_SPINDLE_CONTROL) return;
  neuClear(10, SPN_DIR_Y, 220, 36);
  neuSegmented(10, SPN_DIR_Y, 220, 36, ["Fwd  M3", "Rev  M4"], pendantSpindle.directionFwd ? 0 : 1);
}

function redrawSpindlePresetButtons() {
  if (currentPendantScreen !== PSCREEN_SPINDLE_CONTROL) return;
  const presets = getSpindlePresets();
  display.fillRect(10, SPN_PRE_Y - 13, 220, 10, NEU_BG);
  neuCaption("Preset rpm", 12, SPN_PRE_Y - 12);
  neuCaption(pendantMachine.spindleMinRPM + " - " + pendantMachine.spindleMaxRPM, 228, SPN_PRE_Y - 12, NEU_TEXT3, "R");
  neuClear(10, SPN_PRE_Y, 220, 34);
  neuSegmented(10, SPN_PRE_Y, 220, 34, presets.map(fmtRPM), presets.indexOf(pendantSpindle.targetRPM));
  updateSpindleRPMDisplay();
}

function _spnStartStop() {
  const running = pendantMachine.spindleRunning;
  _spnRunKey = running;
  neuClear(10, SPN_BTN_Y, 104, SPN_BTN_H);
  if (running) neuButton(10, SPN_BTN_Y, 104, SPN_BTN_H, "Running", { on: true, color: NEU_RUN, bar: false });
  else neuButton(10, SPN_BTN_Y, 104, SPN_BTN_H, "Start", { face: NEU_GO_FACE, color: NEU_TEXT, icon: NEU_ICON.play, font: "B12" });
  neuClear(126, SPN_BTN_Y, 104, SPN_BTN_H);
  neuButton(126, SPN_BTN_Y, 104, SPN_BTN_H, "Stop", { face: NEU_STOP_FACE, color: NEU_TEXT, icon: NEU_ICON.stop, font: "B12" });
}

function handleSpindleControlTouch(x, y) {
  const dir = neuSegHit(10, SPN_DIR_Y, 220, 36, 2, x, y);
  if (dir >= 0) {
    pendantSpindle.directionFwd = dir === 0; pendantMachine.spindleDir = dir === 0 ? "Fwd" : "Rev";
    redrawSpindleDirectionButtons(); return;
  }
  const pre = neuSegHit(10, SPN_PRE_Y, 220, 34, 3, x, y);
  if (pre >= 0) {
    pendantSpindle.dialMode = false; pendantSpindle.selectedPreset = pre;
    pendantSpindle.targetRPM = getSpindlePresets()[pre];
    redrawSpindlePresetButtons(); return;
  }
  if (isTouchInBounds(x, y, 10, SPN_CARD_Y, 220, SPN_CARD_H)) {
    pendantSpindle.dialMode = !pendantSpindle.dialMode;
    if (pendantSpindle.dialMode) {
      const maxRPM = pendantMachine.spindleMaxRPM > 0 ? pendantMachine.spindleMaxRPM : 24000;
      pendantSpindle.targetRPM = constrain(pendantSpindle.targetRPM, pendantMachine.spindleMinRPM, maxRPM);
    }
    updateSpindleRPMDisplay(); return;
  }
  if (isTouchInBounds(x, y, 10, SPN_BTN_Y, 104, SPN_BTN_H)) {
    if (pendantConnected) send_line(`${pendantSpindle.directionFwd ? "M3" : "M4"} S${pendantSpindle.targetRPM}`);
    pendantMachine.spindleRunning = true;
    _spnStartStop(); return;
  }
  if (isTouchInBounds(x, y, 126, SPN_BTN_Y, 104, SPN_BTN_H)) {
    if (pendantConnected) send_line("M5");
    pendantMachine.spindleRunning = false;
    _spnStartStop(); return;
  }
  const n = neuNavHit(SPINDLE_NAV, x, y);
  if (n === "home") currentPendantScreen = PSCREEN_MAIN_MENU;
  else if (n === "ovr") currentPendantScreen = PSCREEN_FEEDS_SPEEDS;
}
