/* screen_macros.cpp — neumorphic layout
 *   crumb strip  macro source + page readout;  list well  5 macros
 *   tap a macro to select it, then Run macro in the nav bar (Back cancels)
 */

function enterMacros() {
  releasePanelSprites();
  pendantMacros.scrollOffset = 0;
  pendantMacros.selected = -1;
  pendantMacros.pendingRun = false;
  if (pendantMacros.cacheValid) {
    pendantMacros.loading = false; pendantMacros.loadFailed = false;
  } else {
    pendantMacros.loading = true; pendantMacros.loadFailed = false; pendantMacros.count = 0;
    if (pendantConnected) requestMacros();
  }
}
function exitMacros() {}

function _refreshMacros() {
  pendantMacros.cacheValid = false;
  pendantMacros.scrollOffset = 0;
  pendantMacros.selected = -1;
  pendantMacros.pendingRun = false;
  pendantMacros.loading = true;
  pendantMacros.count = 0;
  if (pendantConnected) requestMacros();
}

function macroLabel(displayIndex) {
  let label = pendantMacros.content[displayIndex];
  if (label.length > 36) label = label.substring(0, 33) + "...";
  return label;
}

let _macKey = "";
function updateMacrosFileList(force) {
  if (currentPendantScreen !== PSCREEN_MACROS) return;
  const m = pendantMacros;
  if (m.loading && m.loadStartMs !== 0 && millis() - m.loadStartMs > 35000) { m.loading = false; m.loadFailed = true; }
  const key = [m.loading, m.loadFailed, m.count, m.scrollOffset, m.selected, m.pendingRun, pendantConnected].join("|");
  if (!force && key === _macKey) return;
  _macKey = key;
  const pages = m.count === 0 ? 0 : listPageCount(m.count);
  neuListCrumb("macro", m.sourceFile || "macrocfg.json", listPageIndex(m.scrollOffset) + 1, pages, false);
  neuListClear();
  if (m.loading) neuListMessage(pendantConnected ? "Loading..." : "Not connected", null);
  else if (m.loadFailed) neuListMessage("Couldn't load macros", "Tap Refresh to try again", NEU_HOLD);
  else if (m.count === 0) neuListMessage("No macros found", "Add them in FluidNC preferences");
  else {
    for (let i = 0; i < LIST_ROWS; i++) {
      const idx = i + m.scrollOffset;
      if (idx >= m.count) break;
      neuListRow(i, m.content[idx], "macro", m.pendingRun && idx === m.selected);
    }
  }
}

// Dial pages the macro list one screen at a time, clamped.
function handleMacrosDial(delta) {
  const next = listPageStep(delta, pendantMacros.count, pendantMacros.scrollOffset);
  if (next < 0) return;
  pendantMacros.scrollOffset = next;
  updateMacrosFileList();
}
function drawMacrosFooter() {}

function macrosNav() {
  if (pendantMacros.pendingRun) return [{ id: "cancel", icon: "back", w: 40 }, { id: "run", label: "Run macro", icon: "play", color: NEU_RUN }];
  return [NAV_HOME, { id: "refresh", label: "Refresh", icon: "refresh" }];
}

function drawMacrosScreen() {
  neuHeader("Macros");
  neuListFrame();
  updateMacrosFileList(true);
  neuNav(macrosNav());
}

function handleMacrosTouch(x, y) {
  const m = pendantMacros;
  const row = neuListRowHit(x, y);
  if (row >= 0) {
    const idx = row + m.scrollOffset;
    if (idx < m.count) {
      if (m.pendingRun && m.selected === idx) m.pendingRun = false;   // tap again to deselect
      else { m.selected = idx; m.pendingRun = true; }
      drawMacrosScreen();
    }
    return;
  }
  if (neuListPageHit(x, y)) {
    const next = listPageAdvanceWrap(m.count, m.scrollOffset);
    if (next >= 0) { m.scrollOffset = next; updateMacrosFileList(); }
    return;
  }
  const n = neuNavHit(macrosNav(), x, y);
  if (n === "home") currentPendantScreen = PSCREEN_MAIN_MENU;
  else if (n === "refresh") { if (pendantConnected) { _refreshMacros(); drawMacrosScreen(); } }
  else if (n === "cancel") { m.pendingRun = false; drawMacrosScreen(); }
  else if (n === "run") {
    if (pendantConnected && m.selected >= 0) {
      const fn = m.filename[m.selected];
      let cmd;
      if (fn.startsWith("/sd/")) cmd = "$SD/Run=" + fn.substring(4);
      else if (fn.startsWith("/localfs/")) cmd = "$Localfs/Run=" + fn.substring(9);
      else if (fn.startsWith("cmd:")) cmd = fn.substring(4);
      else cmd = fn;
      send_line(cmd);
      m.pendingRun = false;
      currentPendantScreen = PSCREEN_STATUS;
    }
  }
}
