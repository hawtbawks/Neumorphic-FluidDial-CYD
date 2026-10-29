/* screen_probe_z.cpp — SCR1 Z surface probe, neumorphic layout (see probe_kit.js) */

const PZ_FIELDS = [{ x: PRK_RX, y: 70, w: PRK_RW, h: 46 }, { x: PRK_RX, y: 124, w: PRK_RW, h: 46 }];
const PZ_RESULT = { x: PRK_RX, y: 178, w: PRK_RW, h: 72 };

function enterProbeZ() {
  pendantProbeV2.focusedField = 0;
  pendantProbeV2.confirmActive = false;
  pendantProbeV2.dialAccelCount = 0;
}
function exitProbeZ() { pendantProbeV2.focusedField = -1; }

function runProbeZ() {
  if (!pendantConnected) return;
  const pNum = pendantProbing.selectedCoordIndex + 1;
  const zOffset = probeIs3D() ? probeTipOffset3D() : pendantProbeV2.plateThick;
  probeActivateWcs();          // zero into the system shown on screen
  send_line("G91 G21");
  probeSeekFine("Z", -pendantProbeV2.maxZTravel, pendantProbeV2.seekRate, pendantProbeV2.probeRate);
  send_line("G90");
  probeReturnToTrigger("Z");   // undo the deceleration overshoot before zeroing
  send_line(`G10 L20 P${pNum} Z${fmtF(zOffset, 3)}`);
  send_line("G91");
  send_line(`G0 Z${fmtF(pendantProbeV2.retractDist, 3)}`);
  send_line("G90");
}

// Values are stored in mm; inch mode shows them converted.
function prkLen(mm, decMm) { return pendantMachine.inInches ? [fmtF(mm / 25.4, 3), "in"] : [fmtF(mm, decMm), "mm"]; }

function updateProbeZFields() {
  if (currentPendantScreen !== PSCREEN_PROBE_Z) return;
  const p = pendantProbeV2, fo = p.focusedField;
  const a = prkLen(p.maxZTravel, 0), b = prkLen(p.retractDist, 0);
  neuFieldValue(PZ_FIELDS[0].x, PZ_FIELDS[0].y, PRK_RW, 46, "Max travel", a[0], a[1], { focus: fo === 0 });
  neuFieldValue(PZ_FIELDS[1].x, PZ_FIELDS[1].y, PRK_RW, 46, "Retract", b[0], b[1], { focus: fo === 1 });
}

function drawProbeZScreen() {
  neuHeader("Z Surface");
  prkPosStrip(true);
  prkSequence(["Fast seek -Z", "Slow probe", "Set Z0"]);
  prkDiagramWell();
  display.fillRect(22, 228, 86, 14, NEU_TEXT3);
  if (!probeIs3D()) display.fillRect(40, 222, 50, 6, NEU_ACCENT);   // the plate
  prkProbeTool(65, probeIs3D() ? 228 : 222, 24);
  prkArrowDown(36, 186, 26, NEU_JOG);
  PZ_FIELDS.forEach((f) => neuInset(f.x, f.y, f.w, f.h, 8, 3));
  updateProbeZFields();
  prkResultCard(PZ_RESULT, "Z0");
  prkHint(probeIs3D() ? "Probe connected?" : "Plate clip connected?");
  neuNav(PROBE_ROUTINE_NAV);
  if (pendantProbeV2.confirmActive) prkConfirm("Z Surface");
}

function updateProbeZScreen() {
  if (currentPendantScreen !== PSCREEN_PROBE_Z || pendantProbeV2.confirmActive) return;
  prkPosStrip(false);
}

function handleProbeZTouch(x, y) {
  prkHandleTouch(x, y, { name: "Z Surface", fields: PZ_FIELDS, resultRect: PZ_RESULT, resultWhat: "Z0",
    run: runProbeZ, redraw: drawProbeZScreen, redrawFields: updateProbeZFields });
}
