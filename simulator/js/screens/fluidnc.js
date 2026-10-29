/* screen_fluidnc.cpp — neumorphic layout
 *   info well   y 38 h 162   six key/value rows: versions, network, free heap, link status
 *   Connection  raised row -> Connection screen
 *   Display     raised row: tap (or turn the dial) to flip the screen rotation
 *   nav         Home | Status
 */

const FNC_NAV = [NAV_HOME, { id: "status", label: "Status", icon: "status" }];
const FNC_CONN_Y = 208, FNC_ROT_Y = 244, FNC_ROW_H = 30;
let _fncKey = "";

function enterFluidNC() { releasePanelSprites(); _fncKey = ""; }
function exitFluidNC() {
  releasePanelSprites();
  if (pendantMachine.rotationDirty) {
    localStorage.setItem("sim.rotation", String(pendantMachine.rotation));
    pendantMachine.rotationDirty = false;
  }
}

function _fncRows() {
  const m = pendantMachine;
  const isEth = m.netMode === "Ethernet", ethDown = isEth && (m.netStatus || "").startsWith("Link down");
  const has = (v) => v && v !== "---";
  return [
    ["FluidDial", m.fluidDialVersion, NEU_TEXT],
    ["FluidNC", m.fluidNCVersion || "---", m.fluidNCVersion ? NEU_TEXT : NEU_TEXT3],
    ["IP address", has(m.ipAddress) ? m.ipAddress : "---", has(m.ipAddress) ? NEU_TEXT : NEU_TEXT3],
    [isEth ? "Network" : "WiFi SSID", isEth ? (ethDown ? "Ethernet, no link" : "Wired Ethernet") : (has(m.wifiSSID) ? m.wifiSSID : "---"),
      ethDown ? NEU_HOLD : (isEth || has(m.wifiSSID)) ? NEU_TEXT : NEU_TEXT3],
    ["Free heap", ((ESP.getFreeHeap() / 1024) | 0) + " KB", NEU_TEXT],
    ["Link", m.connectionStatus, pendantConnected ? NEU_RUN : NEU_ALARM],
  ];
}

function updateFluidNCDisplay() {
  if (currentPendantScreen !== PSCREEN_FLUIDNC) return;
  const rows = _fncRows(), key = JSON.stringify(rows) + pendantMachine.displayRotation;
  if (key === _fncKey) return;
  _fncKey = key;
  display.fillRect(14, 42, 213, 155, NEU_WELL);
  rows.forEach(([k, v, col], i) => {
    const y = 44 + i * 26;
    if (i) display.drawFastHLine(22, y - 2, 196, NEU_SH_DARK);
    neuCaption(k, 22, y + 8);
    neuText(neuFit(v, "B9", 120), 218, y + 17, "B9", col, "R");
  });
  _fncRow(FNC_ROT_Y, "Display", pendantMachine.displayRotation, "gear");
}

function _fncRow(y, label, value, icon) {
  neuClear(10, y, 220, FNC_ROW_H);
  neuRaised(10, y, 220, FNC_ROW_H, 10, 3);
  NEU_ICON[icon](30, y + (FNC_ROW_H / 2 | 0), NEU_ACCENT);
  neuText(label, 46, neuCapBase(y, FNC_ROW_H, "B9"), "B9", NEU_TEXT);
  neuCaption(value, 202, y + 11, NEU_TEXT2, "R");
  NEU_ICON.chevron(214, y + (FNC_ROW_H / 2 | 0), NEU_TEXT3);
}

function drawFluidNCScreen() {
  neuHeader("FluidNC");
  _fncKey = "";
  neuInset(10, 38, 220, 162, 12, 3);
  _fncRow(FNC_CONN_Y, "Connection", comms_active_mode() === "UART" ? "UART" : "ESP-NOW", "link");
  updateFluidNCDisplay();
  neuNav(FNC_NAV);
}

function handleFluidNCTouch(x, y) {
  if (isTouchInBounds(x, y, 10, FNC_CONN_Y, 220, FNC_ROW_H)) { currentPendantScreen = PSCREEN_CONNECTION; return; }
  if (isTouchInBounds(x, y, 10, FNC_ROT_Y, 220, FNC_ROW_H)) { handleEncoderDelta(1); return; }   // same as a dial turn
  const n = neuNavHit(FNC_NAV, x, y);
  if (n === "home") currentPendantScreen = PSCREEN_MAIN_MENU;
  else if (n === "status") currentPendantScreen = PSCREEN_STATUS;
}
