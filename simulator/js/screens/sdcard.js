/* screen_sd_card.cpp (Files) — neumorphic layout
 *   crumb strip  current folder + page readout (tap to advance a page; the dial pages too)
 *   list well    5 rows; folders open on tap, a file is selected (raised pill) on tap
 *   nav          Home | Up | Refresh, or with a file selected: Back | Load | Run
 */

function enterSDCard() {
  releasePanelSprites();
  pendantSdCard.pendingRun = false;
  g_expecting_json = false;
  if (pendantConnected) {
    pendantSdCard.loading = true;
    pendantSdCard.loadFailed = false;
    pendantSdCard.loadStartMs = millis();
    pendantSdCard.fileCount = 0;
    pendantSdCard.totalFiles = 0;
    sdRequestDir("/sd");          // always re-enter at the card root
  }
}
function exitSDCard() {}

// Repaints the crumb + list only when something they show has changed.
let _sdKey = "";
function updateSDCardFileList(force) {
  if (currentPendantScreen !== PSCREEN_SD_CARD) return;
  if (pendantSdCard.loading && pendantSdCard.loadStartMs !== 0 && millis() - pendantSdCard.loadStartMs > 10000) {
    pendantSdCard.loading = false; pendantSdCard.loadFailed = true;
  }
  const sd = pendantSdCard;
  const key = [sd.curPath, sd.loading, sd.loadFailed, sd.fileCount, sd.totalFiles, sd.scrollOffset, sd.selectedFile, sd.pendingRun, pendantConnected].join("|");
  if (!force && key === _sdKey) return;
  _sdKey = key;
  const pages = sd.fileCount === 0 ? 0 : listPageCount(sd.fileCount);
  neuListCrumb("folder", sd.curPath || "/sd", listPageIndex(sd.scrollOffset) + 1, pages, sd.totalFiles > sd.fileCount);
  neuListClear();
  if (sd.loading) neuListMessage("Loading...", null, NEU_TEXT2);
  else if (sd.loadFailed) neuListMessage("Couldn't load file list", "Tap Refresh to try again", NEU_HOLD);
  else if (sd.fileCount === 0) neuListMessage(pendantConnected ? "No G-code files" : "Not connected", "Tap Refresh to try again");
  else {
    for (let i = 0; i < LIST_ROWS; i++) {
      const idx = i + sd.scrollOffset;
      if (idx >= sd.fileCount) break;
      const dir = !!sd.isDir[idx], up = dir && sd.files[idx] === "..";
      neuListRow(i, up ? "Up a folder" : sd.files[idx], up ? "up" : dir ? "dir" : "file", !dir && sd.pendingRun && idx === sd.selectedFile);
    }
  }
}
function drawSDFooter() {}   // the page readout now lives in the crumb strip

// Row label — folders get a trailing slash, ".." is spelled out, and long names
// are truncated so they cannot overflow the row (230 px at 6 px/char).
const SD_LABEL_MAX = 35;
function sdRowLabel(i) {
  const name = pendantSdCard.files[i];
  if (pendantSdCard.isDir[i] && name === "..") return ".. (up a folder)";
  const label = name + (pendantSdCard.isDir[i] ? "/" : "");
  return label.length > SD_LABEL_MAX ? label.substring(0, SD_LABEL_MAX - 3) + "..." : label;
}

// Absolute path of a listed entry — "$SD/Run=" needs the full path once files
// can live in subfolders, which is what the old UI's FilePreviewScene sends.
function sdFullPath(i) {
  const base = pendantSdCard.curPath === "/" ? "" : pendantSdCard.curPath;
  return base + "/" + pendantSdCard.files[i];
}

// Ask for a directory listing and park the screen in its loading state.
// A generation counter tags the request so a reply that arrives after we have
// navigated somewhere else can be dropped rather than filling the wrong folder.
function sdRequestDir(path) {
  pendantSdCard.curPath     = path;
  pendantSdCard.dirLevel    = path.split("/").filter(Boolean).length - 1;
  pendantSdCard.listGen     = (pendantSdCard.listGen || 0) + 1;
  pendantSdCard.scrollOffset = 0;
  pendantSdCard.selectedFile = 0;
  pendantSdCard.pendingRun   = false;
  pendantSdCard.loading      = true;
  pendantSdCard.loadFailed   = false;
  pendantSdCard.loadStartMs  = millis();
  drawSDCardScreen();          // bottom row changes with pendingRun
  request_file_list(path);
}

function sdDescend(i) { sdRequestDir(sdFullPath(i)); }

function sdAscend() {
  if (pendantSdCard.dirLevel <= 0) return;
  const p = pendantSdCard.curPath;
  sdRequestDir(p.substring(0, p.lastIndexOf("/")) || "/sd");
}

function sdRepaintPaged() { updateSDCardFileList(); }

function handleSDCardDial(delta) {
  const next = listPageStep(delta, pendantSdCard.fileCount, pendantSdCard.scrollOffset);
  if (next < 0) return;              // already at the first/last page
  pendantSdCard.scrollOffset = next;
  sdRepaintPaged();
}

function sdNav() {
  if (pendantSdCard.pendingRun)
    return [{ id: "cancel", icon: "back", w: 40 }, { id: "load", label: "Load" }, { id: "run", label: "Run", icon: "play", color: NEU_RUN }];
  return [NAV_HOME, { id: "up", icon: "up", w: 40, dim: pendantSdCard.dirLevel <= 0 }, { id: "refresh", label: "Refresh", icon: "refresh" }];
}

function drawSDCardScreen() {
  neuHeader("Files");
  neuListFrame();
  updateSDCardFileList(true);
  neuNav(sdNav());
}

function handleSDCardTouch(x, y) {
  const sd = pendantSdCard;
  const row = neuListRowHit(x, y);
  if (row >= 0) {
    const idx = row + sd.scrollOffset;
    if (idx >= sd.fileCount) return;
    if (sd.isDir[idx]) { if (sd.files[idx] === "..") sdAscend(); else sdDescend(idx); }
    else if (sd.pendingRun && sd.selectedFile === idx) { sd.pendingRun = false; drawSDCardScreen(); }   // tap again to deselect
    else { sd.selectedFile = idx; sd.pendingRun = true; drawSDCardScreen(); }
    return;
  }
  if (neuListPageHit(x, y)) {
    const next = listPageAdvanceWrap(sd.fileCount, sd.scrollOffset);
    if (next >= 0) { sd.scrollOffset = next; sdRepaintPaged(); }
    return;
  }
  const n = neuNavHit(sdNav(), x, y);
  if (n === "home") currentPendantScreen = PSCREEN_MAIN_MENU;
  else if (n === "up") sdAscend();
  else if (n === "refresh") { if (pendantConnected) sdRequestDir(sd.curPath || "/sd"); }
  else if (n === "cancel") { sd.pendingRun = false; drawSDCardScreen(); }
  else if (n === "load") {
    sd.loadedFile = sdFullPath(sd.selectedFile);
    sd.pendingRun = false;
    currentPendantScreen = PSCREEN_STATUS;
  } else if (n === "run") {
    if (pendantConnected) {
      send_line("$SD/Run=" + sdFullPath(sd.selectedFile));
      sd.loadedFile = ""; sd.pendingRun = false;
      currentPendantScreen = PSCREEN_STATUS;
    }
  }
}
