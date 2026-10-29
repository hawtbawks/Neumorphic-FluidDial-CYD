/* screen_probe.cpp — SCR0 probe hub, neumorphic layout
 *   probe type   segmented  Z Plate / XYZ / 3D
 *   shared       2x2 fields (tap = dial adjusts it; tap again or elsewhere releases)
 *   routines     1, 2 or 4 raised buttons depending on the probe type
 *   nav          Home | Configure (plate or 3D probe setup)
 */

const kProbeTypeLabels = ["Z Plate", "XYZ", "3D"];
const PRB_TYPE_Y = 38, PRB_TYPE_H = 36;
const PRB_FIELDS = [[10, 96], [124, 96], [10, 144], [124, 144]], PRB_FW = 106, PRB_FH = 40;
const PRB_ROUT_Y = 208;
const PROBE_NAV = [NAV_HOME, { id: "cfg", label: "Configure", icon: "gear" }];

function enterProbe() {
  pendantProbeV2.focusedField = -1;
  pendantProbeV2.confirmActive = false;
  pendantProbeV2.dialAccelCount = 0;
}
function exitProbe() { pendantProbeV2.focusedField = -1; }

function drawProbeTypeRow() {
  neuClear(10, PRB_TYPE_Y, 220, PRB_TYPE_H);
  neuSegmented(10, PRB_TYPE_Y, 220, PRB_TYPE_H, kProbeTypeLabels, pendantProbeV2.probeTypeIdx);
}

// Repaint ONLY the shared-settings fields — the dial handler calls this per detent.
function updateProbeSharedFields() {
  if (currentPendantScreen !== PSCREEN_PROBE) return;
  // Values are mm (the probe G-code runs in G21), so they are labelled mm in either unit mode.
  const p = pendantProbeV2, fo = p.focusedField, u = "mm";
  const vals = [["Probe", fmtF(p.probeRate, 0), u + "/m"], ["Seek", fmtF(p.seekRate, 0), u + "/m"],
                ["Retract", fmtF(p.retractDist, 1), u], ["Max Z", fmtF(p.maxZTravel, 1), u]];
  vals.forEach(([c, v, un], i) => neuFieldValue(PRB_FIELDS[i][0], PRB_FIELDS[i][1], PRB_FW, PRB_FH, c, v, un, { focus: fo === i }));
}

function _probeRoutines() {
  const n = probeRoutineCount();
  if (n === 1) return [{ x: 10, y: PRB_ROUT_Y, w: 220, h: 60, label: "Z Surface", to: PSCREEN_PROBE_Z, icon: "probe", font: "B12" }];
  if (n === 2) return [{ x: 10, y: PRB_ROUT_Y, w: 104, h: 60, label: "Z Surface", to: PSCREEN_PROBE_Z },
                       { x: 126, y: PRB_ROUT_Y, w: 104, h: 60, label: "Corner", to: PSCREEN_PROBE_CORNER }];
  return [["Z Surface", PSCREEN_PROBE_Z], ["Corner", PSCREEN_PROBE_CORNER], ["Bore", PSCREEN_PROBE_BORE], ["Boss", PSCREEN_PROBE_BOSS]]
    .map(([label, to], i) => ({ x: 10 + (i % 2) * 116, y: PRB_ROUT_Y + (i >> 1) * 34, w: 104, h: 28, label, to }));
}

function drawProbeScreen() {
  neuHeader("Probe");
  drawProbeTypeRow();
  neuCaption("Shared settings", 12, 84);
  PRB_FIELDS.forEach(([x, y]) => neuInset(x, y, PRB_FW, PRB_FH, 8, 3));
  updateProbeSharedFields();
  neuCaption("Routines", 12, 196);
  for (const r of _probeRoutines()) neuButton(r.x, r.y, r.w, r.h, r.label, { icon: r.icon ? NEU_ICON[r.icon] : null, font: r.font });
  neuNav(PROBE_NAV);
}

function handleProbeTouch(x, y) {
  const p = pendantProbeV2;
  const t = neuSegHit(10, PRB_TYPE_Y, 220, PRB_TYPE_H, PROBE_TYPE_COUNT, x, y);
  if (t >= 0) { if (p.probeTypeIdx !== t) { p.probeTypeIdx = t; p.focusedField = -1; drawProbeScreen(); } return; }
  for (let i = 0; i < 4; i++) {
    if (isTouchInBounds(x, y, PRB_FIELDS[i][0], PRB_FIELDS[i][1], PRB_FW, PRB_FH)) {
      p.focusedField = p.focusedField === i ? -1 : i;
      updateProbeSharedFields();
      return;
    }
  }
  for (const r of _probeRoutines()) {
    if (isTouchInBounds(x, y, r.x, r.y, r.w, r.h)) { p.focusedField = -1; currentPendantScreen = r.to; return; }
  }
  const n = neuNavHit(PROBE_NAV, x, y);
  if (n === "home") { currentPendantScreen = PSCREEN_MAIN_MENU; return; }
  if (n === "cfg") {
    p.returnScreen = PSCREEN_PROBE;
    currentPendantScreen = probeIs3D() ? PSCREEN_PROBE_CFG_3D : PSCREEN_PROBE_CFG_PLATE;
    return;
  }
  if (p.focusedField >= 0) { p.focusedField = -1; updateProbeSharedFields(); }   // tap elsewhere releases the dial
}
