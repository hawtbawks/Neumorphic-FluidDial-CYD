#!/usr/bin/env python3
"""Build mockups/gallery.html: a self-contained before/after gallery of the neumorphic mockups."""
import base64, os, json
H = os.path.dirname(os.path.abspath(__file__))
def rd(p): return open(os.path.join(H, p)).read()
js = "\n".join(rd(p) for p in ["../simulator/js/colors.js", "../simulator/js/font.js", "../simulator/js/lgfx.js",
                               "js/gfxfonts.js", "js/neu.js", "js/screens.js"])
SCREENS = [  # fn, opts, title, before-png, note
 ("scrMainMenu", {}, "Main Menu", "MAIN_MENU", "DRO card replaces the big status word; machine state moves to the header chip on every screen. Eight tiles with icons, single-word labels."),
 ("scrMainMenu", {"state": "Alarm:1"}, "Main Menu · Alarm", None, "In Alarm the DRO card becomes an alarm card with the decoded reason. The chip turns red everywhere."),
 ("scrStatus", {}, "Status · running", "STATUS", "State, job progress, DRO and feed/spindle in one screen. Shown mid-job."),
 ("scrJog", {}, "Jog", "JOG_HOMING", "The DRO rows are the axis selector: tap a row, the dial jogs it. Step is a segmented control; Home buttons use axis colours."),
 ("scrWork", {}, "Work Area", "PROBING_WORK", "WCS segmented control, DRO, zero buttons. Go to XY0 / Z0 is new and optional."),
 ("scrProbe", {}, "Probe · Z Plate", "PROBE", "Plate type as segmented control, shared settings as inset fields, routine as one big raised button."),
 ("scrProbe", {"mode": 2}, "Probe · 3D", None, "3D probe mode lists all four routines."),
 ("scrProbeZ", {}, "Z Surface", "PROBE_Z", "Shared routine layout: position strip, sequence, diagram, fields, result, hint. The outlined field is the dial target."),
 ("scrProbeCorner", {}, "XYZ Corner", "PROBE_CORNER", "Corner picker becomes a 2×2 grid instead of a text field."),
 ("scrProbeBore", {}, "Bore", "PROBE_BORE", "Step 2 active: done steps turn green, current is orange."),
 ("scrProbeBoss", {}, "Boss", None, "Same template as Bore."),
 ("scrProbeCfg", {}, "Plate Setup", "PROBE_CFG_PLATE", "Large thickness field with a dimensioned diagram."),
 ("scrFeeds", {}, "Overrides", "FEEDS_SPEEDS", "Renamed from Feeds & Speeds. Big % readout with the actual feed/RPM beside it; presets below. Reset in the nav bar."),
 ("scrSpindle", {}, "Spindle", "SPINDLE_CONTROL", "Actual vs target, direction, presets. Start and Stop are the only solid-filled buttons in the UI."),
 ("scrMacros", {}, "Macros", "MACROS", "Raised rows; the selected row is pressed in and shows its command."),
 ("scrFiles", {}, "Files", "SD_CARD", "Renamed from SD Card. List in one well, selection is a raised pill. Long names truncate with ..."),
 ("scrFluidNC", {}, "FluidNC", "FLUIDNC", "Key/value info list plus settings rows with chevrons."),
 ("scrConnection", {}, "Connection", "CONNECTION", "Transport toggle, link card with signal and counters, pairing actions."),
]
cards = []
for fn, opts, title, before, note in SCREENS:
    b = ""
    if before:
        b = "data:image/png;base64," + base64.b64encode(open(os.path.join(H, "before", before + ".png"), "rb").read()).decode()
    cards.append(dict(fn=fn, opts=opts, title=title, before=b, note=note))
html = rd("gallery_template.html").replace("/*__JS__*/", js).replace("/*__CARDS__*/", json.dumps(cards))
open(os.path.join(H, "gallery.html"), "w").write(html)
print("gallery.html", len(html) // 1024, "KB")
