/* screen_sd_card.cpp (Files) — neumorphic layout (spec: simulator/js/screens/sdcard.js)
 *   crumb strip  current folder + page readout (tap to advance a page; the dial pages too)
 *   list well    5 rows; folders open on tap, a file is selected (raised pill) on tap
 *   nav          Home | Up | Refresh, or with a file selected: Back | Load | Run
 */
#include "pendant_shared.h"
#include "neu_ui.h"
#include "screen_sd_card.h"
#include "../FileParser.h"

enum { NAV_ID_UP = NAV_ID_FIRST, NAV_ID_REFRESH, NAV_ID_CANCEL, NAV_ID_LOAD, NAV_ID_RUN };

// Absolute path of a listed entry.  "$SD/Run=" needs the full path once files can
// live in subfolders — the same form the old FileSelectScene/FilePreviewScene pair
// has always sent (e.g. $SD/Run=/sd/jobs/x.nc).
static String sdFullPath(int i) {
    String base = pendantSdCard.curPath;
    if (base == "/") base = "";
    return base + "/" + pendantSdCard.files[i];
}

// Change key: everything the crumb and the list show.  valid = false forces a paint.
struct SdKey {
    char     path[SD_PATH_MAX];
    bool     loading, loadFailed, pendingRun, connected;
    int      fileCount, totalFiles, scrollOffset, selectedFile;
    uint32_t listGen;   // a new listing with the same count still repaints
};
static SdKey s_key;
static bool  s_keyValid = false;

static void sdRequestDir(const char* path);
static int  sdNav(NeuNavItem* items);

// Selecting or deselecting a file changes only the list and the nav bar.
static void sdRepaintSelection() {
    updateSDCardFileList();
    NeuNavItem nav[3];
    neuNavReplace(nav, sdNav(nav));
}

static bool sdIsUp(int idx) { return pendantSdCard.isDir[idx] && strcmp(pendantSdCard.files[idx], "..") == 0; }

static int sdNav(NeuNavItem* items) {
    if (pendantSdCard.pendingRun) {
        items[0] = { NAV_ID_CANCEL, "", ICON_BACK, 40 };
        items[1] = { NAV_ID_LOAD, "Load" };
        items[2] = { NAV_ID_RUN, "Run", ICON_PLAY, 0, false, false, false, NEU_RUN };
    } else {
        items[0] = NAV_HOME;
        items[1] = { NAV_ID_UP, "", ICON_UP, 40, false, false, pendantSdCard.dirLevel <= 0 };
        items[2] = { NAV_ID_REFRESH, "Refresh", ICON_REFRESH };
    }
    return 3;
}

// Ask the controller for a directory listing and park the screen in its loading
// state.  The generation counter tags the request so a reply that lands after we
// have navigated elsewhere is dropped rather than filling the wrong folder.
static void sdRequestDir(const char* path) {
    strlcpy(pendantSdCard.curPath, path, SD_PATH_MAX);
    int level = -1;
    for (const char* p = pendantSdCard.curPath; *p; p++) if (*p == '/') level++;
    pendantSdCard.dirLevel     = level < 0 ? 0 : level;
    pendantSdCard.listGen++;
    pendantSdCard.scrollOffset = 0;
    pendantSdCard.selectedFile = 0;
    pendantSdCard.pendingRun   = false;
    pendantSdCard.fileCount    = 0;
    pendantSdCard.loading      = true;
    pendantSdCard.loadFailed   = false;
    pendantSdCard.loadStartMs  = millis();
    if (pendantConnected) request_file_list(pendantSdCard.curPath);
    drawSDCardScreen();          // the nav bar changes with pendingRun / dirLevel
    // The list under the finger has just been replaced — don't let a held tap
    // re-fire onto the new rows (notably ".." at row 0 of the folder we opened).
    suppressTouchBounce();
}

static void sdDescend(int i) { sdRequestDir(sdFullPath(i).c_str()); }

static void sdAscend() {
    if (pendantSdCard.dirLevel <= 0) return;
    char up[SD_PATH_MAX];
    strlcpy(up, pendantSdCard.curPath, sizeof(up));
    char* slash = strrchr(up, '/');
    if (!slash || slash == up) strlcpy(up, "/sd", sizeof(up));
    else                       *slash = '\0';
    sdRequestDir(up);
}

// Dial — one detent pages the list by one screen, clamped at both ends.  Does NOT
// move the selection: Load/Run stay armed on the file you picked, so you can page
// away and still load it.
void handleSDCardDial(int delta) {
    int next = listPageStep(delta, pendantSdCard.fileCount, pendantSdCard.scrollOffset);
    if (next < 0) return;              // already at the first/last page
    pendantSdCard.scrollOffset = next;
    updateSDCardFileList();
}

void enterSDCard() {
    releasePanelSprites();

    pendantSdCard.pendingRun = false;
    s_keyValid = false;

    g_expecting_json = false;  // clear any stuck request state from a prior screen

    // Always re-enter at the card root rather than wherever we were last time.
    strlcpy(pendantSdCard.curPath, "/sd", SD_PATH_MAX);
    pendantSdCard.dirLevel     = 0;
    pendantSdCard.totalFiles   = 0;
    if (pendantConnected) {
        pendantSdCard.listGen++;
        pendantSdCard.loading      = true;
        pendantSdCard.loadFailed   = false;
        pendantSdCard.loadStartMs  = millis();   // arm the UI loading deadline
        pendantSdCard.fileCount    = 0;
        pendantSdCard.scrollOffset = 0;
        pendantSdCard.selectedFile = 0;
        request_file_list("/sd");
    }
}

void exitSDCard() {
    releasePanelSprites();
}

static const char* s_msg1;
static const char* s_msg2;
static uint16_t    s_msgColor;

static void drawMessageBand(int) { neuListMessage(s_msg1, s_msg2, s_msgColor); }

static void drawFileBand(int i) {
    const int idx = i + pendantSdCard.scrollOffset;
    if (idx >= pendantSdCard.fileCount) return;
    const bool dir = pendantSdCard.isDir[idx], up = sdIsUp(idx);
    neuListRow(i, up ? "Up a folder" : pendantSdCard.files[idx],
               up ? NEU_ROW_UP : dir ? NEU_ROW_DIR : NEU_ROW_FILE,
               !dir && pendantSdCard.pendingRun && idx == pendantSdCard.selectedFile);
}

// Repaints the crumb + list, and only when something they show has changed.
void updateSDCardFileList() {
    if (currentPendantScreen != PSCREEN_SD_CARD) return;

    // UI safety deadline.  The list request is fire-once (no auto-retry), so if
    // its reply is ever dropped, "Loading…" would otherwise sit forever.  After
    // 10 s, fail it cleanly so the screen invites a manual Refresh instead.
    if (pendantSdCard.loading && pendantSdCard.loadStartMs != 0 &&
        (millis() - pendantSdCard.loadStartMs) > 10000) {
        pendantSdCard.loading    = false;
        pendantSdCard.loadFailed = true;
    }

    SdKey k;
    memset(&k, 0, sizeof(k));
    strlcpy(k.path, pendantSdCard.curPath, sizeof(k.path));
    k.loading      = pendantSdCard.loading;
    k.loadFailed   = pendantSdCard.loadFailed;
    k.pendingRun   = pendantSdCard.pendingRun;
    k.connected    = pendantConnected;
    k.fileCount    = pendantSdCard.fileCount;
    k.totalFiles   = pendantSdCard.totalFiles;
    k.scrollOffset = pendantSdCard.scrollOffset;
    k.selectedFile = pendantSdCard.selectedFile;
    k.listGen      = pendantSdCard.listGen;
    if (s_keyValid && memcmp(&k, &s_key, sizeof(k)) == 0) return;
    s_key      = k;
    s_keyValid = true;

    const int pages = k.fileCount == 0 ? 0 : listPageCount(k.fileCount);
    neuPanelBegin(13, NEU_LIST_CRUMB_Y + 3, 215, 21);
    neuListCrumb(ICON_FOLDER, k.path[0] ? k.path : "/sd", listPageIndex(k.scrollOffset) + 1, pages,
                 k.totalFiles > k.fileCount);
    neuPanelEnd();

    s_msg2     = "Tap Refresh to try again";
    s_msgColor = NEU_TEXT2;
    if (k.loading) {
        s_msg1 = "Loading...";
        s_msg2 = nullptr;
    } else if (k.loadFailed) {
        s_msg1     = "Couldn't load file list";
        s_msgColor = NEU_HOLD;
    } else if (k.fileCount == 0) {
        s_msg1 = pendantConnected ? "No G-code files" : "Not connected";
    } else {
        neuListPaint(drawFileBand);
        return;
    }
    neuListPaint(drawMessageBand);
}

void drawSDCardScreen() {
    neuHeader("Files");
    neuListFrame();
    s_keyValid = false;
    updateSDCardFileList();
    NeuNavItem nav[3];
    neuNav(nav, sdNav(nav));
}

void handleSDCardTouch(int x, int y) {
    const int row = neuListRowHit(x, y);
    if (row >= 0) {
        const int idx = row + pendantSdCard.scrollOffset;
        if (idx >= pendantSdCard.fileCount) return;
        if (pendantSdCard.isDir[idx]) {
            // ".." walks back up; any other folder is descended into.
            if (sdIsUp(idx)) sdAscend();
            else             sdDescend(idx);
        } else if (pendantSdCard.pendingRun && pendantSdCard.selectedFile == idx) {
            pendantSdCard.pendingRun = false;   // tap again to deselect
            sdRepaintSelection();
        } else {
            pendantSdCard.selectedFile = idx;
            pendantSdCard.pendingRun   = true;
            sdRepaintSelection();
        }
        return;
    }

    // The page readout in the crumb strip advances one page, wrapping to the first.
    if (neuListPageHit(x, y)) {
        int next = listPageAdvanceWrap(pendantSdCard.fileCount, pendantSdCard.scrollOffset);
        if (next >= 0) { pendantSdCard.scrollOffset = next; updateSDCardFileList(); }
        return;
    }

    NeuNavItem nav[3];
    switch (neuNavHit(nav, sdNav(nav), x, y)) {
    case NAV_ID_HOME:
        currentPendantScreen = PSCREEN_MAIN_MENU;
        break;
    case NAV_ID_UP:
        sdAscend();
        break;
    case NAV_ID_REFRESH:
        // Re-lists the CURRENT folder, not always the root.
        if (pendantConnected) sdRequestDir(pendantSdCard.curPath[0] ? pendantSdCard.curPath : "/sd");
        break;
    case NAV_ID_CANCEL:
        pendantSdCard.pendingRun = false;
        sdRepaintSelection();
        break;
    case NAV_ID_LOAD:
        // Store the path and go to Status; the green button runs it.
        pendantSdCard.loadedFile = sdFullPath(pendantSdCard.selectedFile);
        pendantSdCard.pendingRun = false;
        currentPendantScreen = PSCREEN_STATUS;
        break;
    case NAV_ID_RUN:
        if (pendantConnected) {
            String cmd = "$SD/Run=" + sdFullPath(pendantSdCard.selectedFile);
            send_line(cmd.c_str());
            pendantSdCard.loadedFile = "";
            pendantSdCard.pendingRun = false;
            currentPendantScreen = PSCREEN_STATUS;
        }
        break;
    }
}
