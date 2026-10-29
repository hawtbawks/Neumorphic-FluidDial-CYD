/* screen_espnow.cpp — pairing wizard + machine list (ESPNOW_SPEC §6b/§6c), neumorphic layout
 *
 * Pairing:   steps well (what to do on the controller) · status well · action buttons
 *            nav Back | Home   (Back cancels a pairing in progress)
 * Machines:  up to 5 rows; the active machine is pressed in.  Tap a row to use it.
 *            nav Back | Pair | Forget (forget asks first)
 */

const PAIR_STEP_Y = 38, PAIR_STEP_H = 100, PAIR_STAT_Y = 146, PAIR_STAT_H = 74, PAIR_ACT_Y = 230, PAIR_ACT_H = 40;
const PAIR_NAV = [NAV_HOME, { id: "back", label: "Back", icon: "back" }];

// A pairing window that never ends is worse than one that fails.
const PAIR_TIMEOUT_MS = 90000;
let _pairStartedMs = 0;

function enterEspNowPair() { releasePanelSprites(); _pairStartedMs = millis(); espnow_start_pairing(); }
function exitEspNowPair() { if (!espnow_pairing_complete()) espnow_cancel_pairing(); releasePanelSprites(); }

function pairView() {
  if (!espnow_radio_ready()) return "noradio";
  if (espnow_pairing_complete()) return "ok";
  if (simEspNow.pairing === "fail") return "fail";
  if (espnow_is_reconnecting()) return "working";
  if (_pairStartedMs && millis() - _pairStartedMs >= PAIR_TIMEOUT_MS) return "fail";   // only the idle wait times out
  return "wait";
}

function drawSteps(v) {
  display.fillRect(14, PAIR_STEP_Y + 4, 213, PAIR_STEP_H - 7, NEU_WELL);
  neuCaption("On the controller", 22, PAIR_STEP_Y + 9);
  const steps = [["Open the FluidNC console", v !== "wait"], ["Run  $espnow/pair", v === "ok" || v === "working"], ["Keep the pendant nearby", v === "ok"]];
  steps.forEach(([txt, done], i) => {
    const y = PAIR_STEP_Y + 26 + i * 20;
    if (done) { display.fillCircle(28, y + 3, 6, NEU_RUN); display.drawLine(25, y + 3, 27, y + 5, NEU_WELL); display.drawLine(27, y + 5, 31, y + 1, NEU_WELL); }
    else { display.drawCircle(28, y + 3, 6, NEU_TEXT3); neuSmall(String(i + 1), 26, y, NEU_TEXT3); }
    neuSmall(txt, 40, y, done ? NEU_TEXT3 : NEU_TEXT);
  });
  neuSmall("or WebUI > Settings > ESP-NOW", 40, PAIR_STEP_Y + 86, NEU_TEXT3);
}

function drawPairStatus(v) {
  display.fillRect(14, PAIR_STAT_Y + 4, 213, PAIR_STAT_H - 7, NEU_WELL);
  const M = {
    ok: ["Paired", NEU_RUN, null], working: ["Pairing...", NEU_JOG, "Exchanging keys (P-256)"],
    fail: ["Timed out", NEU_ALARM, "No controller responded"], noradio: ["Radio down", NEU_ALARM, "ESP-NOW failed to start"],
    wait: ["Waiting...", NEU_HOLD, "Listening for the controller"],
  }[v];
  neuLed(26, PAIR_STAT_Y + 20, M[1], 4);
  neuText(M[0], 38, PAIR_STAT_Y + 26, "B12", M[1]);
  if (v === "ok") {
    const info = {}, idx = espnow_active_profile_index();
    if (idx >= 0 && espnow_get_profile(idx, info)) {
      neuText(neuFit(info.hostname, "B9", 190), 22, PAIR_STAT_Y + 50, "B9", NEU_TEXT);
      neuCaption(macStr(info.mac, info.channel), 22, PAIR_STAT_Y + 56, NEU_TEXT3);
    }
  } else if (M[2]) {
    neuSmall(M[2], 22, PAIR_STAT_Y + 40, NEU_TEXT2);
    if (v === "fail") neuSmall("Check FluidNC is v4.0.4 or newer", 22, PAIR_STAT_Y + 54, NEU_TEXT3);
    if (v === "wait") neuSmall(`Gives up in ${Math.max(0, ((PAIR_TIMEOUT_MS - (millis() - _pairStartedMs)) / 1000) | 0)} s`, 22, PAIR_STAT_Y + 54, NEU_TEXT3);
  }
}

function drawPairActions(v) {
  display.fillRect(6, PAIR_ACT_Y - 4, 228, PAIR_ACT_H + 9, NEU_BG);
  if (v === "ok") {
    neuButton(10, PAIR_ACT_Y, 220, PAIR_ACT_H, "Use this machine", { color: NEU_RUN, icon: NEU_ICON.play });
  } else if (v === "fail") {
    neuButton(10, PAIR_ACT_Y, 104, PAIR_ACT_H, "Retry", { icon: NEU_ICON.refresh, color: NEU_ACCENT });
    neuButton(126, PAIR_ACT_Y, 104, PAIR_ACT_H, "Cancel", { color: NEU_TEXT2 });
  } else {
    neuButton(10, PAIR_ACT_Y, 220, PAIR_ACT_H, "Cancel pairing", { color: NEU_TEXT2 });
  }
}

let _lastPairView = "wait", _lastPairSecs = -1;
function drawEspNowPairScreen() {
  const v = pairView();
  neuHeader("Pair Machine");
  neuInset(10, PAIR_STEP_Y, 220, PAIR_STEP_H, 12, 3);
  neuInset(10, PAIR_STAT_Y, 220, PAIR_STAT_H, 12, 3);
  drawSteps(v); drawPairStatus(v); drawPairActions(v);
  _lastPairView = v;
  neuNav(PAIR_NAV);
}

function updateEspNowPairDisplay() {
  if (currentPendantScreen !== PSCREEN_ESPNOW_PAIR) return;
  const v = pairView(), secs = (millis() / 1000) | 0;
  if (v !== _lastPairView) { drawSteps(v); drawPairStatus(v); drawPairActions(v); _lastPairView = v; }
  else if (v === "wait" && secs !== _lastPairSecs) drawPairStatus(v);   // countdown
  _lastPairSecs = secs;
}

function handleEspNowPairTouch(x, y) {
  const v = pairView();
  if (y >= PAIR_ACT_Y - 4 && y <= PAIR_ACT_Y + PAIR_ACT_H + 4) {
    if (v === "ok") { currentPendantScreen = PSCREEN_CONNECTION; return; }
    if (v === "fail" && x < 120) { _pairStartedMs = millis(); espnow_start_pairing(); drawEspNowPairScreen(); return; }
    espnow_cancel_pairing(); currentPendantScreen = PSCREEN_CONNECTION; return;
  }
  const n = neuNavHit(PAIR_NAV, x, y);
  if (n === "home") currentPendantScreen = PSCREEN_MAIN_MENU;
  else if (n === "back") currentPendantScreen = v === "ok" ? PSCREEN_ESPNOW_MACHINES : PSCREEN_CONNECTION;
}

// ===== Machines list =====
const MCH_ROW0_Y = 38, MCH_ROW_H = 40, MCH_PITCH = 45, MCH_MAX = 5;
let _selectedRow = -1, _confirmForget = false, _forgetDialog = null, _mchKey = "";

function machinesNav() {
  const n = espnow_profile_count();
  return [NAV_BACK, { id: "pair", label: n >= MCH_MAX ? "Full" : "Pair", icon: "plus", dim: n >= MCH_MAX },
          { id: "forget", label: "Forget", icon: "trash", dim: _selectedRow < 0 || _selectedRow >= n }];
}

function enterEspNowMachines() { releasePanelSprites(); _selectedRow = espnow_active_profile_index(); _confirmForget = false; }
function exitEspNowMachines() { releasePanelSprites(); }
function machineRowY(i) { return MCH_ROW0_Y + i * MCH_PITCH; }

function drawMachineRow(i, full) {
  const info = {};
  if (!espnow_get_profile(i, info)) return;
  const y = machineRowY(i), active = espnow_active_profile_index() === i;
  if (full) { neuClear(10, y, 220, MCH_ROW_H); if (active) neuInset(10, y, 220, MCH_ROW_H, 10, 3); else neuRaised(10, y, 220, MCH_ROW_H, 10, 3); }
  else display.fillRect(active ? 14 : 12, y + (active ? 4 : 2), active ? 211 : 216, MCH_ROW_H - 6, active ? NEU_WELL : NEU_FACE);
  neuText(neuFit(info.hostname, "B9", 140), 22, y + 19, "B9", active ? NEU_ACCENT : NEU_TEXT);
  neuCaption(macStr(info.mac), 22, y + 26, NEU_TEXT3);
  const live = active && espnow_is_connected(), bars = live ? espnow_signal_bars() : 0;
  for (let b = 0; b < 4; b++) { const h = 4 + b * 3; display.fillRoundRect(196 + b * 6, y + 19 - h, 4, h, 1, b < bars ? NEU_RUN : NEU_SH_LIGHT); }
  neuCaption(active ? (live ? "Active" : "Offline") : "ch" + info.channel, 218, y + 26, active ? (live ? NEU_RUN : NEU_HOLD) : NEU_TEXT3, "R");
}

function drawEspNowMachinesScreen() {
  neuHeader("Machines");
  const n = espnow_profile_count();
  for (let i = 0; i < n; i++) drawMachineRow(i, true);
  if (n === 0) { neuInset(10, MCH_ROW0_Y, 220, 60, 12, 3); neuText("No machines paired yet", 120, MCH_ROW0_Y + 30, "B9", NEU_TEXT2, "C"); neuSmall("Tap Pair to add one", 120, MCH_ROW0_Y + 40, NEU_TEXT3, "C"); }
  neuCaption(`Tap a machine to use it  ${n}/${MCH_MAX}`, 120, 266, NEU_TEXT3, "C");
  neuNav(machinesNav());
  _mchKey = _machinesKey();
  if (_confirmForget) {
    const info = {}; espnow_get_profile(_selectedRow, info);
    _forgetDialog = neuDialog("Forget machine?", [info.hostname || "", "You will need to pair it again"], "Cancel", "Forget", NEU_ALARM);
  }
}
function _machinesKey() { return espnow_profile_count() + "|" + espnow_active_profile_index() + "|" + espnow_is_connected() + "|" + (espnow_is_connected() ? espnow_signal_bars() : 0); }

function updateEspNowMachinesDisplay() {
  if (currentPendantScreen !== PSCREEN_ESPNOW_MACHINES || _confirmForget) return;
  const k = _machinesKey();
  if (k === _mchKey) return;
  _mchKey = k;
  for (let i = 0; i < espnow_profile_count(); i++) drawMachineRow(i, false);
}

function handleEspNowMachinesTouch(x, y) {
  if (_confirmForget) {
    if (neuDialogHit(_forgetDialog.cancel, x, y)) { _confirmForget = false; drawEspNowMachinesScreen(); }
    else if (neuDialogHit(_forgetDialog.ok, x, y)) {
      if (_selectedRow >= 0) espnow_remove_profile(_selectedRow);
      _confirmForget = false;
      _selectedRow = espnow_active_profile_index();
      drawEspNowMachinesScreen();
    }
    return;
  }
  const n = espnow_profile_count();
  for (let i = 0; i < n; i++) {
    if (isTouchInBounds(x, y, 10, machineRowY(i), 220, MCH_ROW_H)) { _selectedRow = i; espnow_select_profile(i); drawEspNowMachinesScreen(); return; }
  }
  const hit = neuNavHit(machinesNav(), x, y);
  if (hit === "back") currentPendantScreen = PSCREEN_CONNECTION;
  else if (hit === "pair") { if (n < MCH_MAX) currentPendantScreen = PSCREEN_ESPNOW_PAIR; }
  else if (hit === "forget") { if (_selectedRow >= 0 && _selectedRow < n) { _confirmForget = true; drawEspNowMachinesScreen(); } }
}
