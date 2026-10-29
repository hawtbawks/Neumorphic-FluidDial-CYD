# FluidDial-CYD: neumorphic UI overhaul

FluidDial-CYD is a FluidNC CNC pendant on an ESP32 "Cheap Yellow Display" (CYD):
240x320 portrait touchscreen, RGB565, LovyanGFX, a rotary encoder (the "dial"),
and three hardware buttons (red E-stop, yellow pause, green cycle start). The
owner is redesigning the whole UI in a neumorphic style. Upstream:
https://github.com/dJOS1475/FluidDial-CYD

## Where things stand

Work is on branch `neu-ui`, on top of upstream `5dcc78b`.

1. **Done: static mockups**, approved. They live in `mockups/` (`build_gallery.py`
   builds `gallery.html`).
2. **Done: simulator.** Every screen in `simulator/` uses the new design and
   works with touch, dial and live state. It is the spec for the firmware.
3. **Done (awaiting panel checks): firmware port.** Every screen in
   `src/screens/` is on the kit (`neu_ui.h` / `neu_ui.cpp`). Checked on the
   panel: Main Menu, Jog, Status, Work Area, Overrides, Spindle. Not yet: Files,
   Macros, FluidNC, Connection, ESP-NOW, and the probe screens. The probe kit
   (port of `probe_kit.js`) lives in `screen_probe.h/.cpp`; every probe G-code
   function is unchanged from before the port.
   Panels taller than the 230x70 scratch repaint in bands: `neuListPaint()` for
   lists, two clipped passes for other wells. Focusable fields repaint with
   `neuFieldRepaint()` (ring direct, interior through the panel). Only the
   power-off and sleep paths in `CNC_Pendant_UI.cpp` still use `drawTitle()`;
   the tick picks the header style through `neuHeaderActive()`.
   Next: fix whatever the panel checks turn up (sim first, then firmware).

Decisions already made (don't reopen them):
- Renames: "Feeds & Speeds" becomes **Overrides**, "SD Card" becomes **Files**.
  Screen enums and file names stay as they are (`PSCREEN_FEEDS_SPEEDS`,
  `screen_sd_card.cpp`); only the user-facing text changes.
- New features to keep: **Go to XY0 / Z0** on Work Area (each behind a confirm
  dialog) and a **2x2 corner picker** on XYZ Corner.
- Fonts: switch from the GLCD 5x7 font to LovyanGFX's built-in FreeSans fonts.

## Running the simulator

```
python simulator/dev_server.py 8777      # then open http://localhost:8777
```

It reloads on every file change and keeps the current screen. The bench panel on
the right drives machine state (status, positions, link, axes, units). The dev
server also runs `sync.py`, which regenerates `js/colors.js` from the firmware's
colour `#define`s; after changing a firmware screen and its JS port, run
`python simulator/sync.py --accept` so its drift banner clears.

## Design system

Everything is drawn with fillRoundRect / fillRect / drawFastHLine / GFX text,
no anti-aliasing, so it ports 1:1 to LovyanGFX.

**Rules**
- Raised = something you can press. Inset = a readout, a field, or the selected
  option. Pressing a toggle moves it from raised to inset.
- Orange (ACCENT) means focus only: the selected option, the field the dial is
  adjusting (accent ring), the primary action.
- Machine state lives in the header chip on every screen: OFFLINE, SYNC, IDLE,
  RUN, HOLD, ALARM and so on, each with its colour and an LED dot.
- Home is always bottom-left in the nav bar; Back sits beside it.
- Start and Stop (Spindle) are the only solid-filled buttons.
- Anything a tap would set moving (Go to zero, Probe, Calibrate, Forget machine)
  goes through a confirm dialog.
- Every `update*()` keeps a change key and repaints only what changed. The
  100 ms tick must never redraw whole screens.

**Layout grid**
- Header y 0-29: title (FreeSansBold9) left; link bars + battery (wireless only)
  and the state chip right. Groove line at y 30.
- Content y 36-276, 10 px side margins, 8 px gaps (shadows need 3 px clear on
  every side).
- Groove at y 278; nav buttons at y 285, height 30.

**Tokens** (RGB888, converted with color565)

| Token | RGB | Use |
|---|---|---|
| BG | 44,48,56 | the one surface everything is extruded from |
| FACE | 47,51,60 | raised face |
| WELL | 37,40,47 | inset interior |
| SH_DARK | 24,26,31 | shadow (light comes from top-left) |
| SH_LIGHT | 70,76,88 | highlight |
| TEXT / TEXT2 / TEXT3 | 228,232,238 / 150,158,172 / 100,108,122 | text levels |
| ACCENT | 255,150,48 | focus only |
| IDLE / RUN / HOLD / ALARM | 96,208,200 / 92,220,128 / 255,190,64 / 255,84,84 | machine state |
| JOG / HOME | 110,168,255 / 176,140,255 | state |
| Axis X / Y / Z | 255,112,104 / 112,224,128 / 104,168,255 | A/B/C in `NEU_AX` |
| GO_FACE / STOP_FACE | 36,132,72 / 200,44,44 | Start / Stop only |

**Surfaces**
- `neuRaised(x,y,w,h,r,depth=3)`: for k = depth..1, fillRoundRect at (x+k,y+k)
  in a BG to SH_DARK ramp, then at (x-k,y-k) in a BG to SH_LIGHT ramp, then the
  face. 7 fillRoundRect calls.
- `neuInset(...)`: a rim (BG to SH_LIGHT at 0.8), then for k = 0..depth-1
  fillRoundRect(x+k, y+k, w-k-1, h-k-1) blending SH_DARK to WELL, then flat WELL.
  5 calls. The flat interior is (x+3,y+3)..(x+w-2,y+h-2); to repaint contents,
  fill WELL there and redraw. Don't redraw the shadow.
- Precompute the shadow ramps once (`neuInit()`); don't blend per draw.

**Fonts**
- FreeSans9pt7b, FreeSans12pt7b, FreeSansBold9/12/18/24pt7b (about 25 KB of
  flash). Keys in the JS: S9, S12, B9, B12, B18, B24.
- With a GFX font, cursor y is the **baseline**. LovyanGFX metrics: baseline =
  largest glyph ascent, fontHeight = largest ascent + largest descent.
  `neuCapBase(y,h,font)` = y + (h + capHeight('H'))/2 centres capitals in a box.
- Captions use the GLCD 5x7 font, upper-case, 7 px pitch (1 px extra
  tracking); y is the top. `neuSmall()` is plain GLCD 5x7 text.
- Long strings get shortened with a trailing ".." (`neuFit`); paths trim from the
  left so the file name survives.

## Simulator files (the spec)

- `simulator/js/neu_ui.js`: the kit. Tokens, neuRaised/neuInset/neuClear,
  header and chip (`neuHeader`, `neuHeaderUpdate` from the tick), nav bar
  (`neuNav`, `neuNavHit`, NAV_HOME, NAV_BACK), `neuButton`, `neuSegmented` +
  `neuSegHit`, `neuField` / `neuFieldValue`, list (`neuListFrame`, `neuListRow`,
  `neuListCrumb`), `neuDialog` + `neuDialogHit`, `neuHint`, the 18x18 icon set
  `NEU_ICON`, and the DRO (`neuDro`, `neuDroValues`, `neuDroKey`).
- `simulator/js/gfxfonts.js`: the FreeSans fonts converted from LovyanGFX;
  `simulator/js/lgfx.js` renders them.
- `simulator/js/screens/*.js`: one per screen. Each has enter/exit/draw/update/
  handle functions named like the firmware, with the layout in a comment at the top.
- `simulator/js/screens/probe_kit.js`: shared layout for the probe routines
  (position strip, sequence, diagram well, fields, result card that cycles the
  work coordinate system, hint, confirm).
- `simulator/js/sim.js`: screen registry, touch and dial dispatch (port of
  `CNC_Pendant_UI.cpp`). The 100 ms tick calls `neuHeaderUpdate()` on every screen.

## Porting notes

- Firmware panels draw through `beginPanelSprite()` / `endPanelSprite()`
  (`src/screens/pendant_shared.h`) to avoid flicker; the sim always uses the
  direct-draw fallback. When porting an `update*()`, draw the inset's interior
  (WELL background + contents) into the panel sprite. The shadow itself is drawn
  once in `draw*()` straight to the display.
- The sim keeps logic identical to the firmware where it can: dial handling, jog
  soft-limit clamp, override stepper, probe G-code, ESP-NOW flows. Port the
  drawing and touch layout. Keep the firmware's existing behaviour code unless a
  screen file in the simulator changes it on purpose (for example the Work Area
  go-to moves, the corner picker, or the jog feed toggle in the nav bar).
- `comms_active_mode()` returns the strings "UART" / "ESPNOW" in the sim
  (`connection.js`); in the firmware it's the enum. The header shows link bars
  and battery only on wireless.
- In LovyanGFX, `print()` treats cursor y as the TOP for GFX fonts. The kit
  draws FreeSans with `drawString` on the `baseline_left` datum, which matches
  the sim. Widths are summed `xAdvance` (as in the sim), not `textWidth()`.
  Use the kit's text functions, never `print()`, on neumorphic screens.
- In `update*()`, wrap repaints in `neuPanelBegin(x,y,w,h)` / `neuPanelEnd()`
  (nesting-safe). Read machine state through `neuDroSnapshot()` / `neuStatus()`,
  which take `stateMutex`.
- Build with `~/.platformio/penv/Scripts/pio.exe run -e cyd_new_ui` (pio is not
  on PATH). The first build downloads the toolchain and needs network.
- Build with the `cyd_new_ui` env (or `cyd_new_ui_combined` for resistive and
  capacitive panels) in `platformio.ini`.
- Check flash and heap after adding the fonts and the kit. Six fonts are about
  25 KB; the app partition is 2.5 MB (`partitions_cyd_noota.csv`).

## Working style

- The owner is Kyle, a maker who flashes and tests on a real pendant on a JD Mini
  Plasma table. Show screens rather than describe them: a screenshot of the
  simulator, or ask him to check it on the panel.
- Make each screen match the simulator. If something looks wrong on the panel,
  fix it in the simulator first, then the firmware, so the two stay in step.
- Commit on `neu-ui`, one screen (or the kit) per commit.
