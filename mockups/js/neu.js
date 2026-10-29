/*
 * neu.js — device-faithful neumorphic renderer for the FluidDial-CYD mockups.
 *
 * Extends the simulator's LGFX canvas engine (no AA, RGB565, Adafruit-GFX shapes)
 * with:
 *   • Adafruit-GFX / LovyanGFX "GFXfont" rendering (FreeSans*), using the same
 *     metrics LovyanGFX computes in GFXfont::getDefaultMetric():
 *       baseline = max ascent over all glyphs, height = max ascent + max descent.
 *   • Neumorphic primitives built ONLY from fillRoundRect calls, so every one of
 *     them ports 1:1 to LovyanGFX C++ (see raised()/inset() comments for cost).
 *
 * Every colour goes through color565() so the mockup shows the panel's real
 * RGB565 banding.
 */

// ---------- colour ----------
function rgb(r, g, b) { return (((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3)) & 0xffff; }
function c565to888(c) {
  return [Math.round(((c >> 11) & 31) * 255 / 31), Math.round(((c >> 5) & 63) * 255 / 63), Math.round((c & 31) * 255 / 31)];
}
// Linear blend of two RGB565 colours, t=0 → a, t=1 → b (what a C++ lerp565() would do).
function mix(a, b, t) {
  const A = c565to888(a), B = c565to888(b);
  return rgb(A[0] + (B[0] - A[0]) * t, A[1] + (B[1] - A[1]) * t, A[2] + (B[2] - A[2]) * t);
}

// ---------- design tokens ----------
const NEU = {
  BG:        rgb(44, 48, 56),     // the one surface everything is extruded from
  FACE:      rgb(47, 51, 60),     // raised face — a hair lighter than BG reads as convex
  WELL:      rgb(37, 40, 47),     // inset face
  SH_DARK:   rgb(24, 26, 31),     // shadow (light source top-left)
  SH_LIGHT:  rgb(70, 76, 88),     // highlight
  TEXT:      rgb(228, 232, 238),
  TEXT2:     rgb(150, 158, 172),
  TEXT3:     rgb(100, 108, 122),
  ACCENT:    rgb(255, 150, 48),   // FluidNC orange — selection / focus only
  // machine-state + semantic colours (used as text, LEDs and thin bars — never big fills)
  IDLE:      rgb(96, 208, 200),
  RUN:       rgb(92, 220, 128),
  HOLD:      rgb(255, 190, 64),
  ALARM:     rgb(255, 84, 84),
  JOG:       rgb(110, 168, 255),
  HOME:      rgb(176, 140, 255),
  // axis colours (CNC convention)
  AX_X:      rgb(255, 112, 104),
  AX_Y:      rgb(112, 224, 128),
  AX_Z:      rgb(104, 168, 255),
  // the two exceptions to "no big fills": STOP and START keep solid faces
  STOP_FACE: rgb(200, 44, 44),
  GO_FACE:   rgb(36, 132, 72),
};
const STATE_COLOR = { Idle: NEU.IDLE, Run: NEU.RUN, Jog: NEU.JOG, Home: NEU.HOME, Hold: NEU.HOLD, Door: NEU.HOLD, Alarm: NEU.ALARM, Check: NEU.IDLE, Sleep: NEU.TEXT3 };
function stateColor(s) { return STATE_COLOR[String(s).split(":")[0]] || NEU.TEXT2; }

// ---------- font table ----------
const FONTS = {
  S9: "FreeSans9pt7b", S12: "FreeSans12pt7b",
  B9: "FreeSansBold9pt7b", B12: "FreeSansBold12pt7b", B18: "FreeSansBold18pt7b", B24: "FreeSansBold24pt7b",
};
function gfxMetrics(f) {
  if (f._m) return f._m;
  let ab = 0, bb = 0;
  for (const g of f.glyphs) { const a = -g[5]; if (a > ab) ab = a; const b = g[2] - a; if (b > bb) bb = b; }
  const H = f.glyphs["H".charCodeAt(0) - f.first];
  f._m = { baseline: ab, height: ab + bb, cap: H[2] };
  return f._m;
}

class NeuGFX extends LGFX {
  constructor(ctx, w, h) { super(ctx, w, h); this._font = null; }
  setFont(key) { this._font = key ? GFXFONTS[FONTS[key] || key] : null; }

  fontHeight() { return this._font ? gfxMetrics(this._font).height * this._textSize : 8 * this._textSize; }
  textWidth(s) {
    s = String(s);
    if (!this._font) return s.length * 6 * this._textSize;
    const f = this._font; let w = 0;
    for (const ch of s) { const c = ch.charCodeAt(0); if (c >= f.first && c <= f.last) w += f.glyphs[c - f.first][3]; }
    return w * this._textSize;
  }
  _drawText(s) {
    if (!this._font) return super._drawText(s);
    // GFX fonts: cursor y is the BASELINE (Adafruit semantics)
    const f = this._font, ctx = this.ctx, sz = this._textSize;
    ctx.fillStyle = rgb565ToCss(this._textColor);
    for (const ch of s) {
      const c = ch.charCodeAt(0);
      if (c < f.first || c > f.last) continue;
      const [off, w, h, adv, xo, yo] = f.glyphs[c - f.first];
      let bit = 0, bits = 0, p = off;
      for (let yy = 0; yy < h; yy++) for (let xx = 0; xx < w; xx++) {
        if (!(bit++ & 7)) bits = f.bitmap[p++];
        if (bits & 0x80) ctx.fillRect(this._cx + (xo + xx) * sz, this._cy + (yo + yy) * sz, sz, sz);
        bits <<= 1;
      }
      this._cx += adv * sz;
    }
  }
}

// ---------- drawing API used by the mockups (each maps to a small C++ helper) ----------
let G;            // the active NeuGFX
function useGfx(g) { G = g; }

// text(s, x, baselineY, font, color, align)   align: "L" | "C" | "R"
// Baseline positioning == LovyanGFX setTextDatum(baseline_left/center/right).
// font "G" / "G2" = built-in GLCD 5x7 at size 1/2 (y is then the TOP, as on device).
function text(s, x, y, font, color, align = "L") {
  s = String(s);
  if (font === "G" || font === "G2") {
    G.setFont(null); G.setTextSize(font === "G2" ? 2 : 1);
  } else { G.setFont(font); G.setTextSize(1); }
  const w = G.textWidth(s);
  const x0 = align === "C" ? x - (w / 2 | 0) : align === "R" ? x - w : x;
  G.setTextColor(color); G.setCursor(x0, y); G.print(s);
  return w;
}
// Truncate to fit maxW with a trailing "..", as a C++ fitText() helper would.
function fit(s, font, maxW) {
  s = String(s); if (tw(s, font) <= maxW) return s;
  while (s.length > 1 && tw(s + "..", font) > maxW) s = s.slice(0, -1);
  return s + "..";
}
function tw(s, font) {
  if (font === "G" || font === "G2") { G.setFont(null); G.setTextSize(font === "G2" ? 2 : 1); }
  else { G.setFont(font); G.setTextSize(1); }
  return G.textWidth(String(s));
}
// Baseline that visually centres CAPITALS in a box: y + (h + capHeight)/2
function capBase(y, h, font) { return y + ((h + gfxMetrics(GFXFONTS[FONTS[font]]).cap) / 2 | 0); }
function textIn(s, x, y, w, h, font, color, align = "C") {
  const bx = align === "C" ? x + (w / 2 | 0) : align === "R" ? x + w : x;
  return text(s, bx, capBase(y, h, font), font, color, align);
}
// Small caption: GLCD 5x7, letter-spaced 1px, upper-case.  Crisp at this size where
// FreeSans 9pt would be too big — the one place the old font is kept on purpose.
function caption(s, x, y, color = NEU.TEXT3, align = "L") {
  s = String(s).toUpperCase();
  G.setFont(null); G.setTextSize(1); G.setTextColor(color);
  const w = s.length * 7 - 1;
  let cx = align === "C" ? x - (w / 2 | 0) : align === "R" ? x - w : x;
  for (const ch of s) { G.setCursor(cx, y); G.print(ch); cx += 7; }
  return w;
}

/* raised(): convex surface.  Light from top-left.
 * Cost on device: 2*depth + 1 fillRoundRect.  Needs `depth` px clear around it.
 *   for k = depth..1: fillRoundRect(x+k, y+k, w, h, r, lerp(BG, SH_DARK, (depth-k+1)/depth))
 *   for k = depth..1: fillRoundRect(x-k, y-k, w, h, r, lerp(BG, SH_LIGHT,(depth-k+1)/depth))
 *   fillRoundRect(x, y, w, h, r, FACE)                                                    */
function raised(x, y, w, h, r = 10, depth = 3, face = NEU.FACE) {
  for (let k = depth; k >= 1; k--) G.fillRoundRect(x + k, y + k, w, h, r, mix(NEU.BG, NEU.SH_DARK, (depth - k + 1) / depth));
  for (let k = depth; k >= 1; k--) G.fillRoundRect(x - k, y - k, w, h, r, mix(NEU.BG, NEU.SH_LIGHT, (depth - k + 1) / depth));
  G.fillRoundRect(x, y, w, h, r, face);
}
/* inset(): concave well (fields, selected toggles, readouts).
 * Cost on device: depth + 2 fillRoundRect, drawn entirely inside the box.
 *   fillRoundRect(x, y, w, h, r, lerp(BG, SH_LIGHT, .8))             // 1px light rim, bottom-right
 *   for k = 0..depth-1: fillRoundRect(x+k, y+k, w-k-1, h-k-1, r, lerp(SH_DARK, WELL, k/depth))
 *   fillRoundRect(x+depth, y+depth, w-depth-1, h-depth-1, r, WELL)                         */
function inset(x, y, w, h, r = 10, depth = 3, well = NEU.WELL) {
  G.fillRoundRect(x, y, w, h, r, mix(NEU.BG, NEU.SH_LIGHT, 0.8));
  for (let k = 0; k < depth; k++) G.fillRoundRect(x + k, y + k, w - k - 1, h - k - 1, r, mix(NEU.SH_DARK, well, k / depth));
  G.fillRoundRect(x + depth, y + depth, w - depth - 1, h - depth - 1, r, well);
}
// A 2-line groove separating header / nav from content (dark over light = carved).
function groove(y, x0 = 8, x1 = 232) { G.drawFastHLine(x0, y, x1 - x0, NEU.SH_DARK); G.drawFastHLine(x0, y + 1, x1 - x0, NEU.SH_LIGHT); }
function led(cx, cy, color, r = 3) {
  G.fillCircle(cx, cy, r + 2, mix(NEU.WELL, color, 0.22));   // glow
  G.fillCircle(cx, cy, r, color);
  G.fillRect(cx - 1, cy - 1, 1, 1, mix(color, NEU.TEXT, 0.6)); // specular dot
}

// ---- composite components ----
// Button: raised when idle, inset when on (toggle) or pressed.
// opts: { on, color (label colour), font, icon(fn(cx,cy,col)), sub (small 2nd line) }
function button(x, y, w, h, label, o = {}) {
  const font = o.font || "B9";
  if (o.on) inset(x, y, w, h, o.r ?? 10, 3); else raised(x, y, w, h, o.r ?? 10, o.depth ?? 3, o.face);
  const col = o.color ?? (o.on ? NEU.ACCENT : NEU.TEXT);
  if (o.icon && label) {
    const lw = tw(label, font), total = 18 + 6 + lw, sx = x + ((w - total) / 2 | 0);
    o.icon(sx + 9, y + (h / 2 | 0), col);
    text(label, sx + 24, capBase(y, h, font), font, col);
  } else if (o.icon) {
    o.icon(x + (w / 2 | 0), y + (h / 2 | 0), col);
  } else if (o.sub) {
    text(label, x + (w / 2 | 0), y + (h / 2 | 0) + 1, font, col, "C");
    caption(o.sub, x + (w / 2 | 0), y + (h / 2 | 0) + 6, o.on ? mix(NEU.ACCENT, NEU.WELL, .35) : NEU.TEXT3, "C");
  } else textIn(label, x, y, w, h, font, col);
  if (o.on && o.bar !== false) G.fillRoundRect(x + 8, y + h - 6, w - 17, 2, 1, NEU.ACCENT);   // focus bar
}
// Segmented control: one inset track, the selected segment is a raised pill inside it.
function segmented(x, y, w, h, items, sel, o = {}) {
  inset(x, y, w, h, o.r ?? 10, 3);
  const pad = 5, n = items.length, gap = 2;
  const sw = ((w - 2 * pad - (n - 1) * gap) / n) | 0;
  items.forEach((it, i) => {
    const sx = x + pad + i * (sw + gap), lastW = i === n - 1 ? x + w - pad - sx : sw;
    if (i === sel) raised(sx, y + pad, lastW, h - 2 * pad, (o.r ?? 10) - 3, 2);
    textIn(it, sx, y + pad, lastW, h - 2 * pad, o.font || "B9", i === sel ? (o.selColor ?? NEU.ACCENT) : NEU.TEXT2);
  });
}
// Fix unit glyph baseline for GLCD (top-based): draw unit aligned to value baseline
function unitText(u, x, base) { caption(u, x, base - 7, NEU.TEXT3); }
// Value field: inset well with caption + value + unit.  focus → accent outline (dial target).
function field(x, y, w, h, cap, value, unit, o = {}) {
  inset(x, y, w, h, 8, 3);
  if (o.focus) G.drawRoundRect(x - 3, y - 3, w + 6, h + 6, 11, NEU.ACCENT);
  caption(cap, x + 9, y + 7, o.focus ? NEU.ACCENT : NEU.TEXT3);
  const vf = o.font || "B12", base = y + h - 7;
  if (o.align === "R") {
    const uw = unit ? unit.length * 7 + 3 : 0;
    if (unit) unitText(unit, x + w - 9 - uw + 3, base);
    text(value, x + w - 9 - uw, base, vf, o.color ?? NEU.TEXT, "R");
  } else {
    const vw = text(value, x + 9, base, vf, o.color ?? NEU.TEXT);
    if (unit) unitText(unit, x + 9 + vw + 4, base);
  }
}
function progress(x, y, w, h, frac, color = NEU.ACCENT) {
  inset(x, y, w, h, h / 2 | 0, 2);
  const fw = Math.max(h - 4, ((w - 4) * frac) | 0);
  if (frac > 0) G.fillRoundRect(x + 2, y + 2, fw, h - 4, (h - 4) / 2 | 0, color);
}

// ---------- icons (18x18 box, centred on cx,cy, 2px strokes) ----------
function L2(x0, y0, x1, y1, c) { G.drawLine(x0, y0, x1, y1, c); G.drawLine(x0 + 1, y0, x1 + 1, y1, c); G.drawLine(x0, y0 + 1, x1, y1 + 1, c); }
const ICON = {
  home(cx, cy, c) { L2(cx - 8, cy, cx, cy - 8, c); L2(cx, cy - 8, cx + 7, cy - 1, c); G.fillRect(cx - 6, cy, 2, 8, c); G.fillRect(cx + 4, cy, 2, 8, c); G.fillRect(cx - 6, cy + 7, 12, 2, c); G.fillRect(cx - 1, cy + 3, 3, 5, c); },
  back(cx, cy, c) { L2(cx + 3, cy - 7, cx - 4, cy, c); L2(cx - 4, cy, cx + 3, cy + 7, c); },
  fwd(cx, cy, c) { L2(cx - 4, cy - 7, cx + 3, cy, c); L2(cx + 3, cy, cx - 4, cy + 7, c); },
  play(cx, cy, c) { for (let i = 0; i < 9; i++) G.drawFastVLine(cx - 4 + i, cy - 8 + i, 17 - 2 * i, c); },
  stop(cx, cy, c) { G.fillRoundRect(cx - 6, cy - 6, 13, 13, 2, c); },
  jog(cx, cy, c) {
    const tri = (dx, dy) => { for (let i = 0; i < 4; i++) { if (dx) G.drawFastVLine(cx + dx * (8 - i), cy - i, 2 * i + 1, c); else G.drawFastHLine(cx - i, cy + dy * (8 - i), 2 * i + 1, c); } };
    tri(1, 0); tri(-1, 0); tri(0, 1); tri(0, -1); G.fillRect(cx - 5, cy - 1, 11, 2, c); G.fillRect(cx - 1, cy - 5, 2, 11, c);
  },
  origin(cx, cy, c) { G.drawCircle(cx, cy, 7, c); G.drawCircle(cx, cy, 6, c); G.fillRect(cx - 9, cy - 1, 19, 2, c); G.fillRect(cx - 1, cy - 9, 2, 19, c); G.fillCircle(cx, cy, 2, c); },
  gauge(cx, cy, c) { for (let a = 0; a <= 180; a += 4) { const t = Math.PI + a * Math.PI / 180; for (const r of [8, 9]) G.fillRect(Math.round(cx + Math.cos(t) * r), Math.round(cy + 3 + Math.sin(t) * r), 1, 1, c); } L2(cx, cy + 3, cx + 5, cy - 3, c); G.fillCircle(cx, cy + 3, 2, c); },
  spindle(cx, cy, c) { G.fillRect(cx - 5, cy - 8, 11, 5, c); G.fillRect(cx - 3, cy - 3, 7, 5, c); L2(cx - 1, cy + 2, cx - 1, cy + 9, c); L2(cx - 6, cy + 5, cx - 1, cy + 9, c); L2(cx + 5, cy + 5, cx, cy + 9, c); },
  macro(cx, cy, c) { L2(cx + 2, cy - 9, cx - 4, cy + 1, c); G.fillRect(cx - 4, cy, 9, 2, c); L2(cx + 4, cy + 1, cx - 2, cy + 9, c); },
  card(cx, cy, c) { G.drawRoundRect(cx - 6, cy - 8, 13, 17, 2, c); G.drawRoundRect(cx - 5, cy - 7, 11, 15, 2, c); for (let i = 0; i < 3; i++) G.fillRect(cx - 3 + i * 3, cy - 6, 1, 4, c); },
  probe(cx, cy, c) { G.fillRect(cx - 4, cy - 9, 9, 5, c); G.fillRect(cx - 1, cy - 4, 3, 9, c); G.fillCircle(cx, cy + 6, 2, c); G.fillRect(cx - 8, cy + 9, 17, 1, c); },
  status(cx, cy, c) { G.fillRect(cx - 7, cy + 1, 3, 7, c); G.fillRect(cx - 2, cy - 3, 3, 11, c); G.fillRect(cx + 3, cy - 8, 3, 16, c); },
  folder(cx, cy, c) { G.fillRoundRect(cx - 8, cy - 5, 7, 4, 1, c); G.fillRoundRect(cx - 8, cy - 3, 17, 11, 2, c); },
  file(cx, cy, c) { G.drawRect(cx - 6, cy - 8, 12, 17, c); G.drawRect(cx - 5, cy - 7, 10, 15, c); G.fillRect(cx - 3, cy - 3, 6, 1, c); G.fillRect(cx - 3, cy, 6, 1, c); G.fillRect(cx - 3, cy + 3, 4, 1, c); },
  up(cx, cy, c) { L2(cx - 7, cy + 3, cx, cy - 4, c); L2(cx, cy - 4, cx + 7, cy + 3, c); },
  refresh(cx, cy, c) { for (let a = 0; a < 290; a += 4) { const t = a * Math.PI / 180; for (const r of [6, 7]) G.fillRect(Math.round(cx + Math.cos(t) * r), Math.round(cy + Math.sin(t) * r), 1, 1, c); } for (let i = 0; i < 4; i++) G.drawFastHLine(cx + 1, cy - 9 + i, 7 - 2 * i, c); },
  gear(cx, cy, c) { for (let a = 0; a < 8; a++) { const t = a * Math.PI / 4; G.fillRect(Math.round(cx + Math.cos(t) * 7) - 1, Math.round(cy + Math.sin(t) * 7) - 1, 3, 3, c); } G.drawCircle(cx, cy, 5, c); G.drawCircle(cx, cy, 4, c); G.drawCircle(cx, cy, 6, c); },
  link(cx, cy, c) { G.drawRoundRect(cx - 9, cy - 3, 10, 7, 3, c); G.drawRoundRect(cx, cy - 3, 10, 7, 3, c); },
  plus(cx, cy, c) { G.fillRect(cx - 6, cy - 1, 13, 3, c); G.fillRect(cx - 1, cy - 6, 3, 13, c); },
  minus(cx, cy, c) { G.fillRect(cx - 6, cy - 1, 13, 3, c); },
};
