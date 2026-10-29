/*
 * screens.js — neumorphic mockups of every FluidDial-CYD screen.
 * Static: each function paints one screen with representative data.
 *
 * Layout grid (240x320 portrait):
 *   header   y 0..29    title (FreeSansBold9) left, link + battery + state chip right
 *   groove   y 30
 *   content  y 36..276  10px side margins, 8px gaps (shadows need 3px each side)
 *   groove   y 278
 *   nav bar  y 284..316 raised buttons, h 32 — Home is always bottom-left
 */

const M = {   // mock machine
  state: "Idle", wpos: [125.4, 48.25, -2.0], mpos: [-174.6, -251.75, -42.0], units: "mm", wcs: "G54",
  feed: 0, rpm: 0, link: 3, battery: 78, charging: false,
};
const AXC = [NEU.AX_X, NEU.AX_Y, NEU.AX_Z];
const fmt = (v, d = 3) => (v < 0 ? "-" : "") + Math.abs(v).toFixed(d);

// ---------------- chrome ----------------
function bg() { G.fillScreen(NEU.BG); }
function stateChip(state, xr = 232) {
  const label = String(state).split(":")[0].toUpperCase();
  const w = tw(label, "B9") + 26, x = xr - w;
  inset(x, 5, w, 20, 10, 2);
  led(x + 11, 15, stateColor(state), 3);
  text(label, x + 19, capBase(5, 20, "B9"), "B9", stateColor(state));
  return x;
}
function linkIcon(xr, bars) {           // 4 bars, 13x10
  const x = xr - 13;
  for (let i = 0; i < 4; i++) { const h = 3 + i * 2.5 | 0; G.fillRect(x + i * 3 + (i ? i : 0) - i, 21 - h, 2, h, i < bars ? NEU.TEXT2 : NEU.SH_LIGHT); }
  return x;
}
function batteryIcon(xr, pct, charging) {
  const x = xr - 20, y = 10;
  G.drawRoundRect(x, y, 17, 10, 2, NEU.TEXT3); G.fillRect(x + 17, y + 3, 2, 4, NEU.TEXT3);
  const col = pct > 50 ? NEU.TEXT2 : pct > 20 ? NEU.HOLD : NEU.ALARM;
  G.fillRect(x + 2, y + 2, Math.round(13 * pct / 100), 6, col);
  if (charging) { G.fillRect(x + 7, y + 1, 2, 4, NEU.HOLD); G.fillRect(x + 6, y + 4, 4, 2, NEU.HOLD); G.fillRect(x + 7, y + 5, 2, 4, NEU.HOLD); }
  return x;
}
function header(title, o = {}) {
  bg();
  const chipX = stateChip(o.state || M.state);
  const titleEnd = 10 + tw(title, "B9") + 8;
  let x = chipX;
  if (x - 26 >= titleEnd) x = batteryIcon(x - 6, M.battery, M.charging);   // icons yield to the title
  if (x - 19 >= titleEnd) linkIcon(x - 6, M.link);
  text(fit(title, "B9", chipX - 18), 10, capBase(5, 20, "B9"), "B9", NEU.TEXT);
  groove(30);
}
// nav(items): items = [{icon, label, primary, w}]; Home first by convention.
function nav(items) {
  groove(278);
  const y = 285, h = 30, gap = 8, x0 = 10, W = 220;
  const fixed = items.reduce((a, it) => a + (it.w || 0), 0), flex = items.filter(i => !i.w).length;
  const fw = flex ? ((W - fixed - gap * (items.length - 1)) / flex) | 0 : 0;
  let x = x0;
  items.forEach((it, i) => {
    let w = it.w || fw; if (i === items.length - 1) w = x0 + W - x;
    const col = it.color || (it.primary ? NEU.ACCENT : NEU.TEXT2);
    button(x, y, w, h, it.label || "", { icon: it.icon ? ICON[it.icon] : null, color: col, on: it.on, face: it.face, bar: false });
    x += w + gap;
  });
}
const HOME = { icon: "home", w: 40 };
const BACK = { icon: "back", w: 40 };

// Compact DRO card: rows of  axis | work (B9) | machine (caption).  h 68.
function dro(x, y, showHead = true) {
  inset(x, y, 220, 68, 12, 3);
  caption("WORK " + M.wcs, x + 132, y + 7, NEU.TEXT3, "R");
  caption("MACH", x + 208, y + 7, NEU.TEXT3, "R");
  for (let i = 0; i < 3; i++) {
    const b = y + 31 + i * 16;
    text("XYZ"[i], x + 12, b, "B9", AXC[i]);
    text(fmt(M.wpos[i]), x + 132, b, "B9", NEU.TEXT, "R");
    caption(fmt(M.mpos[i], 2), x + 208, b - 7, NEU.TEXT3, "R");
  }
}

// ---------------- MAIN MENU ----------------
function scrMainMenu(o = {}) {
  header("FluidDial", o);
  // hero DRO card
  const st = o.state || M.state;
  if (st.startsWith("Alarm")) inset(10, 38, 220, 68, 12, 3);
  if (st.startsWith("Alarm")) {
    caption("Alarm 1", 22, 48, NEU.ALARM);
    text("Hard limit", 22, 76, "B12", NEU.ALARM);
    caption("Unlock or home to clear", 22, 87, NEU.TEXT2);
  } else dro(10, 38, false);
  // tile grid
  const tiles = [["Jog", "jog"], ["Work", "origin"], ["Feeds", "gauge"], ["Spindle", "spindle"],
                 ["Macros", "macro"], ["Files", "card"], ["Probe", "probe"], ["Status", "status"]];
  tiles.forEach(([lab, ic], i) => {
    const x = 10 + (i % 2) * 116, y = 116 + (i >> 1) * 50;
    raised(x, y, 104, 40, 12, 3);
    ICON[ic](x + 19, y + 20, NEU.ACCENT);
    text(lab, x + 35, capBase(y, 40, "B9"), "B9", NEU.TEXT);
  });
}

// ---------------- STATUS ----------------
function scrStatus(o = {}) {
  const st = o.state || "Run";
  header("Status", { state: st });
  inset(10, 38, 220, 50, 12, 3);
  led(28, 63, stateColor(st), 5);
  text(st, 42, capBase(38, 50, "B18"), "B18", stateColor(st));
  caption("ELAPSED", 218, 50, NEU.TEXT3, "R");
  text("12:41", 218, 76, "B9", NEU.TEXT, "R");
  // job
  inset(10, 96, 220, 52, 12, 3);
  caption("JOB", 22, 106); caption("LINE 1284 / 2071", 218, 106, NEU.TEXT3, "R");
  text("bracket_v3.nc", 22, 131, "B9", NEU.TEXT);
  text("62%", 218, 131, "B9", NEU.ACCENT, "R");
  G.fillRoundRect(22, 137, 196, 4, 2, NEU.SH_DARK); G.fillRoundRect(22, 137, 196 * 0.62 | 0, 4, 2, NEU.ACCENT);
  dro(10, 156);
  field(10, 232, 106, 40, "Feed", "1200", "mm/m");
  field(124, 232, 106, 40, "Spindle", "18000", "rpm");
  nav([HOME, { label: "FluidNC", icon: "gear" }]);
}

// ---------------- JOG ----------------
function scrJog(o = {}) {
  header("Jog");
  const sel = o.axis ?? 0;
  for (let i = 0; i < 3; i++) {
    const y = 38 + i * 40, on = i === sel;
    if (on) inset(10, y, 220, 34, 10, 3); else raised(10, y, 220, 34, 10, 3);
    text("XYZ"[i], 22, capBase(y, 34, "B12"), "B12", AXC[i]);
    text(fmt(M.wpos[i]), 158, capBase(y, 34, "B12"), "B12", on ? NEU.TEXT : NEU.TEXT2, "R");
    if (on) { caption("DIAL", 218, y + 9, NEU.ACCENT, "R"); G.fillRoundRect(196, y + 20, 22, 3, 1, NEU.ACCENT); }
    else caption(fmt(M.mpos[i], 1), 218, y + 13, NEU.TEXT3, "R");
  }
  caption("Step  mm", 12, 162);
  segmented(10, 174, 220, 36, ["0.01", "0.1", "1", "10"], 2);
  caption("Home", 12, 220);
  ["X", "Y", "Z", "All"].forEach((l, i) => button(10 + i * 57, 232, 49, 34, l, { color: i < 3 ? AXC[i] : NEU.HOME }));
  nav([HOME, { label: "F 5000", icon: "gauge" }, { label: "Work", icon: "origin" }]);
}

// ---------------- WORK AREA ----------------
function scrWork() {
  header("Work Area");
  segmented(10, 38, 220, 36, ["G54", "G55", "G56", "G57"], 0);
  dro(10, 84);
  caption("Set zero here", 12, 164);
  ["X", "Y", "Z", "All"].forEach((l, i) => button(10 + i * 57, 176, 49, 38, l, { color: i < 3 ? AXC[i] : NEU.TEXT }));
  caption("Go to zero", 12, 224);
  button(10, 236, 106, 34, "XY0", { icon: ICON.origin, color: NEU.TEXT2 });
  button(124, 236, 106, 34, "Z0", { icon: ICON.up, color: NEU.TEXT2 });
  nav([HOME, { label: "Jog", icon: "jog" }]);
}

// ---------------- PROBE HUB ----------------
function scrProbe(o = {}) {
  header("Probe");
  const mode = o.mode ?? 0;
  segmented(10, 38, 220, 36, ["Z Plate", "XYZ", "3D"], mode);
  caption("Shared settings", 12, 84);
  field(10, 96, 106, 40, "Probe", "150", "mm/m");
  field(124, 96, 106, 40, "Seek", "500", "mm/m");
  field(10, 144, 106, 40, "Retract", "5.0", "mm");
  field(124, 144, 106, 40, "Max Z", "40.0", "mm");
  caption("Routines", 12, 196);
  if (mode === 2) {
    ["Z Surface", "Corner", "Bore", "Boss"].forEach((l, i) =>
      button(10 + (i % 2) * 116, 208 + (i >> 1) * 36, 104, 28, l));
  } else if (mode === 1) {
    button(10, 208, 104, 60, "Z only", { icon: ICON.probe });
    button(126, 208, 104, 60, "XYZ", { icon: ICON.origin });
  } else button(10, 208, 220, 60, "Z Surface", { icon: ICON.probe, font: "B12" });
  nav([HOME, { label: "Configure", icon: "gear" }]);
}

// ---- probe routine template ----
function posStrip(y = 38) {
  inset(10, y, 220, 24, 8, 2);
  for (let i = 0; i < 3; i++) {
    const x = 20 + i * 72;
    caption("XYZ"[i], x, y + 8, AXC[i]);
    caption(fmt(M.wpos[i], 2), x + 62, y + 8, NEU.TEXT2, "R");
  }
}
function steps(x, y, w, list, cur) {
  inset(x, y, w, 96, 10, 3);
  caption("Sequence", x + 10, y + 9);
  list.forEach((s, i) => {
    const yy = y + 28 + i * 22, on = i === cur, done = i < cur;
    if (on) G.fillCircle(x + 16, yy + 3, 6, NEU.ACCENT); else G.drawCircle(x + 16, yy + 3, 6, done ? NEU.RUN : NEU.TEXT3);
    G.setFont(null); G.setTextSize(1);
    G.setTextColor(on ? NEU.WELL : done ? NEU.RUN : NEU.TEXT3); G.setCursor(x + 14, yy); G.print(String(i + 1));
    G.setTextColor(on ? NEU.TEXT : NEU.TEXT3); G.setCursor(x + 27, yy); G.print(s);
  });
}
function hint(msg, y = 258) {
  const w = msg.length * 6 + 16, x = 120 - (w / 2 | 0);
  G.fillTriangle && 0;
  for (let i = 0; i < 6; i++) G.drawFastHLine(x + 5 - i, y + 1 + i, 1 + 2 * i, NEU.HOLD);
  G.fillRect(x + 5, y + 3, 1, 2, NEU.WELL);
  G.setFont(null); G.setTextSize(1); G.setTextColor(NEU.HOLD); G.setCursor(x + 16, y); G.print(msg);
}
function routineNav() {
  nav([BACK, HOME, { label: "Probe", icon: "play", color: NEU.RUN }]);
}
// tiny illustrations, drawn in the diagram well (x,y,w,h)
function diagWell(x, y, w, h) { inset(x, y, w, h, 10, 3); }
function drawProbeTool(cx, tipY, len = 26) {
  G.fillRoundRect(cx - 5, tipY - len - 8, 11, 9, 2, NEU.TEXT3);
  G.fillRect(cx - 1, tipY - len, 3, len - 3, NEU.TEXT2);
  G.fillCircle(cx, tipY - 2, 3, NEU.ACCENT);
}
function arrowDown(x, y, len, c) { G.fillRect(x, y, 2, len, c); for (let i = 0; i < 4; i++) G.drawFastHLine(x - 3 + i, y + len + i - 2, 8 - 2 * i, c); }
function arrowLeft(x, y, len, c) { G.fillRect(x, y, len, 2, c); for (let i = 0; i < 4; i++) G.drawFastVLine(x + i - 2, y - 3 + i, 8 - 2 * i, c); }
function arrowRight(x, y, len, c) { G.fillRect(x, y, len, 2, c); for (let i = 0; i < 4; i++) G.drawFastVLine(x + len - i + 1, y - 3 + i, 8 - 2 * i, c); }

function scrProbeZ() {
  header("Z Surface");
  posStrip();
  steps(10, 70, 110, ["Fast seek -Z", "Slow probe", "Set Z0"], 0);
  diagWell(10, 174, 110, 76);
  G.fillRect(22, 228, 86, 14, NEU.TEXT3); G.fillRect(40, 222, 50, 6, NEU.ACCENT);   // work + plate
  drawProbeTool(65, 208, 24); arrowDown(36, 186, 26, NEU.JOG);
  field(128, 70, 102, 46, "Max travel", "40", "mm", { focus: true });
  field(128, 124, 102, 46, "Retract", "5", "mm");
  inset(128, 178, 102, 72, 10, 3);
  caption("Result", 138, 187);
  caption("Sets Z0 in", 138, 210, NEU.TEXT2); text(M.wcs, 138, 240, "B12", NEU.RUN);
  hint("Plate clip connected?");
  routineNav();
}
function scrProbeCorner() {
  header("XYZ Corner");
  posStrip();
  steps(10, 70, 110, ["Touch top: Z0", "Probe X & Y", "Set X0 Y0"], 0);
  diagWell(10, 174, 110, 76);
  G.fillRect(52, 210, 56, 34, NEU.TEXT3); G.fillRect(52, 210, 56, 3, NEU.TEXT2);
  drawProbeTool(40, 206, 22); arrowRight(28, 225, 14, NEU.JOG); arrowDown(78, 184, 16, NEU.JOG);
  segmentedMini(128, 70, 102, ["BL", "BR", "TL", "TR"], 0);
  field(128, 124, 102, 38, "Depth", "5.0", "mm", { focus: true, font: "B9" });
  field(128, 168, 102, 38, "Overshoot", "2.0", "mm", { font: "B9" });
  field(128, 212, 102, 38, "XY retract", "3.0", "mm", { font: "B9" });
  hint("Probe above the corner");
  routineNav();
}
// 2x2 corner picker
function segmentedMini(x, y, w, items, sel) {
  inset(x, y, w, 46, 10, 3);
  const cw = (w - 12) / 2 | 0;
  items.forEach((it, i) => {
    const cx = x + 5 + (i % 2) * (cw + 2), cy = y + 5 + (i >> 1) * 19;
    if (i === sel) raised(cx, cy, cw, 17, 6, 1);
    G.setFont(null); G.setTextSize(1); G.setTextColor(i === sel ? NEU.ACCENT : NEU.TEXT3);
    G.setCursor(cx + (cw - it.length * 6) / 2 | 0, cy + 5); G.print(it);
  });
}
function scrProbeBore(o = {}) {
  const boss = o.boss;
  header(boss ? "Boss" : "Bore");
  posStrip();
  steps(10, 70, 110, [boss ? "Probe 4 sides" : "Probe 4 pts", "Find centre", "Set X0 Y0"], 1);
  diagWell(10, 174, 110, 76);
  if (boss) { G.fillCircle(65, 212, 22, NEU.TEXT3); G.fillCircle(65, 212, 21, NEU.TEXT3); }
  else { G.fillRect(26, 190, 78, 46, NEU.TEXT3); G.fillCircle(65, 213, 18, NEU.WELL); }
  G.fillCircle(65, 213, 3, NEU.ACCENT);
  arrowLeft(43, 212, 18, NEU.JOG); arrowRight(66, 212, 18, NEU.JOG);
  field(128, 70, 102, 46, boss ? "Boss dia" : "Nominal dia", "60.0", "mm", { focus: true });
  field(128, 124, 102, 46, "Wall offset", "5.0", "mm");
  inset(128, 178, 102, 72, 10, 3);
  caption("Result", 138, 187);
  caption("Sets X0 Y0", 138, 210, NEU.TEXT2); text(M.wcs, 138, 240, "B12", NEU.RUN);
  hint(boss ? "Centre tip over the boss" : "Place tip inside the bore");
  routineNav();
}
function scrProbeBoss() { scrProbeBore({ boss: true }); }

function scrProbeCfg() {
  header("Plate Setup");
  segmented(10, 38, 220, 36, ["Z Plate", "XYZ Plate"], 0);
  field(10, 84, 220, 50, "Plate thickness", "10.00", "mm", { focus: true, font: "B18" });
  diagWell(10, 144, 220, 124);
  G.fillRect(40, 236, 160, 20, NEU.TEXT3); G.fillRect(70, 216, 100, 20, NEU.ACCENT);
  drawProbeTool(120, 214, 40);
  G.fillRect(186, 216, 1, 20, NEU.TEXT2); G.fillRect(182, 216, 9, 1, NEU.TEXT2); G.fillRect(182, 235, 9, 1, NEU.TEXT2);
  caption("10.00", 192, 222, NEU.ACCENT);
  caption("Turn dial to adjust", 120, 156, NEU.TEXT3, "C");
  nav([BACK, HOME, { label: "Save", primary: true }]);
}

// ---------------- FEEDS & SPEEDS ----------------
function overrideBlock(y, cap, pct, actual, unit, focus) {
  inset(10, y, 220, 50, 12, 3);
  if (focus) G.drawRoundRect(7, y - 3, 226, 56, 15, NEU.ACCENT);
  caption(cap, 22, y + 8, focus ? NEU.ACCENT : NEU.TEXT3);
  text(pct + "%", 22, y + 44, "B18", NEU.TEXT);
  text(actual, 218, y + 32, "B12", NEU.TEXT2, "R"); caption(unit, 218, y + 37, NEU.TEXT3, "R");
  segmented(10, y + 58, 220, 34, ["50", "75", "100", "125", "150"], 2);
}
function scrFeeds() {
  header("Overrides");
  overrideBlock(38, "Feed override", 100, "1200", "mm/m", true);
  overrideBlock(146, "Spindle override", 100, "18000", "rpm", false);
  caption("Dial adjusts the outlined value", 120, 252, NEU.TEXT3, "C");
  nav([HOME, { label: "Reset to 100%", icon: "refresh" }]);
}

// ---------------- SPINDLE ----------------
function scrSpindle() {
  header("Spindle");
  inset(10, 38, 220, 58, 12, 3);
  caption("Actual", 22, 47);
  text("0", 22, 90, "B24", NEU.TEXT);
  G.drawFastVLine(124, 48, 38, NEU.SH_DARK);
  caption("Target", 136, 47, NEU.ACCENT);
  text("12000", 218, 82, "B12", NEU.ACCENT, "R");
  G.fillRoundRect(136, 87, 82, 2, 1, NEU.ACCENT);
  segmented(10, 104, 220, 36, ["Fwd  M3", "Rev  M4"], 0);
  caption("Preset  rpm", 12, 150); caption("0 - 24000", 228, 150, NEU.TEXT3, "R");
  segmented(10, 162, 220, 34, ["6k", "12k", "18k", "24k"], 1);
  button(10, 208, 104, 60, "Start", { face: NEU.GO_FACE, color: NEU.TEXT, icon: ICON.play, font: "B12" });
  button(126, 208, 104, 60, "Stop", { face: NEU.STOP_FACE, color: NEU.TEXT, icon: ICON.stop, font: "B12" });
  nav([HOME, { label: "Overrides", icon: "gauge" }]);
}

// ---------------- MACROS ----------------
function scrMacros() {
  header("Macros");
  const items = [["Park", "G53 G0 Z-5"], ["Tool change", "M6 T1"], ["Spindle warm-up", ""], ["Probe Z & zero", "macro2.nc"], ["Go to XY0", "G0 X0 Y0"]];
  items.forEach(([n, d], i) => {
    const y = 38 + i * 44, on = i === 1;
    if (on) inset(10, y, 220, 36, 10, 3); else raised(10, y, 220, 36, 10, 3);
    ICON.play(26, y + 18, on ? NEU.ACCENT : NEU.TEXT3);
    text(fit(n, "B9", 168), 44, capBase(y, 36, "B9"), "B9", on ? NEU.TEXT : NEU.TEXT2);
    if (on) caption(d, 218, y + 14, NEU.TEXT3, "R");
  });
  [0, 1].forEach(i => G.fillCircle(114 + i * 12, 264, 3, i === 0 ? NEU.ACCENT : NEU.SH_LIGHT));
  nav([HOME, { icon: "refresh", w: 40 }, { label: "Run", icon: "play", color: NEU.RUN }]);
}

// ---------------- FILES (SD CARD) ----------------
function scrFiles() {
  header("Files");
  inset(10, 38, 220, 26, 8, 2);
  ICON.folder(24, 51, NEU.TEXT3);
  G.setFont(null); G.setTextSize(1); G.setTextColor(NEU.TEXT2); G.setCursor(38, 48); G.print("/sd/jobs");
  caption("1/3", 220, 48, NEU.TEXT3, "R");
  inset(10, 72, 220, 198, 12, 3);
  const rows = [["fixtures", null, true], ["bracket_v3.nc", "48 KB"], ["faceplate.nc", "212 KB"], ["logo_engrave.nc", "1.2 MB"], ["spoilboard.gcode", "18 KB"], ["test_square.nc", "3 KB"]];
  rows.forEach(([n, sz, dir], i) => {
    const y = 80 + i * 31, on = i === 1;
    if (on) raised(17, y, 206, 27, 8, 2, NEU.FACE);
    else if (i) G.drawFastHLine(24, y - 2, 192, NEU.SH_DARK);
    (dir ? ICON.folder : ICON.file)(32, y + 13, on ? NEU.ACCENT : dir ? NEU.JOG : NEU.TEXT3);
    const nf = on ? "B9" : "S9";
    text(fit(n, nf, sz ? 214 - 48 - sz.length * 7 - 6 : 150), 48, capBase(y, 27, nf), nf, on ? NEU.TEXT : NEU.TEXT2);
    if (sz) caption(sz, 214, y + 10, NEU.TEXT3, "R"); else ICON.fwd(212, y + 13, NEU.TEXT3);
  });
  nav([HOME, { icon: "up", w: 40 }, { label: "Run", icon: "play", color: NEU.RUN }]);
}

// ---------------- FLUIDNC ----------------
function scrFluidNC() {
  header("FluidNC");
  inset(10, 38, 220, 146, 12, 3);
  const kv = [["FluidDial", "v2.3.3"], ["FluidNC", "v3.9.1"], ["Machine", "Shapeoko"], ["Free heap", "144 KB"], ["Status", "Idle"]];
  kv.forEach(([k, v], i) => {
    const y = 48 + i * 27;
    if (i) G.drawFastHLine(22, y - 3, 196, NEU.SH_DARK);
    caption(k, 22, y + 8);
    text(v, 218, y + 16, "B9", k === "Status" ? NEU.IDLE : NEU.TEXT, "R");
  });
  [["Connection", "ESP-NOW", "link"], ["Jog dial", "Rotate", "gear"]].forEach(([l, v, ic], i) => {
    const y = 194 + i * 42;
    raised(10, y, 220, 34, 10, 3);
    ICON[ic](30, y + 17, NEU.ACCENT);
    text(l, 46, capBase(y, 34, "B9"), "B9", NEU.TEXT);
    caption(v, 200, y + 14, NEU.TEXT2, "R"); ICON.fwd(214, y + 17, NEU.TEXT3);
  });
  nav([HOME, { label: "Status", icon: "status" }]);
}

// ---------------- CONNECTION ----------------
function scrConnection() {
  header("Connection");
  caption("Transport", 12, 40);
  segmented(10, 52, 220, 36, ["UART", "ESP-NOW"], 1);
  inset(10, 98, 220, 102, 12, 3);
  led(26, 116, NEU.RUN, 4);
  text("Connected", 38, 122, "B12", NEU.RUN);
  text("Shapeoko", 22, 146, "B9", NEU.TEXT);
  caption("A4:CF:12:9B:00:1E", 22, 155, NEU.TEXT3);
  for (let i = 0; i < 4; i++) { const h = 5 + i * 4; G.fillRoundRect(190 + i * 7, 126 - h, 5, h, 1, i < 3 ? NEU.RUN : NEU.SH_LIGHT); }
  caption("-58dBm", 218, 132, NEU.TEXT3, "R");
  G.drawFastHLine(22, 168, 196, NEU.SH_DARK);
  [["RX", "48213"], ["LOST", "0"], ["DROP", "0"], ["CH", "6"]].forEach(([k, v], i) => {
    const x = 22 + i * 50; caption(k, x, 176); G.setFont(null); G.setTextColor(NEU.TEXT2); G.setCursor(x, 186); G.print(v); });
  button(10, 208, 104, 44, "Pair", { icon: ICON.plus });
  button(126, 208, 104, 44, "Machines");
  nav([BACK, HOME, { label: "Status", icon: "status" }]);
}
