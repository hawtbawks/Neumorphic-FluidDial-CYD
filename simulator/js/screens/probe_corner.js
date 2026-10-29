/* screen_probe_corner.cpp — SCR2 XYZ corner probe, neumorphic layout (see probe_kit.js)
 *   left: sequence, top-down diagram of the chosen corner, result card (tap = next WCS)
 *   right: 2x2 corner picker laid out like the corners, then depth / overshoot / XY retract
 */

function enterProbeCorner() {
  pendantProbeV2.focusedField = 0;
  pendantProbeV2.confirmActive = false;
  pendantProbeV2.dialAccelCount = 0;
}
function exitProbeCorner() {}

function runProbeCorner() {
  if (!pendantConnected) return;
  const p = pendantProbeV2;
  const pNum = pendantProbing.selectedCoordIndex + 1;
  const rate = p.probeRate, seekF = p.seekRate, depth = p.cornerDepth, over = p.cornerOver, retXY = p.cornerRetXY;
  const retZ = p.retractDist, maxZ = p.maxZTravel;
  const is3D = probeIs3D();
  const platZ = is3D ? probeTipOffset3D() : p.plateThick;
  // XY edge comp: ball radius (3D) or the XYZ plate's wall thickness per axis.
  const edgeOfsX = is3D ? probeTipOffset3D() : p.plateOffX;
  const edgeOfsY = is3D ? probeTipOffset3D() : p.plateOffY;
  const cIdx = p.cornerIdx;
  const xDir = cIdx === 0 || cIdx === 2 ? 1 : -1;
  const yDir = cIdx === 0 || cIdx === 1 ? 1 : -1;
  const doXY = true;   // corner probe always does X/Y/Z
  const doZ = true;
  probeActivateWcs();          // zero into the system shown on screen
  if (doZ) {
    send_line("G91 G21");
    probeSeekFine("Z", -maxZ, seekF, rate);   // two-pass surface touch
    send_line("G90");
    probeReturnToTrigger("Z");   // undo the deceleration overshoot before zeroing
    send_line(`G10 L20 P${pNum} Z${fmtF(platZ, 3)}`);
    send_line("G91");
    send_line(`G0 Z${fmtF(retZ, 3)} F1000`);
  }
  if (doXY) {
    const dropZ = doZ ? depth + retZ : depth;
    send_line("G91");
    send_line(`G0 Z-${fmtF(dropZ, 3)} F500`);
    send_line(`G0 X${fmtF(-xDir * over, 3)} F1000`);
    probeSeekFine("X", xDir * (over + 20), seekF, rate);   // two-pass toward X wall
    send_line("G90");
    probeReturnToTrigger("X");   // undo the deceleration overshoot before zeroing
    send_line(`G10 L20 P${pNum} X${fmtF(xDir > 0 ? -edgeOfsX : edgeOfsX, 3)}`);
    send_line("G91");
    send_line(`G0 X${fmtF(-xDir * retXY, 3)} F1000`);
    send_line(`G0 Y${fmtF(-yDir * over, 3)} F1000`);
    probeSeekFine("Y", yDir * (over + 20), seekF, rate);   // two-pass toward Y wall
    send_line("G90");
    probeReturnToTrigger("Y");   // undo the deceleration overshoot before zeroing
    send_line(`G10 L20 P${pNum} Y${fmtF(yDir > 0 ? -edgeOfsY : edgeOfsY, 3)}`);
    send_line("G91");
    send_line(`G0 Y${fmtF(-yDir * retXY, 3)} F1000`);
    send_line(`G0 Z${fmtF(depth, 3)} F500`);
    send_line("G90");
  }
}

const cornerLabels = ["Bot-Left", "Bot-Right", "Top-Left", "Top-Right"];
// 2x2 picker laid out like the corners themselves: top row TL TR, bottom row BL BR.
const CORNER_GRID = [[2, "TL"], [3, "TR"], [0, "BL"], [1, "BR"]];   // [cornerIdx, label] in drawing order
const PC_PICK = { x: PRK_RX, y: 70, w: PRK_RW, h: 46 };
const PC_FIELDS = [{ x: PRK_RX, y: 124, w: PRK_RW, h: 38 }, { x: PRK_RX, y: 168, w: PRK_RW, h: 38 }, { x: PRK_RX, y: 212, w: PRK_RW, h: 38 }];
const PC_SEQ = { x: 10, y: 70, w: 110, h: 72 }, PC_DIAG = { x: 10, y: 150, w: 110, h: 52 }, PC_RESULT = { x: 10, y: 210, w: 110, h: 40 };

function _cornerCell(k) { return { x: PC_PICK.x + 5 + (k % 2) * 47, y: PC_PICK.y + 5 + (k >> 1) * 19, w: 45, h: 17 }; }
function drawCornerPicker() {
  neuInset(PC_PICK.x, PC_PICK.y, PC_PICK.w, PC_PICK.h, 10, 3);
  CORNER_GRID.forEach(([idx, lab], k) => {
    const c = _cornerCell(k), on = idx === pendantProbeV2.cornerIdx;
    if (on) neuRaised(c.x, c.y, c.w, c.h, 6, 1);
    neuSmall(lab, c.x + (c.w / 2 | 0), c.y + 5, on ? NEU_ACCENT : NEU_TEXT3, "C");
  });
}

// Top-down view (screen up = +Y): stock, the chosen corner, and the probe approaching both edges.
function drawCornerDiagram() {
  const { x, y } = PC_DIAG, ci = pendantProbeV2.cornerIdx;
  const stockRight = ci === 0 || ci === 2;    // BL / TL: stock lies to +X of the corner
  const stockUp = ci === 0 || ci === 1;       // BL / BR: stock lies to +Y (screen up)
  const bx = stockRight ? x + 52 : x + 12, by = stockUp ? y + 8 : y + 22;
  display.fillRect(bx, by, 46, 22, NEU_TEXT3);
  const cx = stockRight ? bx : bx + 45, cy = stockUp ? by + 21 : by;
  // X approach along the stock's mid-height, from outside the corner side
  const my = by + 10;
  if (stockRight) prkArrowRight(cx - 20, my, 14, NEU_JOG); else prkArrowLeft(cx + 6, my, 14, NEU_JOG);
  // Y approach along the stock's mid-width
  const mx = bx + 22;
  if (stockUp) { display.fillRect(mx, cy + 5, 2, 9, NEU_JOG); for (let i = 0; i < 4; i++) display.drawFastHLine(mx - 3 + i, cy + 4 - i + 1, 8 - 2 * i, NEU_JOG); }
  else { display.fillRect(mx, cy - 14, 2, 9, NEU_JOG); for (let i = 0; i < 4; i++) display.drawFastHLine(mx - 3 + i, cy - 6 + i, 8 - 2 * i, NEU_JOG); }
  display.fillCircle(cx, cy, 3, NEU_ACCENT);
}

function updateProbeCornerFields() {
  if (currentPendantScreen !== PSCREEN_PROBE_CORNER) return;
  const p = pendantProbeV2, fo = p.focusedField;
  const vals = [["Depth", p.cornerDepth], ["Overshoot", p.cornerOver], ["XY retract", p.cornerRetXY]];
  vals.forEach(([c, v], i) => { const L = prkLen(v, 1); neuFieldValue(PC_FIELDS[i].x, PC_FIELDS[i].y, PRK_RW, 38, c, L[0], L[1], { focus: fo === i, font: "B9" }); });
}

function drawProbeCornerScreen() {
  neuHeader("XYZ Corner");
  prkPosStrip(true);
  prkSequence(["Touch top: Z0", "Probe X & Y", "Set X0 Y0"], PC_SEQ);
  prkDiagramWell(PC_DIAG);
  drawCornerDiagram();
  prkResultCard(PC_RESULT, "XYZ");
  drawCornerPicker();
  PC_FIELDS.forEach((f) => neuInset(f.x, f.y, f.w, f.h, 8, 3));
  updateProbeCornerFields();
  prkHint("Probe above the corner");
  neuNav(PROBE_ROUTINE_NAV);
  if (pendantProbeV2.confirmActive) prkConfirm("XYZ Corner, " + cornerLabels[pendantProbeV2.cornerIdx]);
}

function updateProbeCornerScreen() {
  if (currentPendantScreen !== PSCREEN_PROBE_CORNER || pendantProbeV2.confirmActive) return;
  prkPosStrip(false);
}

function handleProbeCornerTouch(x, y) {
  prkHandleTouch(x, y, { name: "XYZ Corner, " + cornerLabels[pendantProbeV2.cornerIdx], fields: PC_FIELDS, resultRect: PC_RESULT, resultWhat: "XYZ",
    run: runProbeCorner, redraw: drawProbeCornerScreen, redrawFields: updateProbeCornerFields,
    extra: (tx, ty) => {
      if (pendantProbeV2.confirmActive || !isTouchInBounds(tx, ty, PC_PICK.x, PC_PICK.y, PC_PICK.w, PC_PICK.h)) return false;
      for (let k = 0; k < 4; k++) {
        const c = _cornerCell(k);
        if (isTouchInBounds(tx, ty, c.x - 1, c.y - 1, c.w + 2, c.h + 2)) {
          pendantProbeV2.cornerIdx = CORNER_GRID[k][0];
          drawCornerPicker();
          display.fillRect(PC_DIAG.x + 4, PC_DIAG.y + 4, PC_DIAG.w - 7, PC_DIAG.h - 7, NEU_WELL);
          drawCornerDiagram();
          return true;
        }
      }
      return true;
    } });
}
