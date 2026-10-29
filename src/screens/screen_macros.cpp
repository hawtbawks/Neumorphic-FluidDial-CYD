/* screen_macros.cpp — neumorphic layout (spec: simulator/js/screens/macros.js)
 *   crumb strip  macro source + page readout;  list well  5 macros
 *   tap a macro to select it, then Run macro in the nav bar (Back cancels)
 */
#include "pendant_shared.h"
#include "neu_ui.h"
#include "screen_macros.h"

enum { NAV_ID_REFRESH = NAV_ID_FIRST, NAV_ID_CANCEL, NAV_ID_RUN };

// Change key: everything the crumb and the list show.  valid = false forces a paint.
struct MacroKey {
    bool loading, loadFailed, pendingRun, connected;
    int  count, scrollOffset, selected;
};
static MacroKey s_key;
static bool     s_keyValid = false;

static int macrosNav(NeuNavItem* items) {
    if (pendantMacros.pendingRun) {
        items[0] = { NAV_ID_CANCEL, "", ICON_BACK, 40 };
        items[1] = { NAV_ID_RUN, "Run macro", ICON_PLAY, 0, false, false, false, NEU_RUN };
    } else {
        items[0] = NAV_HOME;
        items[1] = { NAV_ID_REFRESH, "Refresh", ICON_REFRESH };
    }
    return 2;
}

// Selecting or deselecting a macro changes only the list and the nav bar.
static void macrosRepaintSelection() {
    updateMacrosFileList();
    NeuNavItem nav[2];
    neuNavReplace(nav, macrosNav(nav));
}

void enterMacros() {
    releasePanelSprites();

    pendantMacros.scrollOffset = 0;
    pendantMacros.selected     = -1;
    pendantMacros.pendingRun   = false;
    s_keyValid = false;

    if (pendantMacros.cacheValid) {
        // Cached list is still good — show it immediately without a fetch.
        pendantMacros.loading    = false;
        pendantMacros.loadFailed = false;
    } else {
        // First visit, or cache explicitly invalidated (Refresh button / disconnect).
        pendantMacros.loading    = true;
        pendantMacros.loadFailed = false;
        pendantMacros.count      = 0;
        if (pendantConnected) requestMacros();
    }
}

void exitMacros() {
    releasePanelSprites();
}

// Re-fetch the macro list from the controller (Refresh button).
static void refreshMacros() {
    pendantMacros.cacheValid   = false;  // force a fresh UART fetch
    pendantMacros.scrollOffset = 0;
    pendantMacros.selected     = -1;
    pendantMacros.pendingRun   = false;
    pendantMacros.loading      = true;
    pendantMacros.count        = 0;
    s_keyValid = false;
    if (pendantConnected) requestMacros();
}

// Dial — one detent pages the macro list by one screen, clamped at both ends.
void handleMacrosDial(int delta) {
    int next = listPageStep(delta, pendantMacros.count, pendantMacros.scrollOffset);
    if (next < 0) return;
    pendantMacros.scrollOffset = next;
    updateMacrosFileList();
}

static const char* s_msg1;
static const char* s_msg2;
static uint16_t    s_msgColor;

static void drawMessageBand(int) { neuListMessage(s_msg1, s_msg2, s_msgColor); }

static void drawMacroBand(int i) {
    const int idx = i + pendantMacros.scrollOffset;
    if (idx >= pendantMacros.count) return;
    neuListRow(i, pendantMacros.content[idx], NEU_ROW_MACRO,
               pendantMacros.pendingRun && idx == pendantMacros.selected);
}

void updateMacrosFileList() {
    if (currentPendantScreen != PSCREEN_MACROS) return;

    // UI safety deadline: the fetch always delivers a terminal onFilesList()/
    // onError() callback, but if it ever hangs this guarantees "Loading…" can't
    // spin forever.  35 s comfortably exceeds the worst-case retry budget.
    if (pendantMacros.loading && pendantMacros.loadStartMs != 0 &&
        (millis() - pendantMacros.loadStartMs) > 35000) {
        pendantMacros.loading    = false;
        pendantMacros.loadFailed = true;
    }

    MacroKey k;
    memset(&k, 0, sizeof(k));
    k.loading      = pendantMacros.loading;
    k.loadFailed   = pendantMacros.loadFailed;
    k.pendingRun   = pendantMacros.pendingRun;
    k.connected    = pendantConnected;
    k.count        = pendantMacros.count;
    k.scrollOffset = pendantMacros.scrollOffset;
    k.selected     = pendantMacros.selected;
    if (s_keyValid && memcmp(&k, &s_key, sizeof(k)) == 0) return;
    s_key      = k;
    s_keyValid = true;

    const int pages = k.count == 0 ? 0 : listPageCount(k.count);
    neuPanelBegin(13, NEU_LIST_CRUMB_Y + 3, 215, 21);
    neuListCrumb(ICON_MACRO, "macrocfg.json", listPageIndex(k.scrollOffset) + 1, pages, false);
    neuPanelEnd();

    s_msg2     = nullptr;
    s_msgColor = NEU_TEXT2;
    if (k.loading) {
        s_msg1 = pendantConnected ? "Loading..." : "Not connected";
    } else if (k.loadFailed) {
        s_msg1     = "Couldn't load macros";
        s_msg2     = "Tap Refresh to try again";
        s_msgColor = NEU_HOLD;
    } else if (k.count == 0) {
        s_msg1 = "No macros found";
        s_msg2 = "Add them in FluidNC preferences";
    } else {
        neuListPaint(drawMacroBand);
        return;
    }
    neuListPaint(drawMessageBand);
}

void drawMacrosScreen() {
    neuHeader("Macros");
    neuListFrame();
    s_keyValid = false;
    updateMacrosFileList();
    NeuNavItem nav[2];
    neuNav(nav, macrosNav(nav));
}

void handleMacrosTouch(int x, int y) {
    const int row = neuListRowHit(x, y);
    if (row >= 0) {
        const int idx = row + pendantMacros.scrollOffset;
        if (idx < pendantMacros.count) {
            if (pendantMacros.pendingRun && pendantMacros.selected == idx) {
                pendantMacros.pendingRun = false;   // tap again to deselect
            } else {
                pendantMacros.selected   = idx;
                pendantMacros.pendingRun = true;
            }
            macrosRepaintSelection();
        }
        return;
    }

    // The page readout in the crumb strip advances one page, wrapping to the first.
    if (neuListPageHit(x, y)) {
        int next = listPageAdvanceWrap(pendantMacros.count, pendantMacros.scrollOffset);
        if (next >= 0) {
            pendantMacros.scrollOffset = next;
            updateMacrosFileList();
        }
        return;
    }

    NeuNavItem nav[2];
    switch (neuNavHit(nav, macrosNav(nav), x, y)) {
    case NAV_ID_HOME:
        currentPendantScreen = PSCREEN_MAIN_MENU;
        break;
    case NAV_ID_REFRESH:
        if (pendantConnected) {
            refreshMacros();
            drawMacrosScreen();
        }
        break;
    case NAV_ID_CANCEL:
        pendantMacros.pendingRun = false;
        macrosRepaintSelection();
        break;
    case NAV_ID_RUN:
        // Dispatch on the filename prefix set by FileParser
        if (pendantConnected && pendantMacros.selected >= 0) {
            const char* fn = pendantMacros.filename[pendantMacros.selected];
            char        cmd[128];
            if (strncmp(fn, "/sd/", 4) == 0) {
                snprintf(cmd, sizeof(cmd), "$SD/Run=%s", fn + 4);        // SD file
            } else if (strncmp(fn, "/localfs/", 9) == 0) {
                snprintf(cmd, sizeof(cmd), "$Localfs/Run=%s", fn + 9);   // local flash file
            } else if (strncmp(fn, "cmd:", 4) == 0) {
                snprintf(cmd, sizeof(cmd), "%s", fn + 4);                // raw command
            } else {
                snprintf(cmd, sizeof(cmd), "%s", fn);                    // unknown — send as-is
            }
            send_line(cmd);
            pendantMacros.pendingRun = false;
            currentPendantScreen = PSCREEN_STATUS;
        }
        break;
    }
}
