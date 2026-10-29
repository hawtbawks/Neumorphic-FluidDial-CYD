/* screen_probe_bore_boss.cpp — SCR3a Bore / SCR3b Boss, neumorphic layout (see probe_kit.js)
 *
 * Both find the XY centre of a round feature and set X0/Y0 only — Z work-zero is
 * handled by the Z Surface probe.  Every wall is reached with a G38.2 two-pass
 * (fast seek + slow re-probe), never a blind rapid.  Four points are probed at
 * 90° (±X, ±Y); the centre is the midpoint of each opposed pair, which cancels
 * the tip radius per axis and needs no division.  The X pair is probed first;
 * the tool re-centres in X before the Y pair so the +Y/-Y probes run through the
 * true vertical diameter and contact head-on even from an off-centre start. */

// Two-pass probe along a unit direction (ux,uy): fast seek, back off, slow
// re-probe. Comes to rest just past the trigger (deceleration overshoot); the
// trigger point itself is in #5061/#5062, which is what we store. G91.
function probeRadial2Pass(ux, uy, seekDist, seekF, fineF) {
  const BACKOFF = 1.5;
  send_line(`G38.2 G91 X${fmtF(seekDist * ux, 3)} Y${fmtF(seekDist * uy, 3)} F${fmtF(seekF, 0)}`);
  send_line(`G0 G91 X${fmtF(-BACKOFF * ux, 3)} Y${fmtF(-BACKOFF * uy, 3)} F1000`);
  send_line(`G38.2 G91 X${fmtF((BACKOFF + 1) * ux, 3)} Y${fmtF((BACKOFF + 1) * uy, 3)} F${fmtF(fineF, 0)}`);
}

// Two-pass probe along (ux,uy), then store the along-axis machine-coord result:
// axisX -> #5061 (X wall), else #5062 (Y wall).
function probeWallStore(ux, uy, seek, seekF, fineF, storeVar, axisX) {
  probeRadial2Pass(ux, uy, seek, seekF, fineF);
  send_line(`${storeVar} = #${axisX ? "5061" : "5062"}`);
}

// Move to the found centre (machine #<xc>,#<yc>) and zero X/Y there.
function emitMoveCentreZero(pNum) {
  send_line("G53 G0 X#<xc> Y#<yc>");
  send_line(`G10 L20 P${pNum} X0 Y0`);
}

// ===== BORE =====
function enterProbeBore() {
  pendantProbeV2.focusedField = 0;
  pendantProbeV2.confirmActive = false;
  pendantProbeV2.dialAccelCount = 0;
}
function exitProbeBore() {}

function runProbeBore() {
  if (!pendantConnected) return;
  const p = pendantProbeV2;
  const pNum = pendantProbing.selectedCoordIndex + 1;
  const seekF = p.seekRate, fineF = p.probeRate, rad = p.boreDia / 2, wallOf = p.boreOffset;
  const d = 2 * rad + wallOf + 3;   // generous outward seek
  probeActivateWcs();          // zero into the system shown on screen
  send_line("G21 G90");
  send_line("#<sx> = #5420");
  send_line("#<sy> = #5421");
  // X pair along Y = start, average to the centre X, then re-centre X.
  probeWallStore(1.0, 0.0, d, seekF, fineF, "#<ax>", true);
  send_line("G90 G0 X#<sx> Y#<sy> F1000");
  probeWallStore(-1.0, 0.0, d, seekF, fineF, "#<cx>", true);
  send_line("G90 G0 X#<sx> Y#<sy> F1000");
  send_line("#<xc> = [[#<ax> + #<cx>] / 2]");
  send_line("G53 G0 X#<xc>");                   // re-centre X (Y stays at start)
  // Y pair through the centred X (true vertical diameter).
  probeWallStore(0.0, 1.0, d, seekF, fineF, "#<by>", false);
  send_line("G90 G0 Y#<sy> F1000");             // Y back to start (X stays centred)
  probeWallStore(0.0, -1.0, d, seekF, fineF, "#<dy>", false);
  send_line("#<yc> = [[#<by> + #<dy>] / 2]");
  emitMoveCentreZero(pNum);
}

const PB_FIELDS = [{ x: PRK_RX, y: 70, w: PRK_RW, h: 46 }, { x: PRK_RX, y: 124, w: PRK_RW, h: 46 }];
const PB_RESULT = { x: PRK_RX, y: 178, w: PRK_RW, h: 72 };

// Top-down: the bore in the stock, centre mark, outward probe moves.
function drawBoreDiagram() {
  const { x, y } = PRK_DIAG, cx = x + 55, cy = y + 38;
  display.fillRect(x + 14, y + 12, 82, 52, NEU_TEXT3);
  display.fillCircle(cx, cy, 20, NEU_WELL);
  display.drawCircle(cx, cy, 20, NEU_TEXT2);
  prkArrowLeft(cx - 17, cy, 14, NEU_JOG); prkArrowRight(cx + 3, cy, 14, NEU_JOG);
  display.fillCircle(cx, cy, 3, NEU_ACCENT);
}

function updateProbeBoreFields() {
  if (currentPendantScreen !== PSCREEN_PROBE_BORE) return;
  const p = pendantProbeV2, fo = p.focusedField, a = prkLen(p.boreDia, 1), b = prkLen(p.boreOffset, 1);
  neuFieldValue(PB_FIELDS[0].x, PB_FIELDS[0].y, PRK_RW, 46, "Nominal dia", a[0], a[1], { focus: fo === 0 });
  neuFieldValue(PB_FIELDS[1].x, PB_FIELDS[1].y, PRK_RW, 46, "Wall offset", b[0], b[1], { focus: fo === 1 });
}

function drawProbeBoreScreen() {
  neuHeader("Bore");
  prkPosStrip(true);
  prkSequence(["Probe 4 walls", "Find centre", "Set X0 Y0"]);
  prkDiagramWell();
  drawBoreDiagram();
  PB_FIELDS.forEach((f) => neuInset(f.x, f.y, f.w, f.h, 8, 3));
  updateProbeBoreFields();
  prkResultCard(PB_RESULT, "X0 Y0");
  prkHint("Place the tip inside the bore");
  neuNav(PROBE_ROUTINE_NAV);
  if (pendantProbeV2.confirmActive) prkConfirm("Bore centre (Z unchanged)");
}

function updateProbeBoreScreen() {
  if (currentPendantScreen !== PSCREEN_PROBE_BORE || pendantProbeV2.confirmActive) return;
  prkPosStrip(false);
}

function handleProbeBoreTouch(x, y) {
  prkHandleTouch(x, y, { name: "Bore centre (Z unchanged)", fields: PB_FIELDS, resultRect: PB_RESULT, resultWhat: "X0 Y0",
    run: runProbeBore, redraw: drawProbeBoreScreen, redrawFields: updateProbeBoreFields });
}

// ===== BOSS =====
function enterProbeBoss() {
  pendantProbeV2.focusedField = 0;
  pendantProbeV2.confirmActive = false;
  pendantProbeV2.dialAccelCount = 0;
}
function exitProbeBoss() {}

// One boss wall: move clear along (ux,uy) at safe Z, plunge beside the boss,
// two-pass probe INWARD (-ux,-uy), store the along-axis result, then lift.
function bossWallStore(ux, uy, out, inSeek, plunge, seekF, fineF, storeVar, axisX) {
  send_line(`G0 G91 X${fmtF(out * ux, 3)} Y${fmtF(out * uy, 3)} F1000`);
  send_line(`G0 G91 Z-${fmtF(plunge, 3)} F500`);
  probeWallStore(-ux, -uy, inSeek, seekF, fineF, storeVar, axisX);   // inward
  send_line(`G0 G91 Z${fmtF(plunge, 3)} F500`);                      // lift
}

function runProbeBoss() {
  if (!pendantConnected) return;
  const p = pendantProbeV2;
  const pNum = pendantProbing.selectedCoordIndex + 1;
  const seekF = p.seekRate, fineF = p.probeRate, depth = p.bossDepth, clear = p.bossClear;
  const retZ = p.retractDist, maxZ = p.maxZTravel;
  const platZ = probeIs3D() ? probeTipOffset3D() : p.plateThick;
  // Half-size per axis. Circular: both = bossDia/2. Rect: X=bossDia/2, Y=bossSizeY/2.
  const radX = p.bossDia / 2;
  const radY = (p.bossRect ? p.bossSizeY : p.bossDia) / 2;
  const outX = radX + clear, inSeekX = clear + radX + 5;
  const outY = radY + clear, inSeekY = clear + radY + 5;
  const plunge = depth + retZ;
  probeActivateWcs();          // zero into the system shown on screen
  send_line("G21 G90");
  send_line("#<sx> = #5420");
  send_line("#<sy> = #5421");
  // Touch the top and set Z0 there (the boss top is the Z datum), then lift.
  send_line("G91");
  send_line(`G38.2 Z-${fmtF(maxZ, 3)} F${fmtF(fineF, 0)}`);
  send_line("G90");
  probeReturnToTrigger("Z");   // undo the deceleration overshoot before zeroing
  send_line(`G10 L20 P${pNum} Z${fmtF(platZ, 3)}`);
  send_line("G91");
  send_line(`G0 Z${fmtF(retZ, 3)} F500`);
  send_line("G90");
  // X pair along Y = start, average to the centre X, then re-centre X.
  bossWallStore(1.0, 0.0, outX, inSeekX, plunge, seekF, fineF, "#<ax>", true);
  send_line("G90 G0 X#<sx> Y#<sy> F1000");
  bossWallStore(-1.0, 0.0, outX, inSeekX, plunge, seekF, fineF, "#<cx>", true);
  send_line("G90 G0 X#<sx> Y#<sy> F1000");
  send_line("#<xc> = [[#<ax> + #<cx>] / 2]");
  send_line("G53 G0 X#<xc>");                   // re-centre X at safe Z (Y at start)
  // Y pair through the centred X.
  bossWallStore(0.0, 1.0, outY, inSeekY, plunge, seekF, fineF, "#<by>", false);
  send_line("G90 G0 Y#<sy> F1000");             // Y back to start (X stays centred)
  bossWallStore(0.0, -1.0, outY, inSeekY, plunge, seekF, fineF, "#<dy>", false);
  send_line("#<yc> = [[#<by> + #<dy>] / 2]");
  emitMoveCentreZero(pNum);   // sets X0 Y0; Z0 already set at the top
}

const PS_SEQ = { x: 10, y: 70, w: 110, h: 72 }, PS_DIAG = { x: 10, y: 150, w: 110, h: 52 }, PS_RESULT = { x: 10, y: 210, w: 110, h: 40 };
const PS_SHAPE = { x: PRK_RX, y: 70, w: PRK_RW, h: 32 };
function bossFields() {
  return pendantProbeV2.bossRect
    ? [0, 1, 2, 3].map((i) => ({ x: PRK_RX, y: 110 + i * 36, w: PRK_RW, h: 34 }))
    : [0, 1, 2].map((i) => ({ x: PRK_RX, y: 110 + i * 47, w: PRK_RW, h: 42 }));
}

// Top-down: round or rectangular boss with four inward probe moves.
function drawBossDiagram() {
  const { x, y } = PS_DIAG, cx = x + 55, cy = y + 26;
  if (pendantProbeV2.bossRect) display.fillRect(cx - 18, cy - 12, 37, 25, NEU_TEXT3);
  else { display.fillCircle(cx, cy, 14, NEU_TEXT3); }
  prkArrowRight(cx - 40, cy, 14, NEU_JOG); prkArrowLeft(cx + 26, cy, 14, NEU_JOG);
  display.fillCircle(cx, cy, 3, NEU_ACCENT);
}

// Shape picker: two segments drawn as a circle and a square (no room for words at this width).
function drawBossShapePicker() {
  const sel = pendantProbeV2.bossRect ? 1 : 0;
  neuClear(PS_SHAPE.x, PS_SHAPE.y, PS_SHAPE.w, PS_SHAPE.h);
  neuSegmented(PS_SHAPE.x, PS_SHAPE.y, PS_SHAPE.w, PS_SHAPE.h, ["", ""], sel, { r: 8 });
  const segs = neuSegLayout(PS_SHAPE.x, PS_SHAPE.w, 2), cy = PS_SHAPE.y + (PS_SHAPE.h / 2 | 0);
  const c0 = segs[0].x + (segs[0].w / 2 | 0), c1 = segs[1].x + (segs[1].w / 2 | 0);
  const col = (i) => (i === sel ? NEU_ACCENT : NEU_TEXT3);
  display.drawCircle(c0, cy, 7, col(0)); display.drawCircle(c0, cy, 6, col(0));
  display.drawRect(c1 - 7, cy - 6, 15, 13, col(1)); display.drawRect(c1 - 6, cy - 5, 13, 11, col(1));
}

function updateProbeBossFields() {
  if (currentPendantScreen !== PSCREEN_PROBE_BOSS) return;
  const p = pendantProbeV2, fo = p.focusedField, F = bossFields();
  const vals = p.bossRect
    ? [["X size", p.bossDia], ["Y size", p.bossSizeY], ["Depth", p.bossDepth], ["Clearance", p.bossClear]]
    : [["Boss dia", p.bossDia], ["Depth", p.bossDepth], ["Clearance", p.bossClear]];
  vals.forEach(([c, v], i) => { const L = prkLen(v, 1); neuFieldValue(F[i].x, F[i].y, PRK_RW, F[i].h, c, L[0], L[1], { focus: fo === i, font: "B9" }); });
}

function drawProbeBossScreen() {
  neuHeader("Boss");
  prkPosStrip(true);
  prkSequence(["Touch top: Z0", "Probe 4 sides", "Set X0 Y0"], PS_SEQ);
  prkDiagramWell(PS_DIAG);
  drawBossDiagram();
  prkResultCard(PS_RESULT, "XYZ");
  drawBossShapePicker();
  bossFields().forEach((f) => neuInset(f.x, f.y, f.w, f.h, 8, 3));
  updateProbeBossFields();
  prkHint("Start above the boss centre");
  neuNav(PROBE_ROUTINE_NAV);
  if (pendantProbeV2.confirmActive) prkConfirm((pendantProbeV2.bossRect ? "Rectangular" : "Round") + " boss");
}

function updateProbeBossScreen() {
  if (currentPendantScreen !== PSCREEN_PROBE_BOSS || pendantProbeV2.confirmActive) return;
  prkPosStrip(false);
}

function handleProbeBossTouch(x, y) {
  const p = pendantProbeV2;
  prkHandleTouch(x, y, { name: (p.bossRect ? "Rectangular" : "Round") + " boss", fields: bossFields(), resultRect: PS_RESULT, resultWhat: "XYZ",
    run: runProbeBoss, redraw: drawProbeBossScreen, redrawFields: updateProbeBossFields,
    extra: (tx, ty) => {
      const seg = neuSegHit(PS_SHAPE.x, PS_SHAPE.y, PS_SHAPE.w, PS_SHAPE.h, 2, tx, ty);
      if (seg < 0 || (seg === 1) === p.bossRect) return seg >= 0;
      p.bossRect = seg === 1;
      if (p.focusedField > (p.bossRect ? 3 : 2)) p.focusedField = -1;
      saveProbeSettings();
      drawProbeBossScreen();
      return true;
    } });
}
