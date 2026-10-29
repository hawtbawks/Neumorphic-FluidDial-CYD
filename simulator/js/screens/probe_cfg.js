/* screen_probe_cfg.cpp — SCR0a 3D probe setup / SCR0b touch plate setup, neumorphic layout
 *
 * Plate setup (type comes from the probe hub):
 *   Z plate    one large thickness field + dimensioned side view
 *   XYZ plate  2x2 fields (thickness, width, X/Y wall offsets) + plan view
 * 3D probe setup:
 *   stylus     ball diameter + deflection fields
 *   deflection calibration: gauge width field + Calibrate (confirm, progress, result dialogs)
 * nav  Home | Back | Save
 */

const PROBE_CFG_NAV = [NAV_HOME, NAV_BACK, { id: "save", label: "Save", primary: true }];
let _calDialog = null;

function _cfgNavTouch(x, y) {
  const n = neuNavHit(PROBE_CFG_NAV, x, y);
  if (n === "home") { currentPendantScreen = PSCREEN_MAIN_MENU; return true; }
  if (n === "back") { currentPendantScreen = PSCREEN_PROBE; return true; }
  if (n === "save") { saveProbeSettings(); currentPendantScreen = PSCREEN_PROBE; return true; }
  return false;
}
function _cfgFieldTouch(fields, x, y, redrawFields) {
  for (let i = 0; i < fields.length; i++) {
    const f = fields[i];
    if (isTouchInBounds(x, y, f.x, f.y, f.w, f.h)) {
      pendantProbeV2.focusedField = pendantProbeV2.focusedField === i ? -1 : i;
      redrawFields();
      return true;
    }
  }
  return false;
}

// ===== 3D PROBE SETUP =====
const P3_FIELDS = [{ x: 10, y: 52, w: 106, h: 44 }, { x: 124, y: 52, w: 106, h: 44 }, { x: 10, y: 120, w: 106, h: 44 }];
const P3_CAL = { x: 124, y: 120, w: 106, h: 44 };
const P3_DIAG = { x: 10, y: 174, w: 220, h: 96 };

function enterProbeCfg3D() {
  pendantProbeV2.focusedField = 0;
  pendantProbeV2.confirmActive = false;
  pendantProbeV2.dialAccelCount = 0;
  pendantProbeV2.calState = 0;
}
function exitProbeCfg3D() { pendantProbeV2.focusedField = -1; }

// Emit the (side-effect-free) calibration probe program. In the simulator there
// is no real probe, so we also simulate a plausible measured deflection.
function runProbeCalibration() {
  if (!pendantConnected) return;
  const p = pendantProbeV2;
  const seekF = p.seekRate, fineF = p.probeRate, maxZ = p.maxZTravel, retZ = p.retractDist;
  const CLEAR = 10, DEPTH = 5, toClear = p.calGaugeWidth / 2 + CLEAR, plunge = retZ + DEPTH;
  send_line("G21 G90");
  send_line("#<sx> = #5420");
  send_line("#<sy> = #5421");
  send_line("G91");
  send_line(`G38.2 Z-${fmtF(maxZ, 3)} F${fmtF(fineF, 0)}`);
  send_line(`G0 Z${fmtF(retZ, 3)} F500`);
  send_line("G90");
  // left face → x1
  send_line("G91");
  send_line(`G0 X-${fmtF(toClear, 3)} F1000`);
  send_line(`G0 Z-${fmtF(plunge, 3)} F500`);
  send_line(`G38.2 X${fmtF(CLEAR + 3, 3)} F${fmtF(seekF, 0)}`);
  send_line("G0 X-1.5 F1000");
  send_line(`G38.2 X2.5 F${fmtF(fineF, 0)}`);
  send_line(`G0 Z${fmtF(plunge, 3)} F500`);
  send_line("G90 G0 X#<sx> Y#<sy> F1000");
  // right face → x2
  send_line("G91");
  send_line(`G0 X${fmtF(toClear, 3)} F1000`);
  send_line(`G0 Z-${fmtF(plunge, 3)} F500`);
  send_line(`G38.2 X-${fmtF(CLEAR + 3, 3)} F${fmtF(seekF, 0)}`);
  send_line("G0 X1.5 F1000");
  send_line(`G38.2 X-2.5 F${fmtF(fineF, 0)}`);
  send_line(`G0 Z${fmtF(plunge, 3)} F500`);
  send_line("G90 G0 X#<sx> Y#<sy> F1000");
  // --- simulate probe results: pretend the probe over-reads 0.013 mm per face ---
  const simDefl = 0.013;
  const x1 = -p.calGaugeWidth / 2 - p.ballDia / 2 - simDefl;
  const x2 = p.calGaugeWidth / 2 + p.ballDia / 2 + simDefl;
  p.calResult = Math.max(-1, Math.min(1, ((x2 - x1) - p.calGaugeWidth - p.ballDia) / 2));
  p.calState = 2;
}

function updateProbeCfg3DScreen() {
  if (currentPendantScreen !== PSCREEN_PROBE_CFG_3D) return;
  // (Firmware polls probe results here; the sim resolves synchronously in runProbeCalibration.)
}

function updateProbeCfg3DFields() {
  if (currentPendantScreen !== PSCREEN_PROBE_CFG_3D) return;
  const p = pendantProbeV2, fo = p.focusedField;
  const vals = [["Ball dia", fmtF(p.ballDia, 1)], ["Deflection", fmtF(p.deflection, 3)], ["Gauge width", fmtF(p.calGaugeWidth, 1)]];
  vals.forEach(([c, v], i) => neuFieldValue(P3_FIELDS[i].x, P3_FIELDS[i].y, P3_FIELDS[i].w, P3_FIELDS[i].h, c, v, "mm", { focus: fo === i, font: i === 1 ? "B9" : "B12" }));
}

function drawProbe3DGraphic() {
  const { x, y, w, h } = P3_DIAG, cx = x + (w / 2 | 0);
  neuInset(x, y, w, h, 12, 3);
  display.fillRect(cx - 7, y + 10, 14, 8, NEU_TEXT3);
  display.fillRoundRect(cx - 17, y + 18, 34, 18, 4, NEU_TEXT2);
  display.fillRect(cx - 1, y + 36, 3, 30, NEU_TEXT2);
  display.fillCircle(cx, y + 70, 5, NEU_ACCENT);
  display.fillRect(cx - 70, y + 78, 140, 6, NEU_TEXT3);
  // dimension: ball diameter
  display.drawFastHLine(cx + 12, y + 70, 30, NEU_TEXT3);
  neuSmall(fmtF(pendantProbeV2.ballDia, 1) + " mm", cx + 46, y + 66, NEU_ACCENT);
}

function drawProbeCfg3DScreen() {
  neuHeader("Probe Setup");
  neuCaption("Stylus  3D touch probe", 12, 40);
  P3_FIELDS.forEach((f) => neuInset(f.x, f.y, f.w, f.h, 8, 3));
  neuCaption("Deflection calibration", 12, 108);
  updateProbeCfg3DFields();
  neuButton(P3_CAL.x, P3_CAL.y, P3_CAL.w, P3_CAL.h, "Calibrate", { color: NEU_ACCENT });
  drawProbe3DGraphic();
  neuNav(PROBE_CFG_NAV);
  const p = pendantProbeV2;
  if (p.calState === 4) _calDialog = neuDialog("Calibrate?", ["The probe will move.", "Centre it over the gauge block."], "Cancel", "Start", NEU_RUN);
  else if (p.calState === 1) _calDialog = neuDialog("Calibrating...", ["Probing both faces"], null, null);
  else if (p.calState === 2) _calDialog = neuDialog(fmtF(p.calResult, 3) + " mm", ["Measured deflection", "Apply it to the stylus?"], "Cancel", "Apply", NEU_RUN, NEU_RUN);
  else if (p.calState === 3) _calDialog = neuDialog("Calibration failed", ["Check the setup and the", "probe connection"], null, "OK", NEU_TEXT, NEU_ALARM);
}

function handleProbeCfg3DTouch(x, y) {
  const p = pendantProbeV2;
  if (p.calState === 4) {
    if (neuDialogHit(_calDialog.cancel, x, y)) { p.calState = 0; drawProbeCfg3DScreen(); }
    else if (neuDialogHit(_calDialog.ok, x, y)) { p.calState = 1; p.calStartMs = Date.now(); drawProbeCfg3DScreen(); runProbeCalibration(); drawProbeCfg3DScreen(); }
    return;
  }
  if (p.calState === 2) {
    if (neuDialogHit(_calDialog.cancel, x, y)) { p.calState = 0; drawProbeCfg3DScreen(); }
    else if (neuDialogHit(_calDialog.ok, x, y)) { p.deflection = p.calResult; saveProbeSettings(); p.calState = 0; drawProbeCfg3DScreen(); }
    return;
  }
  if (p.calState === 3) { if (neuDialogHit(_calDialog.ok, x, y)) { p.calState = 0; drawProbeCfg3DScreen(); } return; }
  if (p.calState === 1) return;
  if (_cfgFieldTouch(P3_FIELDS, x, y, updateProbeCfg3DFields)) return;
  if (isTouchInBounds(x, y, P3_CAL.x, P3_CAL.y, P3_CAL.w, P3_CAL.h)) {
    p.calState = pendantConnected ? 4 : 3;
    drawProbeCfg3DScreen();
    return;
  }
  _cfgNavTouch(x, y);
}

// ===== TOUCH PLATE SETUP =====
function plateFields() {
  return pendantProbeV2.probeTypeIdx === PROBE_TYPE_XYZPLATE
    ? [{ x: 10, y: 52, w: 106, h: 42 }, { x: 124, y: 52, w: 106, h: 42 }, { x: 10, y: 102, w: 106, h: 42 }, { x: 124, y: 102, w: 106, h: 42 }]
    : [{ x: 10, y: 52, w: 220, h: 54 }];
}

function enterProbeCfgPlate() {
  pendantProbeV2.focusedField = 0;
  pendantProbeV2.confirmActive = false;
  pendantProbeV2.dialAccelCount = 0;
}
function exitProbeCfgPlate() { pendantProbeV2.focusedField = -1; }

function updateProbeCfgPlateFields() {
  if (currentPendantScreen !== PSCREEN_PROBE_CFG_PLATE) return;
  const p = pendantProbeV2, fo = p.focusedField, F = plateFields();
  if (F.length === 1) {
    neuFieldValue(F[0].x, F[0].y, F[0].w, F[0].h, "Plate thickness", fmtF(p.plateThick, 2), "mm", { focus: fo === 0, font: "B18" });
    display.fillRect(192, 231, 34, 10, NEU_WELL);                     // the diagram's dimension label
    neuSmall(fmtF(p.plateThick, 2), 192, 232, NEU_ACCENT);
    return;
  }
  const vals = [["Thickness", p.plateThick], ["Width", p.plateWidth], ["Wall X", p.plateOffX], ["Wall Y", p.plateOffY]];
  vals.forEach(([c, v], i) => neuFieldValue(F[i].x, F[i].y, F[i].w, F[i].h, c, fmtF(v, 1), "mm", { focus: fo === i }));
}

function drawPlateDiagram() {
  const xyz = pendantProbeV2.probeTypeIdx === PROBE_TYPE_XYZPLATE;
  const d = xyz ? { x: 10, y: 154, w: 220, h: 116 } : { x: 10, y: 116, w: 220, h: 154 };
  neuInset(d.x, d.y, d.w, d.h, 12, 3);
  const cx = d.x + 110, by = d.y + d.h - 24;
  if (xyz) {   // plan view: L-shaped plate sitting on the stock corner
    display.fillRect(cx - 30, by - 50, 90, 52, NEU_TEXT3);
    display.fillRect(cx - 42, by - 62, 80, 12, NEU_ACCENT);
    display.fillRect(cx - 42, by - 62, 12, 64, NEU_ACCENT);
    neuCaption("Plan view", d.x + 12, d.y + 10);
  } else {     // side view: plate on the stock, probe tip on the plate, thickness dimension
    display.fillRect(cx - 80, by, 160, 18, NEU_TEXT3);
    display.fillRect(cx - 50, by - 20, 100, 20, NEU_ACCENT);
    prkProbeTool(cx, by - 20, 50);
    const dx = cx + 66;
    display.drawFastVLine(dx, by - 20, 20, NEU_TEXT2); display.drawFastHLine(dx - 4, by - 20, 9, NEU_TEXT2); display.drawFastHLine(dx - 4, by - 1, 9, NEU_TEXT2);
    neuSmall(fmtF(pendantProbeV2.plateThick, 2), dx + 6, by - 14, NEU_ACCENT);
    neuCaption("Turn the dial to adjust", cx, d.y + 12, NEU_TEXT3, "C");
  }
}

function drawProbeCfgPlateScreen() {
  const xyz = pendantProbeV2.probeTypeIdx === PROBE_TYPE_XYZPLATE;
  neuHeader("Plate Setup");
  neuCaption(xyz ? "XYZ touch plate" : "Z touch plate", 12, 40);
  plateFields().forEach((f) => neuInset(f.x, f.y, f.w, f.h, 8, 3));
  updateProbeCfgPlateFields();
  drawPlateDiagram();
  neuNav(PROBE_CFG_NAV);
}

function handleProbeCfgPlateTouch(x, y) {
  if (_cfgFieldTouch(plateFields(), x, y, updateProbeCfgPlateFields)) return;
  _cfgNavTouch(x, y);
}
