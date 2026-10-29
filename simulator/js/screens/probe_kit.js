/* Shared layout for the probe routine screens (Z Surface, Corner, Bore, Boss) and
 * the probe config screens.  Mirrors src/screens/screen_probe.cpp's shared helpers.
 *
 *   position strip  y 38  h 24   live work X / Y / Z
 *   left column     x 10  w 110  sequence well (y 70 h 96) + diagram well (y 174 h 76)
 *   right column    x 128 w 102  setting fields, then the result card (tap = next WCS)
 *   hint            y 258        amber reminder, red when something blocks probing
 *   nav             Home | Back (probe hub) | Probe
 */

const PRK_POS_Y = 38, PRK_POS_H = 24;
const PRK_SEQ = { x: 10, y: 70, w: 110, h: 96 }, PRK_DIAG = { x: 10, y: 174, w: 110, h: 76 };
const PRK_RX = 128, PRK_RW = 102, PRK_HINT_Y = 258;
const PROBE_ROUTINE_NAV = [NAV_HOME, NAV_BACK, { id: "probe", label: "Probe", icon: "play", color: NEU_RUN }];
let _prkPosKey = "";
let _prkDialog = null;

function prkPosStrip(force) {
  const m = pendantMachine, key = m.wpos.slice(0, 3).join() + m.inInches;
  if (!force && key === _prkPosKey) return;
  _prkPosKey = key;
  if (force) neuInset(10, PRK_POS_Y, 220, PRK_POS_H, 8, 2);
  display.fillRect(13, PRK_POS_Y + 3, 215, PRK_POS_H - 5, NEU_WELL);
  const dec = m.inInches ? 3 : 2;
  for (let i = 0; i < 3; i++) {
    const x = 20 + i * 72;
    neuCaption(AXIS_NAMES[i], x, PRK_POS_Y + 8, NEU_AX[i]);
    neuSmall(fmtF(m.wpos[i], dec), x + 64, PRK_POS_Y + 8, NEU_TEXT2, "R");
  }
}

// Numbered sequence — describes what the routine will do, in order.  rect defaults to PRK_SEQ.
function prkSequence(steps, r = PRK_SEQ) {
  const { x, y, w, h } = r, pitch = h >= 90 ? 22 : 17;
  neuInset(x, y, w, h, 10, 3);
  neuCaption("Sequence", x + 10, y + 9);
  steps.forEach((s, i) => {
    const yy = y + (h >= 90 ? 28 : 24) + i * pitch;
    display.drawCircle(x + 16, yy + 3, 6, i === 0 ? NEU_ACCENT : NEU_TEXT3);
    neuSmall(String(i + 1), x + 14, yy, i === 0 ? NEU_ACCENT : NEU_TEXT3);
    neuSmall(s, x + 27, yy, i === 0 ? NEU_TEXT : NEU_TEXT2);
  });
}

function prkDiagramWell(r = PRK_DIAG) { neuInset(r.x, r.y, r.w, r.h, 10, 3); }

// Result card: what the routine sets, and in which work system.  Tap to cycle G54..G57.
// rect {x,y,w,h}.  Tall cards (h >= 60) show "Sets <what> in" above the WCS; short ones put it on one line.
function prkResultCard(r, what) {
  neuInset(r.x, r.y, r.w, r.h, 10, 3);
  prkResultValue(r, what);
}
function prkResultValue(r, what) {
  const { x, y, w, h } = r;
  display.fillRect(x + 4, y + 4, w - 7, h - 7, NEU_WELL);
  if (h >= 60) {
    neuCaption("Result", x + 10, y + 9);
    neuSmall("Sets " + what + " in", x + 10, y + 24, NEU_TEXT2);
    neuText(pendantProbing.selectedCoordSystem, x + 10, y + h - 9, "B12", NEU_RUN);
    neuCaption("tap", x + w - 8, y + h - 16, NEU_TEXT3, "R");
  } else {
    neuCaption("Sets " + what, x + 10, y + 8);
    neuText(pendantProbing.selectedCoordSystem, x + 10, y + h - 7, "B9", NEU_RUN);
    neuCaption("tap", x + w - 8, y + h - 15, NEU_TEXT3, "R");
  }
}

function prkHint(msg, red) { neuHint(msg, PRK_HINT_Y, red ? NEU_ALARM : NEU_HOLD); }

function prkConfirm(routine) {
  _prkDialog = neuDialog("Start probing?", [routine, "Zeroes into " + pendantProbing.selectedCoordSystem], "Cancel", "Probe", NEU_RUN);
}

// Common touch handling for a routine screen.  Returns true when handled.
//   fields: [{x,y,w,h}] in focus-index order;  resultRect: {y,h};  run(): sends the G-code;
//   redraw(): full repaint;  redrawFields(): fields only.
function prkHandleTouch(x, y, o) {
  const p = pendantProbeV2;
  if (p.confirmActive) {
    if (neuDialogHit(_prkDialog.cancel, x, y)) { p.confirmActive = false; o.redraw(); }
    else if (neuDialogHit(_prkDialog.ok, x, y)) { p.confirmActive = false; o.run(); currentPendantScreen = PSCREEN_STATUS; }
    return true;
  }
  for (let i = 0; i < o.fields.length; i++) {
    const f = o.fields[i];
    if (isTouchInBounds(x, y, f.x, f.y, f.w, f.h)) { p.focusedField = p.focusedField === i ? -1 : i; o.redrawFields(); return true; }
  }
  const rr = o.resultRect;
  if (rr && isTouchInBounds(x, y, rr.x, rr.y, rr.w, rr.h)) {
    probeCycleWorkArea(); prkResultValue(rr, o.resultWhat); return true;
  }
  if (o.extra && o.extra(x, y)) return true;
  const n = neuNavHit(PROBE_ROUTINE_NAV, x, y);
  if (n === "home") { currentPendantScreen = PSCREEN_MAIN_MENU; return true; }
  if (n === "back") { p.returnScreen = currentPendantScreen; currentPendantScreen = PSCREEN_PROBE; return true; }
  if (n === "probe") {
    if (!pendantConnected) { prkHint("Not connected", true); return true; }
    p.confirmActive = true; prkConfirm(o.name); return true;
  }
  return false;
}

// ---- diagram parts (drawn inside the diagram well) ----
function prkProbeTool(cx, tipY, len) {
  display.fillRoundRect(cx - 5, tipY - len - 8, 11, 9, 2, NEU_TEXT3);
  display.fillRect(cx - 1, tipY - len, 3, len - 3, NEU_TEXT2);
  display.fillCircle(cx, tipY - 2, 3, NEU_ACCENT);
}
function prkArrowDown(x, y, len, c) { display.fillRect(x, y, 2, len, c); for (let i = 0; i < 4; i++) display.drawFastHLine(x - 3 + i, y + len + i - 2, 8 - 2 * i, c); }
function prkArrowLeft(x, y, len, c) { display.fillRect(x, y, len, 2, c); for (let i = 0; i < 4; i++) display.drawFastVLine(x + i - 2, y - 3 + i, 8 - 2 * i, c); }
function prkArrowRight(x, y, len, c) { display.fillRect(x, y, len, 2, c); for (let i = 0; i < 4; i++) display.drawFastVLine(x + len - i + 1, y - 3 + i, 8 - 2 * i, c); }
