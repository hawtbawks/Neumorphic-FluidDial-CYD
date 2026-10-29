/* screen_status.cpp — neumorphic layout
 *   state card  y 38 h 50   LED + state word; progress % on the right while a job runs
 *   job card    y 96 h 52   current / loaded file + progress bar
 *   DRO         y 156       work + machine
 *   feed/spindle fields y 232 h 40
 *   nav         Home | FluidNC
 */

const STATUS_NAV = [NAV_HOME, { id: "fluidnc", label: "FluidNC", icon: "gear" }];
let _statusKeys = {};

function enterStatus() { releasePanelSprites(); _statusKeys = {}; }
function exitStatus() { releasePanelSprites(); }

function drawStatusScreen() {
  neuHeader("Status");
  _statusKeys = {};
  neuInset(10, 38, 220, 50, 12, 3);
  neuInset(10, 96, 220, 52, 12, 3);
  neuInset(10, 156, 220, NEU_DRO_H, 12, 3);
  neuInset(10, 232, 106, 40, 8, 3);
  neuInset(124, 232, 106, 40, 8, 3);
  updateStatusMachineStatus();
  updateStatusCurrentFile();
  updateStatusAxisPositions();
  updateStatusFeedSpindle();
  neuNav(STATUS_NAV);
}

function updateStatusMachineStatus() {
  if (currentPendantScreen !== PSCREEN_STATUS) return;
  const st = pendantMachine.status, job = pendantMachine.currentFile.length > 0;
  const key = st + pendantConnected + pendantSynced + job + pendantMachine.jobPercent;
  if (_statusKeys.state === key) return;
  _statusKeys.state = key;
  display.fillRect(13, 41, 214, 44, NEU_WELL);
  const chip = neuChipState();
  if (st.startsWith("Alarm") && pendantSynced && pendantConnected) {
    neuLed(28, 56, NEU_ALARM, 4);
    neuText("Alarm", 42, 64, "B12", NEU_ALARM);
    neuSmall(alarmDescription(st), 22, 73, NEU_TEXT2);
    return;
  }
  const word = chip.label === "OFFLINE" ? "Connecting" : chip.label === "SYNC" ? "Syncing" : st.split(":")[0];
  neuLed(28, 63, chip.color, 5);
  // Big when it fits; a long word ("Connecting") steps down a size rather than being cut short.
  const maxW = job ? 120 : 176, font = neuTextW(word, "B18") <= maxW ? "B18" : "B12";
  neuText(neuFit(word, font, maxW), 42, neuCapBase(38, 50, font), font, chip.color);
  if (job) {
    neuCaption("Progress", 218, 50, NEU_TEXT3, "R");
    neuText(pendantMachine.jobPercent + "%", 218, 76, "B12", NEU_TEXT, "R");
  }
}

function updateStatusCurrentFile() {
  if (currentPendantScreen !== PSCREEN_STATUS) return;
  const file = pendantMachine.currentFile, loaded = pendantSdCard.loadedFile;
  const key = file + "|" + loaded + "|" + pendantMachine.jobPercent;
  if (_statusKeys.file === key) return;
  _statusKeys.file = key;
  display.fillRect(13, 99, 214, 46, NEU_WELL);
  if (loaded.length > 0 && file.length === 0) {
    neuCaption("Ready", 22, 106, NEU_RUN);
    neuCaption("Press green", 218, 106, NEU_TEXT3, "R");
    neuText(neuFit(loaded.split("/").pop(), "B9", 196), 22, 128, "B9", NEU_TEXT);
    neuSmall(neuFitSmall(loaded, 32), 22, 134, NEU_TEXT3);
  } else if (file.length > 0) {
    neuCaption("Job", 22, 106);
    neuText(neuFit(file.split("/").pop(), "B9", 150), 22, 128, "B9", NEU_TEXT);
    neuText(pendantMachine.jobPercent + "%", 218, 128, "B9", NEU_ACCENT, "R");
    neuProgressBar(22, 136, 196, pendantMachine.jobPercent / 100);
  } else {
    neuCaption("Job", 22, 106);
    neuText("No job running", 22, 131, "S9", NEU_TEXT3);
  }
}
// Trim a path from the LEFT so the file name survives.
function neuFitSmall(s, maxChars) { return s.length > maxChars ? "..." + s.substring(s.length - (maxChars - 3)) : s; }

function updateStatusAxisPositions() {
  if (currentPendantScreen !== PSCREEN_STATUS) return;
  const key = neuDroKey();
  if (_statusKeys.dro === key) return;
  _statusKeys.dro = key;
  neuDroLabels(10, 156);
  neuDroValues(10, 156);
}

function updateStatusFeedSpindle() {
  if (currentPendantScreen !== PSCREEN_STATUS) return;
  const m = pendantMachine;
  const key = m.feedRate + "|" + m.spindleRPM + "|" + m.spindleDir + "|" + m.inInches;
  if (_statusKeys.fs === key) return;
  _statusKeys.fs = key;
  neuFieldValue(10, 232, 106, 40, "Feed", String(Math.round(m.feedRate)), m.inInches ? "in/m" : "mm/m");
  neuFieldValue(124, 232, 106, 40, "Spindle " + (m.spindleRPM > 0 ? m.spindleDir : ""), String(Math.round(m.spindleRPM)), "rpm");
}

function handleStatusTouch(x, y) {
  const n = neuNavHit(STATUS_NAV, x, y);
  if (n === "home") currentPendantScreen = PSCREEN_MAIN_MENU;
  else if (n === "fluidnc") currentPendantScreen = PSCREEN_FLUIDNC;
}
