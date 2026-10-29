/* screen_connection.cpp — transport select + link status (ESPNOW_SPEC §6a), neumorphic layout
 *   Transport  segmented UART / ESP-NOW (a change arms a 5 s reboot; tapping back cancels it)
 *   link well  wired details, or ESP-NOW state, machine, signal and receive counters
 *   actions    Pair / Machines (ESP-NOW), or the reboot countdown
 *   nav        Back (FluidNC) | Home
 */


// Transport changes only take effect at boot, so the change arms a countdown and
// the pendant reboots itself.  0 = no restart pending.  The sim reloads the page
// where the firmware calls ESP.restart().
const RESTART_DELAY_MS = 5000;
let _restartAtMs = 0;

function restartSecsLeft() {
  if (!_restartAtMs) return 0;
  const remain = _restartAtMs - millis();
  return remain <= 0 ? 0 : Math.ceil(remain / 1000);
}

// Simulated NVS transport selection (firmware: get/set_transport_force) versus
// the transport actually RUNNING (firmware: comms_active_mode()).  They differ
// between a transport change and the reboot that applies it — the firmware only
// re-reads NVS in comms_init(), so the LINK panel keeps showing the live link
// during the countdown.  Keeping them separate here preserves that behaviour.
let simTransport       = "ESPNOW";   // stored choice ("UART" | "ESPNOW")
let simActiveTransport = "ESPNOW";   // what comms_init() picked at boot
function get_transport_force() { return simTransport; }
function set_transport_force(f) { simTransport = f; }
function comms_active_mode() { return simActiveTransport; }

function enterConnection() { releasePanelSprites(); _restartAtMs = 0; }
function exitConnection() { releasePanelSprites(); }

function macStr(mac, ch) {
  const h = mac.map(b => b.toString(16).toUpperCase().padStart(2, "0")).join(":");
  return ch === undefined ? h : `${h} ch${ch}`;
}

const CN_TP_Y = 52, CN_TP_H = 36, CN_LK_Y = 96, CN_LK_H = 104, CN_ACT_Y = 210, CN_ACT_H = 44;
const CONN_NAV = [NAV_HOME, { id: "back", label: "FluidNC", icon: "back" }];
let _cnKey = "";

// Link well contents.  Repainted only when what it shows changes.
function drawLinkPanel(force) {
  const wired = comms_active_mode() === "UART";
  const up = !wired && espnow_is_connected(), paired = !wired && espnow_has_saved_pairing();
  const info = {}, idx = wired ? -1 : espnow_active_profile_index();
  const got = idx >= 0 && espnow_get_profile(idx, info);
  const lost = wired ? 0 : espnow_rx_dropped() + espnow_rx_pkt_dropped(), drop = wired ? 0 : espnow_frag_aborted();
  const key = [wired, wired || espnow_radio_ready(), up, paired, got && info.hostname, up && espnow_signal_bars(), up && espnow_rssi(),
               wired ? 0 : espnow_rx_bytes(), lost, drop, pendantMachine.baudRate, pendantMachine.port].join("|");
  if (!force && key === _cnKey) return;
  _cnKey = key;
  const y = CN_LK_Y;
  display.fillRect(14, y + 4, 213, CN_LK_H - 7, NEU_WELL);
  const head = (col, word) => { neuLed(26, y + 18, col, 4); neuText(word, 38, y + 24, "B12", col); };
  if (wired) {
    head(NEU_RUN, "Wired");
    neuCaption("Baud", 22, y + 40); neuCaption("Port", 124, y + 40);
    neuText(pendantMachine.baudRate, 22, y + 66, "B9", NEU_TEXT);
    neuText(pendantMachine.port, 124, y + 66, "B9", NEU_TEXT);
    neuSmall("FluidNC over the RJ12 cable", 22, y + 82, NEU_TEXT3);
    return;
  }
  if (!espnow_radio_ready()) {           // the radio never started: waiting will not fix it
    head(NEU_ALARM, "Radio down");
    neuSmall("ESP-NOW failed to start.", 22, y + 44, NEU_TEXT2);
    neuSmall("Power-cycle the pendant; if it", 22, y + 58, NEU_TEXT3);
    neuSmall("persists, switch to UART.", 22, y + 70, NEU_TEXT3);
    return;
  }
  head(up ? NEU_RUN : paired ? NEU_HOLD : NEU_ALARM, up ? "Connected" : paired ? "Reconnecting" : "Not paired");
  if (!paired) { neuSmall("Tap Pair to add a machine", 22, y + 46, NEU_TEXT2); return; }
  neuText(neuFit(got ? info.hostname : "(unnamed)", "B9", 140), 22, y + 50, "B9", NEU_TEXT);
  if (got) neuCaption(macStr(info.mac), 22, y + 58, NEU_TEXT3);
  const bars = up ? espnow_signal_bars() : 0;
  for (let i = 0; i < 4; i++) { const h = 5 + i * 4; display.fillRoundRect(190 + i * 7, y + 28 - h, 5, h, 1, i < bars ? NEU_RUN : NEU_SH_LIGHT); }
  if (up) neuCaption(espnow_rssi() + "dBm", 218, y + 34, NEU_TEXT3, "R");
  // Receive-path counters.  LOST: packets never handed over.  DROP: whole messages
  // abandoned in fragment reassembly.  Both read 0 on a healthy link.
  display.drawFastHLine(22, y + 70, 196, NEU_SH_DARK);
  const cols = [["RX", String(espnow_rx_bytes()), NEU_TEXT2], ["Lost", String(lost), lost ? NEU_ALARM : NEU_TEXT2],
                ["Drop", String(drop), drop ? NEU_ALARM : NEU_TEXT2], ["Ch", got ? String(info.channel) : "-", NEU_TEXT2]];
  cols.forEach(([k, v, c], i) => { neuCaption(k, 22 + i * 50, y + 77); neuSmall(v, 22 + i * 50, y + 87, c); });
}

function drawTransportPanel() {
  neuCaption("Transport", 12, CN_TP_Y - 12);
  neuClear(10, CN_TP_Y, 220, CN_TP_H);
  neuSegmented(10, CN_TP_Y, 220, CN_TP_H, ["UART", "ESP-NOW"], get_transport_force() === "UART" ? 0 : 1);
}

function drawActionRow() {
  display.fillRect(6, CN_ACT_Y - 4, 228, CN_ACT_H + 9, NEU_BG);
  if (_restartAtMs) {
    neuInset(10, CN_ACT_Y, 220, CN_ACT_H, 12, 3);
    neuCaption("Transport changed", 22, CN_ACT_Y + 9, NEU_HOLD);
    neuCaption("Tap back to undo", 218, CN_ACT_Y + 9, NEU_TEXT3, "R");
    neuText("Rebooting in " + restartSecsLeft() + " s", 22, CN_ACT_Y + 35, "B9", NEU_HOLD);
    return;
  }
  if (get_transport_force() !== "ESPNOW") return;
  neuButton(10, CN_ACT_Y, 104, CN_ACT_H, "Pair", { icon: NEU_ICON.plus });
  neuButton(126, CN_ACT_Y, 104, CN_ACT_H, "Machines");
}

function drawConnectionScreen() {
  neuHeader("Connection");
  drawTransportPanel();
  neuInset(10, CN_LK_Y, 220, CN_LK_H, 12, 3);
  drawLinkPanel(true);
  drawActionRow();
  neuNav(CONN_NAV);
}

let _lastShownSecs = -1;
function updateConnectionDisplay() {
  if (currentPendantScreen !== PSCREEN_CONNECTION) return;
  if (_restartAtMs) {
    if (millis() >= _restartAtMs) {
      _restartAtMs = 0;
      simRestart();          // firmware: ESP.restart()
      return;
    }
    const secs = restartSecsLeft();
    if (secs !== _lastShownSecs) { _lastShownSecs = secs; drawActionRow(); }
  }
  drawLinkPanel();
}

// Stands in for ESP.restart(): re-runs boot-time transport selection, including
// the first-run routing that opens pairing when nothing is paired yet.
function simRestart() {
  _lastShownSecs = -1;
  simActiveTransport = simTransport;   // comms_init() re-reads NVS at boot
  // Nothing paired lands on Connection (which shows "Not paired" + Pair New),
  // NOT on the pairing screen — auto-advertising from boot left the pendant
  // waiting with no obvious way out.
  if (simTransport === "ESPNOW" && !espnow_has_saved_pairing()) {
    navigateTo(PSCREEN_CONNECTION);
    drawConnectionScreen();
  } else {
    navigateTo(PSCREEN_MAIN_MENU);
  }
}

function handleConnectionTouch(x, y) {
  const seg = neuSegHit(10, CN_TP_Y, 220, CN_TP_H, 2, x, y);
  if (seg >= 0) {
    const want = seg === 0 ? "UART" : "ESPNOW";
    if (want === get_transport_force()) return;
    set_transport_force(want);
    // Re-selecting during the countdown is the escape hatch for a mis-tap.
    _restartAtMs = want === simActiveTransport ? 0 : millis() + RESTART_DELAY_MS;
    _lastShownSecs = -1;
    drawTransportPanel(); drawLinkPanel(true); drawActionRow();
    return;
  }
  if (!_restartAtMs && get_transport_force() === "ESPNOW" && y >= CN_ACT_Y - 4 && y <= CN_ACT_Y + CN_ACT_H + 4) {
    currentPendantScreen = x < 120 ? PSCREEN_ESPNOW_PAIR : PSCREEN_ESPNOW_MACHINES;
    return;
  }
  const n = neuNavHit(CONN_NAV, x, y);
  if (n === "home") currentPendantScreen = PSCREEN_MAIN_MENU;
  else if (n === "back") currentPendantScreen = PSCREEN_FLUIDNC;
}
