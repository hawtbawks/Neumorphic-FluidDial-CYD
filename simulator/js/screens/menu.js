/* screen_main_menu.cpp — neumorphic layout
 *   hero card  y 38 h 68   DRO (work + machine), or the alarm / link / sync state when that matters more
 *   tiles      2 x 4 grid, 105 x 44 at a 49 px pitch, raised, icon above a one-word label
 * No nav bar: this is Home.
 */

const MENU_TILES = [
  ["Jog", "jog", PSCREEN_JOG_HOMING], ["Work", "origin", PSCREEN_PROBING_WORK],
  ["Overrides", "gauge", PSCREEN_FEEDS_SPEEDS], ["Spindle", "spindle", PSCREEN_SPINDLE_CONTROL],
  ["Macros", "macro", PSCREEN_MACROS], ["Files", "card", PSCREEN_SD_CARD],
  ["Probe", "probe", PSCREEN_PROBE], ["Status", "status", PSCREEN_STATUS],
];
function menuTileRect(i) { return { x: 10 + (i % 2) * 115, y: 114 + (i >> 1) * 49, w: 105, h: 44 }; }

let _menuKey = "";

function enterMainMenu() { releasePanelSprites(); _menuKey = ""; }
function exitMainMenu() { releasePanelSprites(); }

function drawMainMenu() {
  neuHeader("FluidDial");
  _menuKey = "";
  neuInset(10, 38, 220, NEU_DRO_H, 12, 3);
  updateMainMenuDisplay();
  MENU_TILES.forEach(([label, icon], i) => {
    const r = menuTileRect(i);
    neuRaised(r.x, r.y, r.w, r.h, 12, 3);
    NEU_ICON[icon](r.x + (r.w / 2 | 0), r.y + 15, NEU_ACCENT);
    neuText(label, r.x + (r.w / 2 | 0), r.y + 39, "B9", NEU_TEXT, "C");
  });
}

// Wireless link problems are reported in the hero card, captioned ESP-NOW, so a
// dead radio link never looks like a machine alarm.  UART has no link to report.
function espNowLinkFault() {
  if (comms_active_mode() !== "ESPNOW") return null;
  if (!espnow_radio_ready()) return { big: "Radio down", col: NEU_ALARM, sub: "ESP-NOW failed to start" };
  if (!espnow_has_saved_pairing()) return { big: "Not paired", col: NEU_ALARM, sub: "Connection > Pair" };
  if (!espnow_is_connected()) {
    const info = {}, idx = espnow_active_profile_index();
    const name = (idx >= 0 && espnow_get_profile(idx, info)) ? info.hostname : "machine";
    return { big: "No link", col: NEU_HOLD, sub: "Reconnecting to " + name };
  }
  return null;
}

function updateMainMenuDisplay() {
  if (currentPendantScreen !== PSCREEN_MAIN_MENU) return;
  const st = pendantMachine.status, fault = espNowLinkFault();
  const syncing = !pendantSynced || st === "N/C" || st.length === 0;
  const mode = fault ? "fault:" + fault.big : syncing ? "sync:" + pendantConnected : st.startsWith("Alarm") ? "alarm:" + st : "dro";
  const key = mode + "|" + (mode === "dro" ? neuDroKey() : "");
  if (key === _menuKey) return;
  const modeChanged = _menuKey.split("|")[0] !== mode;
  _menuKey = key;
  if (mode === "dro") {
    if (modeChanged) { display.fillRect(13, 41, 214, 62, NEU_WELL); neuDroLabels(10, 38); }
    neuDroValues(10, 38);
    return;
  }
  display.fillRect(13, 41, 214, 62, NEU_WELL);
  if (fault) {
    neuCaption("ESP-NOW", 22, 48);
    neuText(fault.big, 22, 78, "B12", fault.col);
    neuSmall(fault.sub, 22, 88, NEU_TEXT2);
  } else if (syncing) {
    neuCaption("Controller", 22, 48);
    neuText(pendantConnected ? "Syncing..." : "Connecting...", 22, 78, "B12", pendantConnected ? NEU_HOLD : NEU_TEXT2);
    neuSmall(pendantConnected ? "Reading machine settings" : "Waiting for FluidNC", 22, 88, NEU_TEXT3);
  } else {
    neuCaption(st.replace(":", " "), 22, 48, NEU_ALARM);
    neuText(alarmDescription(st).replace(" triggered", ""), 22, 78, "B12", NEU_ALARM);
    neuSmall("Home or unlock to clear", 22, 88, NEU_TEXT2);
  }
}

function handleMainMenuTouch(x, y) {
  for (let i = 0; i < MENU_TILES.length; i++) {
    const r = menuTileRect(i);
    if (x >= r.x - 4 && x <= r.x + r.w + 4 && y >= r.y - 4 && y <= r.y + r.h + 4) { currentPendantScreen = MENU_TILES[i][2]; return; }
  }
}
