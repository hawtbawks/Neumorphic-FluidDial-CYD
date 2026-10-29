/*
 * neu_ui.js — the neumorphic UI kit shared by every screen.
 * Mirrors (and is the spec for) src/screens/neu_ui.{h,cpp}.
 *
 * Rules of the system
 *   • One surface (NEU_BG), light from the top-left.
 *   • raised()  = something you can press.   inset() = a readout, a field, or the
 *     selected option.  Pressing a toggle moves it from raised to inset.
 *   • NEU_ACCENT (orange) = focus only: selected option, dial target, primary action.
 *   • Machine-state colour lives in the header chip on every screen.
 *   • Every primitive is built from fillRoundRect / fillRect / drawFastHLine, so it
 *     ports 1:1 to LovyanGFX.
 *
 * Text API: neuText(s, x, baselineY, font, color, align) — y is the BASELINE for
 * FreeSans fonts (LovyanGFX setTextDatum(baseline_*)).  The GLCD font is only
 * used for captions, through neuCaption(), where y is the TOP of the text.
 *
 * Layout grid (240x320):
 *   header   y 0..29, groove at y 30
 *   content  y 36..276, 10 px side margins, 8 px gaps (shadows need 3 px clear)
 *   nav bar  groove at y 278, buttons y 285 h 30; Home always bottom-left
 */

// ---------- colour ----------
function rgb565(r, g, b) { return (((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3)) & 0xffff; }
function rgb565to888(c) {
  return [Math.round(((c >> 11) & 31) * 255 / 31), Math.round(((c >> 5) & 63) * 255 / 63), Math.round((c & 31) * 255 / 31)];
}
// Linear blend of two RGB565 colours: t=0 -> a, t=1 -> b.  (C++: neuMix)
function neuMix(a, b, t) {
  const A = rgb565to888(a), B = rgb565to888(b);
  return rgb565(A[0] + (B[0] - A[0]) * t, A[1] + (B[1] - A[1]) * t, A[2] + (B[2] - A[2]) * t);
}

// ---------- tokens ----------
const NEU_BG       = rgb565(44, 48, 56);
const NEU_FACE     = rgb565(47, 51, 60);
const NEU_WELL     = rgb565(37, 40, 47);
const NEU_SH_DARK  = rgb565(24, 26, 31);
const NEU_SH_LIGHT = rgb565(70, 76, 88);
const NEU_TEXT     = rgb565(228, 232, 238);
const NEU_TEXT2    = rgb565(150, 158, 172);
const NEU_TEXT3    = rgb565(100, 108, 122);
const NEU_ACCENT   = rgb565(255, 150, 48);
const NEU_IDLE     = rgb565(96, 208, 200);
const NEU_RUN      = rgb565(92, 220, 128);
const NEU_HOLD     = rgb565(255, 190, 64);
const NEU_ALARM    = rgb565(255, 84, 84);
const NEU_JOG      = rgb565(110, 168, 255);
const NEU_HOME     = rgb565(176, 140, 255);
const NEU_AX       = [rgb565(255, 112, 104), rgb565(112, 224, 128), rgb565(104, 168, 255), rgb565(220, 200, 120), rgb565(200, 140, 220), rgb565(140, 210, 230)];
const NEU_GO_FACE  = rgb565(36, 132, 72);
const NEU_STOP_FACE= rgb565(200, 44, 44);
// Pre-blended shadow ramps (C++: computed once in neuInit()).
const NEU_DEPTH = 3;
const _neuDark  = [1, 2, 3].map((i) => neuMix(NEU_BG, NEU_SH_DARK, i / NEU_DEPTH));
const _neuLight = [1, 2, 3].map((i) => neuMix(NEU_BG, NEU_SH_LIGHT, i / NEU_DEPTH));
const _neuRim   = neuMix(NEU_BG, NEU_SH_LIGHT, 0.8);

// ---------- fonts ----------
const NEU_FONTS = {
  S9: "FreeSans9pt7b", S12: "FreeSans12pt7b",
  B9: "FreeSansBold9pt7b", B12: "FreeSansBold12pt7b", B18: "FreeSansBold18pt7b", B24: "FreeSansBold24pt7b",
};
function _neuSetFont(font) { display.setFont(GFXFONTS[NEU_FONTS[font]]); display.setTextSize(1); }
function neuTextW(s, font) { _neuSetFont(font); const w = display.textWidth(String(s)); display.setFont(null); return w; }
// Baseline that visually centres capitals in a box of height h starting at y.
function neuCapBase(y, h, font) { return y + ((h + gfxMetrics(GFXFONTS[NEU_FONTS[font]]).cap) / 2 | 0); }
function neuText(s, x, y, font, color, align = "L") {
  s = String(s);
  _neuSetFont(font);
  const w = display.textWidth(s);
  const x0 = align === "C" ? x - (w / 2 | 0) : align === "R" ? x - w : x;
  display.setTextColor(color); display.setCursor(x0, y); display.print(s);
  display.setFont(null);
  return w;
}
function neuTextIn(s, x, y, w, h, font, color, align = "C") {
  const bx = align === "C" ? x + (w / 2 | 0) : align === "R" ? x + w : x;
  return neuText(s, bx, neuCapBase(y, h, font), font, color, align);
}
// Shorten s with a trailing ".." until it fits maxW.
function neuFit(s, font, maxW) {
  s = String(s);
  if (neuTextW(s, font) <= maxW) return s;
  while (s.length > 1 && neuTextW(s + "..", font) > maxW) s = s.slice(0, -1);
  return s + "..";
}
// Caption: GLCD 5x7, upper-case, 1 px extra tracking (7 px pitch).  y = top.
function neuCaptionW(s) { return String(s).length * 7 - 1; }
function neuCaption(s, x, y, color = NEU_TEXT3, align = "L") {
  s = String(s).toUpperCase();
  const w = neuCaptionW(s);
  let cx = align === "C" ? x - (w / 2 | 0) : align === "R" ? x - w : x;
  display.setFont(null); display.setTextSize(1); display.setTextColor(color);
  for (const ch of s) { display.setCursor(cx, y); display.print(ch); cx += 7; }
  return w;
}
// Plain GLCD text (mixed case), y = top.
function neuSmall(s, x, y, color, align = "L") {
  s = String(s);
  const w = s.length * 6;
  const x0 = align === "C" ? x - (w / 2 | 0) : align === "R" ? x - w : x;
  display.setFont(null); display.setTextSize(1); display.setTextColor(color);
  display.setCursor(x0, y); display.print(s);
  return w;
}

// ---------- surfaces ----------
// raised(): convex.  2*depth+1 fillRoundRect.  Needs `depth` px clear around it.
function neuRaised(x, y, w, h, r = 10, depth = NEU_DEPTH, face = NEU_FACE) {
  for (let k = depth; k >= 1; k--) display.fillRoundRect(x + k, y + k, w, h, r, _neuDark[NEU_DEPTH - k] ?? _neuDark[0]);
  for (let k = depth; k >= 1; k--) display.fillRoundRect(x - k, y - k, w, h, r, _neuLight[NEU_DEPTH - k] ?? _neuLight[0]);
  display.fillRoundRect(x, y, w, h, r, face);
}
// inset(): concave.  depth+2 fillRoundRect, entirely inside the box.  The flat
// interior is (x+depth, y+depth) .. (x+w-2, y+h-2): clear that to repaint contents.
function neuInset(x, y, w, h, r = 10, depth = NEU_DEPTH, well = NEU_WELL) {
  display.fillRoundRect(x, y, w, h, r, _neuRim);
  for (let k = 0; k < depth; k++) display.fillRoundRect(x + k, y + k, w - k - 1, h - k - 1, r, neuMix(NEU_SH_DARK, well, k / depth));
  display.fillRoundRect(x + depth, y + depth, w - depth - 1, h - depth - 1, r, well);
}
// Clear the space a raised/inset element and its shadow occupy (before redrawing it in another state).
function neuClear(x, y, w, h, depth = NEU_DEPTH) { display.fillRect(x - depth, y - depth, w + 2 * depth + 1, h + 2 * depth + 1, NEU_BG); }
// Carved line: dark over light.
function neuGroove(y, x0 = 8, x1 = 232) { display.drawFastHLine(x0, y, x1 - x0, NEU_SH_DARK); display.drawFastHLine(x0, y + 1, x1 - x0, NEU_SH_LIGHT); }
function neuLed(cx, cy, color, r = 3, bg = NEU_WELL) {
  display.fillCircle(cx, cy, r + 2, neuMix(bg, color, 0.22));
  display.fillCircle(cx, cy, r, color);
  display.fillRect(cx - 1, cy - 1, 1, 1, neuMix(color, NEU_TEXT, 0.6));
}

// ---------- state ----------
function neuStateColor(status) {
  const s = String(status).split(":")[0];
  return { Idle: NEU_IDLE, Run: NEU_RUN, Jog: NEU_JOG, Home: NEU_HOME, Hold: NEU_HOLD, Door: NEU_HOLD,
           Alarm: NEU_ALARM, Check: NEU_IDLE, Sleep: NEU_TEXT3 }[s] ?? NEU_TEXT2;
}
// What the header chip says: link problems first, then the machine state.
function neuChipState() {
  if (!pendantConnected) return { label: "OFFLINE", color: NEU_TEXT3 };
  if (!pendantSynced || pendantMachine.status === "N/C" || !pendantMachine.status) return { label: "SYNC", color: NEU_HOLD };
  const s = pendantMachine.status;
  return { label: s.split(":")[0].toUpperCase(), color: neuStateColor(s) };
}

// ---------- header ----------
const NEU_HDR_H = 30;
let _neuTitle = "";
let _neuChipKey = "";
function _neuChip() {
  const st = neuChipState();
  const w = neuTextW(st.label, "B9") + 26, x = 232 - w;
  const clearX = Math.min(x - 52, 10 + neuTextW(_neuTitle, "B9") + 4);   // chip + icon zone, never the title
  display.fillRect(clearX, 1, 240 - clearX, 28, NEU_BG);
  neuInset(x, 5, w, 20, 10, 2);
  neuLed(x + 11, 15, st.color, 3);
  neuText(st.label, x + 19, neuCapBase(5, 20, "B9"), "B9", st.color);
  return x;
}
function _neuLinkIcon(xr, bars) {
  const x = xr - 14;
  if (pendantMachine.wifiInApMode) { neuSmall("AP", x, 11, NEU_HOLD); return x; }
  for (let i = 0; i < 4; i++) { const h = 3 + i * 2; display.fillRect(x + i * 4, 20 - h, 2, h, i < bars ? NEU_TEXT2 : NEU_SH_LIGHT); }
  return x;
}
function _neuBattery(xr) {
  const pct = pendantMachine.batteryPercent, x = xr - 20, y = 10;
  if (pct < 0) return xr;
  display.drawRoundRect(x, y, 17, 10, 2, NEU_TEXT3); display.fillRect(x + 17, y + 3, 2, 4, NEU_TEXT3);
  const col = pct > 50 ? NEU_TEXT2 : pct > 20 ? NEU_HOLD : NEU_ALARM;
  display.fillRect(x + 2, y + 2, Math.round(13 * pct / 100), 6, col);
  if (pendantMachine.batteryCharging) { display.fillRect(x + 7, y + 1, 2, 4, NEU_HOLD); display.fillRect(x + 6, y + 4, 4, 2, NEU_HOLD); display.fillRect(x + 7, y + 5, 2, 4, NEU_HOLD); }
  return x;
}
// Wireless transport?  (The sim's Connection screen reports "UART" / "ESPNOW".)
function neuWireless() { const m = comms_active_mode(); return m === "ESPNOW" || m === COMMS_MODE_WIFI; }
function neuLinkBars() {
  if (comms_active_mode() === "ESPNOW") return espnow_is_connected() ? espnow_signal_bars() : 0;
  return Math.max(0, pendantMachine.wifiSignalBars);
}
function _neuHeaderRight() {
  const chipX = _neuChip();
  if (!neuWireless()) return;                                  // wired: no link / battery icons
  const titleEnd = 10 + neuTextW(_neuTitle, "B9") + 8;
  let x = chipX;
  if (x - 26 >= titleEnd) x = _neuBattery(x - 6);              // icons yield to a long title
  if (x - 20 >= titleEnd) _neuLinkIcon(x - 6, neuLinkBars());
}
// Clears the screen and draws the header.  Every screen's draw*() starts here.
function neuHeader(title) {
  display.fillScreen(NEU_BG);
  _neuTitle = title;
  _neuChipKey = _neuHeaderKey();
  _neuHeaderRight();
  neuText(neuFit(title, "B9", 150), 10, neuCapBase(5, 20, "B9"), "B9", NEU_TEXT);
  neuGroove(NEU_HDR_H);
}
function _neuHeaderKey() {
  const st = neuChipState();
  return st.label + "|" + comms_active_mode() + "|" + neuLinkBars() + "|" + pendantMachine.batteryPercent + "|" + pendantMachine.batteryCharging + "|" + pendantMachine.wifiInApMode;
}
// Called from the 100 ms tick on every screen: repaints the right side only on change.
function neuHeaderUpdate() {
  if (!_neuTitle) return;
  const k = _neuHeaderKey();
  if (k === _neuChipKey) return;
  _neuChipKey = k;
  _neuHeaderRight();
}

// ---------- nav bar ----------
const NEU_NAV_Y = 285, NEU_NAV_H = 30;
const NAV_HOME = { icon: "home", w: 40, id: "home" };
const NAV_BACK = { icon: "back", w: 40, id: "back" };
// Lays out nav items left to right (fixed widths first, the rest share the space).
function neuNavLayout(items) {
  const gap = 8, x0 = 10, W = 220;
  const fixed = items.reduce((a, it) => a + (it.w || 0), 0), flex = items.filter((i) => !i.w).length;
  const fw = flex ? ((W - fixed - gap * (items.length - 1)) / flex) | 0 : 0;
  let x = x0;
  return items.map((it, i) => {
    let w = it.w || fw; if (i === items.length - 1) w = x0 + W - x;
    const r = { x, w, it }; x += w + gap; return r;
  });
}
function neuNav(items) {
  neuGroove(278);
  for (const { x, w, it } of neuNavLayout(items)) neuNavButton(x, w, it);
}
function neuNavButton(x, w, it) {
  const col = it.color ?? (it.primary ? NEU_ACCENT : NEU_TEXT2);
  neuClear(x, NEU_NAV_Y, w, NEU_NAV_H);
  neuButton(x, NEU_NAV_Y, w, NEU_NAV_H, it.label || "", { icon: it.icon ? NEU_ICON[it.icon] : null, color: col, on: it.on, face: it.face, bar: false, dim: it.dim });
}
// Returns the id (or index) of the nav item under (x,y), or null.
function neuNavHit(items, x, y) {
  if (y < NEU_NAV_Y - 4) return null;
  for (const { x: bx, w, it } of neuNavLayout(items)) if (x >= bx - 4 && x <= bx + w + 4) return it.id ?? it.label;
  return null;
}

// ---------- controls ----------
// opts: { on, color, font, icon, sub, face, r, bar, dim }
function neuButton(x, y, w, h, label, o = {}) {
  const font = o.font || "B9";
  if (o.on) neuInset(x, y, w, h, o.r ?? 10, 3); else neuRaised(x, y, w, h, o.r ?? 10, o.depth ?? 3, o.face);
  let col = o.color ?? (o.on ? NEU_ACCENT : NEU_TEXT);
  if (o.dim) col = NEU_TEXT3;
  if (o.icon && label) {
    const lw = neuTextW(label, font), total = 18 + 6 + lw, sx = x + ((w - total) / 2 | 0);
    o.icon(sx + 9, y + (h / 2 | 0), col, o.on ? NEU_WELL : (o.face ?? NEU_FACE));
    neuText(label, sx + 24, neuCapBase(y, h, font), font, col);
  } else if (o.icon) {
    o.icon(x + (w / 2 | 0), y + (h / 2 | 0), col, o.on ? NEU_WELL : (o.face ?? NEU_FACE));
  } else if (o.sub) {
    neuText(label, x + (w / 2 | 0), y + (h / 2 | 0) + 1, font, col, "C");
    neuCaption(o.sub, x + (w / 2 | 0), y + (h / 2 | 0) + 6, o.on ? neuMix(NEU_ACCENT, NEU_WELL, 0.35) : NEU_TEXT3, "C");
  } else neuTextIn(label, x, y, w, h, font, col);
  if (o.on && o.bar !== false) display.fillRoundRect(x + 8, y + h - 6, w - 17, 2, 1, NEU_ACCENT);
}
// Segmented control: inset track, the selected segment is a raised pill.
// sel may be -1 (none selected).  Use neuSegHit() for touch.
function neuSegLayout(x, w, n) {
  const pad = 5, gap = 2, sw = ((w - 2 * pad - (n - 1) * gap) / n) | 0;
  const out = [];
  for (let i = 0; i < n; i++) { const sx = x + pad + i * (sw + gap); out.push({ x: sx, w: i === n - 1 ? x + w - pad - sx : sw }); }
  return out;
}
function neuSegmented(x, y, w, h, items, sel, o = {}) {
  neuInset(x, y, w, h, o.r ?? 10, 3);
  const segs = neuSegLayout(x, w, items.length), pad = 5;
  items.forEach((it, i) => {
    const s = segs[i];
    if (i === sel) neuRaised(s.x, y + pad, s.w, h - 2 * pad, (o.r ?? 10) - 3, 2);
    neuTextIn(it, s.x, y + pad, s.w, h - 2 * pad, o.font || "B9", i === sel ? (o.selColor ?? NEU_ACCENT) : (o.colors?.[i] ?? NEU_TEXT2));
  });
}
function neuSegHit(x, y, w, h, n, tx, ty) {
  if (ty < y || ty > y + h || tx < x || tx > x + w) return -1;
  return Math.min(n - 1, Math.max(0, ((tx - x) * n / w) | 0));
}
// Value field: inset well with caption + value + unit; focus = dial target (accent ring).
function neuField(x, y, w, h, cap, value, unit, o = {}) {
  neuInset(x, y, w, h, 8, 3);
  neuFieldValue(x, y, w, h, cap, value, unit, o);
}
// Repaint only the field's contents and focus ring (no shadow redraw).
function neuFieldValue(x, y, w, h, cap, value, unit, o = {}) {
  display.drawRoundRect(x - 3, y - 3, w + 6, h + 6, 11, o.focus ? NEU_ACCENT : NEU_BG);
  display.drawRoundRect(x - 2, y - 2, w + 4, h + 4, 10, o.focus ? NEU_ACCENT : NEU_BG);
  display.fillRect(x + 4, y + 4, w - 7, h - 7, NEU_WELL);
  const pad = h < 38 ? 5 : 7;          // short fields pull caption and value toward the edges
  neuCaption(cap, x + 9, y + pad, o.focus ? NEU_ACCENT : NEU_TEXT3);
  const vf = o.font || "B12", base = y + h - pad;
  if (o.align === "R") {
    const uw = unit ? neuCaptionW(unit) + 4 : 0;
    if (unit) neuCaption(unit, x + w - 9, base - 7, NEU_TEXT3, "R");
    neuText(value, x + w - 9 - uw, base, vf, o.color ?? NEU_TEXT, "R");
  } else {
    const vw = neuText(value, x + 9, base, vf, o.color ?? NEU_TEXT);
    if (unit) neuCaption(unit, x + 9 + vw + 4, base - 7, NEU_TEXT3);
  }
}
// Thin progress bar for use inside a well.
function neuProgressBar(x, y, w, frac, color = NEU_ACCENT) {
  display.fillRoundRect(x, y, w, 4, 2, NEU_SH_DARK);
  const fw = Math.round(w * Math.max(0, Math.min(1, frac)));
  if (fw > 0) display.fillRoundRect(x, y, Math.max(4, fw), 4, 2, color);
}
// Small amber hint line with a warning triangle (centred).
function neuHint(msg, y, color = NEU_HOLD) {
  const w = msg.length * 6 + 16, x = 120 - (w / 2 | 0);
  display.fillRect(10, y - 2, 220, 12, NEU_BG);
  for (let i = 0; i < 6; i++) display.drawFastHLine(x + 5 - i, y + 1 + i, 1 + 2 * i, color);
  display.fillRect(x + 5, y + 3, 1, 2, NEU_BG);
  neuSmall(msg, x + 16, y, color);
}
// Modal dialog.  okLabel null = no buttons (progress); cancelLabel null = a single
// centred OK.  Returns button rects for hit testing (missing buttons are absent).
function neuDialog(title, body, cancelLabel, okLabel, okColor = NEU_RUN, titleColor = NEU_TEXT) {
  // dim the screen behind with a 2px checker — cheap and AA-free
  for (let yy = 32; yy < 320; yy += 2) for (let xx = (yy >> 1) & 1; xx < 240; xx += 2) display.fillRect(xx, yy, 1, 1, NEU_SH_DARK);
  neuRaised(18, 92, 204, 138, 14, 3, NEU_BG);
  neuText(title, 120, 120, "B12", titleColor, "C");
  const lines = Array.isArray(body) ? body : [body];
  lines.forEach((l, i) => neuSmall(l, 120, 134 + i * 12, NEU_TEXT2, "C"));
  const out = {};
  if (okLabel && cancelLabel) {
    out.cancel = { x: 32, y: 180, w: 80, h: 36 }; out.ok = { x: 124, y: 180, w: 84, h: 36 };
    neuButton(out.cancel.x, out.cancel.y, out.cancel.w, out.cancel.h, cancelLabel, { color: NEU_TEXT2 });
  } else if (okLabel) out.ok = { x: 72, y: 180, w: 96, h: 36 };
  if (out.ok) neuButton(out.ok.x, out.ok.y, out.ok.w, out.ok.h, okLabel, { color: okColor });
  return out;
}
function neuDialogHit(r, x, y) { return !!r && x >= r.x - 4 && x <= r.x + r.w + 4 && y >= r.y - 4 && y <= r.y + r.h + 4; }

// ---------- icons: 18x18 box centred on (cx,cy), 2 px strokes.  bg = surface colour behind ----------
function _L2(x0, y0, x1, y1, c) { display.drawLine(x0, y0, x1, y1, c); display.drawLine(x0 + 1, y0, x1 + 1, y1, c); display.drawLine(x0, y0 + 1, x1, y1 + 1, c); }
function _arc(cx, cy, r0, r1, a0, a1, c) {
  for (let a = a0; a <= a1; a += 3) { const t = a * Math.PI / 180; for (let r = r0; r <= r1; r++) display.fillRect(Math.round(cx + Math.cos(t) * r), Math.round(cy + Math.sin(t) * r), 1, 1, c); }
}
const NEU_ICON = {
  home(cx, cy, c) { _L2(cx - 8, cy, cx, cy - 8, c); _L2(cx, cy - 8, cx + 7, cy - 1, c); display.fillRect(cx - 6, cy, 2, 8, c); display.fillRect(cx + 4, cy, 2, 8, c); display.fillRect(cx - 6, cy + 7, 12, 2, c); display.fillRect(cx - 1, cy + 3, 3, 5, c); },
  back(cx, cy, c) { _L2(cx + 3, cy - 7, cx - 4, cy, c); _L2(cx - 4, cy, cx + 3, cy + 7, c); },
  fwd(cx, cy, c) { _L2(cx - 4, cy - 7, cx + 3, cy, c); _L2(cx + 3, cy, cx - 4, cy + 7, c); },
  chevron(cx, cy, c) { display.drawLine(cx - 2, cy - 4, cx + 2, cy, c); display.drawLine(cx + 2, cy, cx - 2, cy + 4, c); display.drawLine(cx - 1, cy - 4, cx + 3, cy, c); display.drawLine(cx + 3, cy, cx - 1, cy + 4, c); },
  play(cx, cy, c) { for (let i = 0; i < 9; i++) display.drawFastVLine(cx - 4 + i, cy - 8 + i, 17 - 2 * i, c); },
  playSm(cx, cy, c) { for (let i = 0; i < 6; i++) display.drawFastVLine(cx - 3 + i, cy - 5 + i, 11 - 2 * i, c); },
  stop(cx, cy, c) { display.fillRoundRect(cx - 6, cy - 6, 13, 13, 2, c); },
  jog(cx, cy, c) {
    const tri = (dx, dy) => { for (let i = 0; i < 4; i++) { if (dx) display.drawFastVLine(cx + dx * (8 - i), cy - i, 2 * i + 1, c); else display.drawFastHLine(cx - i, cy + dy * (8 - i), 2 * i + 1, c); } };
    tri(1, 0); tri(-1, 0); tri(0, 1); tri(0, -1); display.fillRect(cx - 5, cy - 1, 11, 2, c); display.fillRect(cx - 1, cy - 5, 2, 11, c);
  },
  origin(cx, cy, c) { display.drawCircle(cx, cy, 7, c); display.drawCircle(cx, cy, 6, c); display.fillRect(cx - 9, cy - 1, 19, 2, c); display.fillRect(cx - 1, cy - 9, 2, 19, c); display.fillCircle(cx, cy, 2, c); },
  gauge(cx, cy, c) { _arc(cx, cy + 3, 8, 9, 180, 360, c); _L2(cx, cy + 3, cx + 5, cy - 3, c); display.fillCircle(cx, cy + 3, 2, c); },
  spindle(cx, cy, c) { display.fillRect(cx - 5, cy - 8, 11, 5, c); display.fillRect(cx - 3, cy - 3, 7, 5, c); _L2(cx - 1, cy + 2, cx - 1, cy + 9, c); _L2(cx - 6, cy + 5, cx - 1, cy + 9, c); _L2(cx + 5, cy + 5, cx, cy + 9, c); },
  macro(cx, cy, c) { _L2(cx + 2, cy - 9, cx - 4, cy + 1, c); display.fillRect(cx - 4, cy, 9, 2, c); _L2(cx + 4, cy + 1, cx - 2, cy + 9, c); },
  card(cx, cy, c) { display.drawRoundRect(cx - 6, cy - 8, 13, 17, 2, c); display.drawRoundRect(cx - 5, cy - 7, 11, 15, 2, c); for (let i = 0; i < 3; i++) display.fillRect(cx - 3 + i * 3, cy - 6, 1, 4, c); },
  probe(cx, cy, c) { display.fillRect(cx - 4, cy - 9, 9, 5, c); display.fillRect(cx - 1, cy - 4, 3, 9, c); display.fillCircle(cx, cy + 6, 2, c); display.fillRect(cx - 8, cy + 9, 17, 1, c); },
  status(cx, cy, c) { display.fillRect(cx - 7, cy + 1, 3, 7, c); display.fillRect(cx - 2, cy - 3, 3, 11, c); display.fillRect(cx + 3, cy - 8, 3, 16, c); },
  folder(cx, cy, c) { display.fillRoundRect(cx - 8, cy - 5, 7, 4, 1, c); display.fillRoundRect(cx - 8, cy - 3, 17, 11, 2, c); },
  file(cx, cy, c) { display.drawRect(cx - 6, cy - 8, 12, 17, c); display.drawRect(cx - 5, cy - 7, 10, 15, c); display.fillRect(cx - 3, cy - 3, 6, 1, c); display.fillRect(cx - 3, cy, 6, 1, c); display.fillRect(cx - 3, cy + 3, 4, 1, c); },
  up(cx, cy, c) { _L2(cx - 7, cy + 3, cx, cy - 4, c); _L2(cx, cy - 4, cx + 7, cy + 3, c); },
  refresh(cx, cy, c) { _arc(cx, cy, 6, 7, 0, 290, c); for (let i = 0; i < 4; i++) display.drawFastHLine(cx + 1, cy - 9 + i, 7 - 2 * i, c); },
  gear(cx, cy, c) { for (let a = 0; a < 8; a++) { const t = a * Math.PI / 4; display.fillRect(Math.round(cx + Math.cos(t) * 7) - 1, Math.round(cy + Math.sin(t) * 7) - 1, 3, 3, c); } display.drawCircle(cx, cy, 4, c); display.drawCircle(cx, cy, 5, c); display.drawCircle(cx, cy, 6, c); },
  link(cx, cy, c) { display.drawRoundRect(cx - 9, cy - 3, 10, 7, 3, c); display.drawRoundRect(cx, cy - 3, 10, 7, 3, c); },
  plus(cx, cy, c) { display.fillRect(cx - 6, cy - 1, 13, 3, c); display.fillRect(cx - 1, cy - 6, 3, 13, c); },
  minus(cx, cy, c) { display.fillRect(cx - 6, cy - 1, 13, 3, c); },
  trash(cx, cy, c) { display.fillRect(cx - 6, cy - 6, 13, 2, c); display.fillRect(cx - 2, cy - 8, 5, 2, c); display.drawRect(cx - 5, cy - 3, 11, 12, c); display.fillRect(cx - 2, cy - 1, 1, 8, c); display.fillRect(cx + 2, cy - 1, 1, 8, c); },
  unlock(cx, cy, c) { display.fillRoundRect(cx - 6, cy - 1, 13, 10, 2, c); _arc(cx - 3, cy - 3, 4, 5, 180, 360, c); display.fillRect(cx - 8, cy - 4, 2, 4, c); },
};

// ---------- DRO (work + machine), used by Main Menu, Status, Work Area ----------
const NEU_DRO_H = 68;
function neuDroLabels(x, y) {
  display.fillRect(x + 4, y + 4, 212, 12, NEU_WELL);
  if (pendantMachine.numAxes > 3) {    // 4-6 axes: two columns of work positions, no machine column
    neuCaption("Work " + pendantMachine.workCoordSystem, x + 12, y + 7, NEU_TEXT3);
    return;
  }
  neuCaption("Work " + pendantMachine.workCoordSystem, x + 132, y + 7, NEU_TEXT3, "R");
  neuCaption("Mach", x + 208, y + 7, NEU_TEXT3, "R");
}
function neuDro(x, y) {
  neuInset(x, y, 220, NEU_DRO_H, 12, 3);
  neuDroLabels(x, y);
  neuDroValues(x, y);
}
function neuFmtPos(v) { return Number(v).toFixed(pendantMachine.inInches ? 4 : 3); }
// Repaint just the numbers (called from the 100 ms tick when positions change).
function neuDroValues(x, y) {
  const all = pendantMachine.numAxes, n = Math.min(3, all);
  display.fillRect(x + 4, y + 16, 212, 49, NEU_WELL);
  if (all > 3) {
    for (let i = 0; i < all; i++) {
      const cx = x + (i < 3 ? 12 : 116), b = y + 31 + (i % 3) * 16;
      neuText(AXIS_NAMES[i], cx, b, "B9", NEU_AX[i]);
      neuText(neuFmtPos(pendantMachine.wpos[i]), cx + 92, b, "B9", NEU_TEXT, "R");
    }
    return;
  }
  for (let i = 0; i < n; i++) {
    const b = y + 31 + i * 16;
    neuText(AXIS_NAMES[i], x + 12, b, "B9", NEU_AX[i]);
    neuText(neuFmtPos(pendantMachine.wpos[i]), x + 132, b, "B9", NEU_TEXT, "R");
    neuCaption(Number(pendantMachine.mpos[i]).toFixed(2), x + 208, b - 7, NEU_TEXT3, "R");
  }
}
function neuDroKey() { return pendantMachine.wpos.slice(0, 3).join() + pendantMachine.mpos.slice(0, 3).join() + pendantMachine.inInches + pendantMachine.workCoordSystem; }

// ---------- list screens (Files, Macros, Machines) ----------
// crumb strip y 38 h 26 (path left, page readout right — tap it to advance a page)
// list well   y 72 h 198, LIST_ROWS rows at a 38 px pitch; the selected row is a raised pill
const NEU_LIST_CRUMB_Y = 38, NEU_LIST_WELL_Y = 72, NEU_LIST_WELL_H = 198;
const NEU_LIST_ROW_Y0 = 77, NEU_LIST_PITCH = 38, NEU_LIST_ROW_H = 34;
function neuListRowY(i) { return NEU_LIST_ROW_Y0 + i * NEU_LIST_PITCH; }
// Index of the row under (x,y), or -1.
function neuListRowHit(x, y) {
  if (x < 10 || x > 230 || y < NEU_LIST_WELL_Y || y >= NEU_LIST_WELL_Y + NEU_LIST_WELL_H) return -1;
  const i = ((y - NEU_LIST_ROW_Y0 + 2) / NEU_LIST_PITCH) | 0;
  return i >= 0 && i < LIST_ROWS ? i : -1;
}
function neuListPageHit(x, y) { return y >= NEU_LIST_CRUMB_Y - 4 && y <= NEU_LIST_CRUMB_Y + 30 && x >= 170; }
function neuListFrame() {
  neuInset(10, NEU_LIST_CRUMB_Y, 220, 26, 8, 2);
  neuInset(10, NEU_LIST_WELL_Y, 220, NEU_LIST_WELL_H, 12, 3);
}
// Repaint the crumb strip contents.  page/pages 1-based; pages 0 = nothing listed.
function neuListCrumb(icon, text, page, pages, truncated) {
  display.fillRect(13, NEU_LIST_CRUMB_Y + 3, 215, 21, NEU_WELL);
  NEU_ICON[icon](24, NEU_LIST_CRUMB_Y + 13, NEU_TEXT3);
  const pl = pages > 0 ? `${page}/${pages}${truncated ? "+" : ""}` : "";
  neuSmall(crumbText(text, pl ? 24 : 30), 38, NEU_LIST_CRUMB_Y + 10, NEU_TEXT2);
  if (pl) {
    neuSmall(pl, 220, NEU_LIST_CRUMB_Y + 10, pages > 1 ? NEU_ACCENT : NEU_TEXT3, "R");
  }
}
function neuListClear() { display.fillRect(14, NEU_LIST_WELL_Y + 4, 213, NEU_LIST_WELL_H - 7, NEU_WELL); }
// kind: "dir" | "up" | "file" | "macro".  state: "" | "sel"
function neuListRow(i, label, kind, sel) {
  const y = neuListRowY(i);
  if (sel) neuRaised(18, y, 204, NEU_LIST_ROW_H - 4, 9, 2);
  else if (i > 0) display.drawFastHLine(26, y - 3, 188, NEU_SH_DARK);
  const cy = y + ((NEU_LIST_ROW_H - 4) / 2 | 0);
  const iconCol = sel ? NEU_ACCENT : kind === "dir" || kind === "up" ? NEU_JOG : NEU_TEXT3;
  if (kind === "dir") NEU_ICON.folder(32, cy, iconCol);
  else if (kind === "up") NEU_ICON.up(32, cy + 2, iconCol);
  else if (kind === "macro") NEU_ICON.playSm(32, cy, iconCol);
  else NEU_ICON.file(32, cy, iconCol);
  const font = sel ? "B9" : "S9", maxW = kind === "dir" ? 150 : 166;
  neuText(neuFit(label, font, maxW), 48, neuCapBase(y, NEU_LIST_ROW_H - 4, font), font, sel ? NEU_TEXT : NEU_TEXT2);
  if (kind === "dir") NEU_ICON.chevron(212, cy, NEU_TEXT3);
}
function neuListMessage(line1, line2, color = NEU_TEXT2) {
  neuText(line1, 120, 160, "B9", color, "C");
  if (line2) neuSmall(line2, 120, 172, NEU_TEXT3, "C");
}
