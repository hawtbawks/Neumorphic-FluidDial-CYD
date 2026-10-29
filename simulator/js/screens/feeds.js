/* screen_feeds_speeds.cpp (Overrides) — neumorphic layout
 *   feed card     y 38  h 50   big override % + actual feed; tap = dial adjusts it (accent ring)
 *   feed presets  y 96  h 34   50 / 75 / 100 / 125 / 150 (selected = the preset matching the value)
 *   spindle card  y 146 h 50   same, for spindle override + actual RPM
 *   spindle presets y 204 h 34
 *   nav           Home | Reset to 100%
 */

const OVR_PRESETS = [50, 75, 100, 125, 150];
const OVR_FEED_Y = 38, OVR_SPIN_Y = 146, OVR_CARD_H = 50, OVR_SEG_OFF = 58, OVR_SEG_H = 34;
const FEEDS_NAV = [NAV_HOME, { id: "reset", label: "Reset to 100%", icon: "refresh" }];
let _ovrKeys = {};

function enterFeedsSpeeds() { releasePanelSprites(); _ovrKeys = {}; }
function exitFeedsSpeeds() { pendantFeeds.dialMode = 0; releasePanelSprites(); }

function drawFeedsSpeedsScreen() {
  neuHeader("Overrides");
  _ovrKeys = {};
  neuInset(10, OVR_FEED_Y, 220, OVR_CARD_H, 12, 3);
  neuInset(10, OVR_SPIN_Y, 220, OVR_CARD_H, 12, 3);
  redrawFeedOverrideButtons();
  redrawSpindleOverrideButtons();
  neuCaption("Tap a card, then turn the dial", 120, 254, NEU_TEXT3, "C");
  neuNav(FEEDS_NAV);
}

// What the card shows: the target while the stepper is still ramping, else the reported value.
function _ovrShown(s, reported) { return s.target >= 0 ? s.target : reported; }

function _ovrCard(y, cap, pct, actual, unit, focus) {
  display.drawRoundRect(7, y - 3, 226, OVR_CARD_H + 6, 15, focus ? NEU_ACCENT : NEU_BG);
  display.drawRoundRect(8, y - 2, 224, OVR_CARD_H + 4, 14, focus ? NEU_ACCENT : NEU_BG);
  display.fillRect(14, y + 4, 213, OVR_CARD_H - 7, NEU_WELL);
  neuCaption(focus ? cap + " - dial" : cap, 22, y + 8, focus ? NEU_ACCENT : NEU_TEXT3);
  neuCaption(unit, 218, y + 8, NEU_TEXT3, "R");
  neuText(pct + "%", 22, y + 44, "B18", NEU_TEXT);
  neuText(actual, 218, y + 40, "B12", NEU_TEXT2, "R");
}
function updateFeedsSpeedsTopDisplay() { updateFeedOverrideDisplay(); updateSpindleOverrideDisplay(); }

function updateFeedOverrideDisplay() {
  if (currentPendantScreen !== PSCREEN_FEEDS_SPEEDS) return;
  const pct = _ovrShown(_feedOvr, pendantMachine.feedOverride), focus = pendantFeeds.dialMode === 1;
  const key = pct + "|" + focus + "|" + pendantMachine.feedRate + "|" + pendantMachine.inInches;
  if (_ovrKeys.feed === key) return;
  const presetChanged = _ovrKeys.feedPct !== pct;
  _ovrKeys.feed = key; _ovrKeys.feedPct = pct;
  _ovrCard(OVR_FEED_Y, "Feed override", pct, String(Math.round(pendantMachine.feedRate)), pendantMachine.inInches ? "in/min" : "mm/min", focus);
  if (presetChanged) _ovrPresets(OVR_FEED_Y + OVR_SEG_OFF, pct);
}
function updateSpindleOverrideDisplay() {
  if (currentPendantScreen !== PSCREEN_FEEDS_SPEEDS) return;
  const pct = _ovrShown(_spindleOvr, pendantMachine.spindleOverride), focus = pendantFeeds.dialMode === 2;
  const key = pct + "|" + focus + "|" + pendantMachine.spindleRPM;
  if (_ovrKeys.spin === key) return;
  const presetChanged = _ovrKeys.spinPct !== pct;
  _ovrKeys.spin = key; _ovrKeys.spinPct = pct;
  _ovrCard(OVR_SPIN_Y, "Spindle override", pct, String(Math.round(pendantMachine.spindleRPM)), "rpm", focus);
  if (presetChanged) _ovrPresets(OVR_SPIN_Y + OVR_SEG_OFF, pct);
}
function _ovrPresets(y, pct) {
  neuClear(10, y, 220, OVR_SEG_H);
  neuSegmented(10, y, 220, OVR_SEG_H, OVR_PRESETS.map(String), OVR_PRESETS.indexOf(pct));
}
function redrawFeedOverrideButtons() { _ovrKeys.feed = _ovrKeys.feedPct = undefined; updateFeedOverrideDisplay(); }
function redrawSpindleOverrideButtons() { _ovrKeys.spin = _ovrKeys.spinPct = undefined; updateSpindleOverrideDisplay(); }

// Paced coarse+fine override stepper (mirrors CNC_Pendant_UI.cpp). A button/dial
// sets a TARGET; the driver below walks toward it from the current reported value,
// one real-time byte every OVR_STEP_MS — never a burst. In the sim (no real
// FluidNC) each step also updates pendantMachine.*Override to mimic the reported
// value ramping.
const OVR_STEP_MS = 60;
const _feedOvr = { target: -1, commanded: 100, lastSendMs: 0, active: false };
const _spindleOvr = { target: -1, commanded: 100, lastSendMs: 0, active: false };

function _ovrSetTarget(s, reported, target) {
  target = constrain(target, 10, 200);
  if (!s.active) { s.commanded = constrain(reported, 10, 200); s.active = true; }
  s.target = target;
}
function overrideSetFeedTarget(pct) { _ovrSetTarget(_feedOvr, pendantMachine.feedOverride, pct); }
function overrideSetSpindleTarget(pct) { _ovrSetTarget(_spindleOvr, pendantMachine.spindleOverride, pct); }

function _ovrStep(s, now, cP, cM, fP, fM, applyReported) {
  if (s.target < 0) return false;
  if (s.commanded === s.target) { s.target = -1; s.active = false; return false; }
  if (now - s.lastSendMs < OVR_STEP_MS) return false;
  s.lastSendMs = now;
  const gap = s.target - s.commanded;
  if (gap >= 10) { fnc_realtime(cP); s.commanded += 10; }
  else if (gap <= -10) { fnc_realtime(cM); s.commanded -= 10; }
  else if (gap > 0) { fnc_realtime(fP); s.commanded += 1; }
  else { fnc_realtime(fM); s.commanded -= 1; }
  applyReported(s.commanded);   // sim: simulate FluidNC reporting the new override
  return true;
}

// Drive the ramp (the firmware does this in its periodic loop).
function _ovrDrive() {
  if (!pendantConnected) return;
  const now = Date.now();
  let changed = _ovrStep(_feedOvr, now, FeedOvrCoarsePlus, FeedOvrCoarseMinus, FeedOvrFinePlus, FeedOvrFineMinus, v => pendantMachine.feedOverride = v);
  changed = _ovrStep(_spindleOvr, now, SpindleOvrCoarsePlus, SpindleOvrCoarseMinus, SpindleOvrFinePlus, SpindleOvrFineMinus, v => pendantMachine.spindleOverride = v) || changed;
  if (changed && currentPendantScreen === PSCREEN_FEEDS_SPEEDS) { updateFeedOverrideDisplay(); updateSpindleOverrideDisplay(); }
}
if (typeof window !== "undefined" && !window._ovrDriveTimer) window._ovrDriveTimer = setInterval(_ovrDrive, 20);

function applyFeedOverride(targetPct) {
  if (!pendantConnected) return;
  overrideSetFeedTarget(targetPct);
}
function applySpindleOverride(targetPct) {
  if (!pendantConnected) return;
  overrideSetSpindleTarget(targetPct);
}

function handleFeedsSpeedsTouch(x, y) {
  const fSeg = neuSegHit(10, OVR_FEED_Y + OVR_SEG_OFF, 220, OVR_SEG_H, 5, x, y);
  if (fSeg >= 0) {
    pendantFeeds.dialMode = 0; pendantFeeds.selectedFeedOverride = fSeg;
    applyFeedOverride(OVR_PRESETS[fSeg]); redrawFeedOverrideButtons(); updateSpindleOverrideDisplay();
    return;
  }
  const sSeg = neuSegHit(10, OVR_SPIN_Y + OVR_SEG_OFF, 220, OVR_SEG_H, 5, x, y);
  if (sSeg >= 0) {
    pendantFeeds.dialMode = 0; pendantFeeds.selectedSpindleOverride = sSeg;
    applySpindleOverride(OVR_PRESETS[sSeg]); redrawSpindleOverrideButtons(); updateFeedOverrideDisplay();
    return;
  }
  if (isTouchInBounds(x, y, 10, OVR_FEED_Y, 220, OVR_CARD_H)) {
    pendantFeeds.dialMode = pendantFeeds.dialMode === 1 ? 0 : 1;
    updateFeedOverrideDisplay(); updateSpindleOverrideDisplay();
    return;
  }
  if (isTouchInBounds(x, y, 10, OVR_SPIN_Y, 220, OVR_CARD_H)) {
    pendantFeeds.dialMode = pendantFeeds.dialMode === 2 ? 0 : 2;
    updateFeedOverrideDisplay(); updateSpindleOverrideDisplay();
    return;
  }
  const n = neuNavHit(FEEDS_NAV, x, y);
  if (n === "home") currentPendantScreen = PSCREEN_MAIN_MENU;
  else if (n === "reset") {
    pendantFeeds.dialMode = 0; pendantFeeds.selectedFeedOverride = 2; pendantFeeds.selectedSpindleOverride = 2;
    applyFeedOverride(100); applySpindleOverride(100);
    updateFeedOverrideDisplay(); updateSpindleOverrideDisplay();
  }
}
