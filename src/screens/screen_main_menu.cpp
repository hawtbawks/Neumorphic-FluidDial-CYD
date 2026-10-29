/* screen_main_menu.cpp — neumorphic layout (spec: simulator/js/screens/menu.js)
 *   hero card  y 38 h 68   DRO (work + machine), or the alarm / link / sync state when that matters more
 *   tiles      2 x 4 grid, 105 x 44 at a 49 px pitch, raised, icon above a one-word label
 * No nav bar: this is Home.
 */
#include "pendant_shared.h"
#include "neu_ui.h"
#include "screen_main_menu.h"
#include "../Comms.h"
#ifdef USE_ESPNOW
#include "../PeerLink.h"
#endif

struct MenuTile { const char* label; NeuIcon icon; PendantScreen dest; };
static const MenuTile kTiles[8] = {
    { "Jog",       ICON_JOG,     PSCREEN_JOG_HOMING },      { "Work",    ICON_ORIGIN,  PSCREEN_PROBING_WORK },
    { "Overrides", ICON_GAUGE,   PSCREEN_FEEDS_SPEEDS },    { "Spindle", ICON_SPINDLE, PSCREEN_SPINDLE_CONTROL },
    { "Macros",    ICON_MACRO,   PSCREEN_MACROS },          { "Files",   ICON_CARD,    PSCREEN_SD_CARD },
    { "Probe",     ICON_PROBE,   PSCREEN_PROBE },           { "Status",  ICON_STATUS,  PSCREEN_STATUS },
};
static NeuRect menuTileRect(int i) { return { 10 + (i % 2) * 115, 114 + (i >> 1) * 49, 105, 44 }; }

// Change key for the hero card: which mode it is in (with whatever names that
// mode on screen), plus the positions while it shows the DRO.
static char       s_modeKey[48];
static NeuDroSnap s_dro;

void enterMainMenu() {
    releasePanelSprites();
    s_modeKey[0] = 0;
}

void exitMainMenu() {
    releasePanelSprites();
}

void drawMainMenu() {
    neuHeader("FluidDial");
    s_modeKey[0] = 0;
    neuInset(10, 38, 220, NEU_DRO_H, 12, 3);
    updateMainMenuDisplay();
    for (int i = 0; i < 8; i++) {
        NeuRect r = menuTileRect(i);
        neuRaised(r.x, r.y, r.w, r.h, 12, 3);
        neuIcon(kTiles[i].icon, r.x + r.w / 2, r.y + 15, NEU_ACCENT);
        neuText(kTiles[i].label, r.x + r.w / 2, r.y + 39, NEU_B9, NEU_TEXT, NEU_CENTER);
    }
}

// Wireless link problems are reported in the hero card, captioned ESP-NOW, so a
// dead radio link never looks like a machine alarm.  UART has no link to report.
struct LinkFault { const char* big; uint16_t col; const char* sub; };

static bool espNowLinkFault(LinkFault& out) {
#ifdef USE_ESPNOW
    static char subBuf[40];
    if (comms_active_mode() != COMMS_MODE_ESPNOW) return false;
    if (!espnow_radio_ready()) {
        out = { "Radio down", NEU_ALARM, "ESP-NOW failed to start" };
        return true;
    }
    if (!espnow_has_saved_pairing()) {
        out = { "Not paired", NEU_ALARM, "Connection > Pair" };
        return true;
    }
    if (!espnow_is_connected()) {
        ESPNowProfileInfo info;
        const int idx = espnow_active_profile_index();
        const char* name = (idx >= 0 && espnow_get_profile((size_t)idx, info) && info.hostname[0])
                           ? info.hostname : "machine";
        snprintf(subBuf, sizeof(subBuf), "Reconnecting to %s", name);
        out = { "No link", NEU_HOLD, subBuf };
        return true;
    }
#else
    (void)out;
#endif
    return false;
}

void updateMainMenuDisplay() {
    if (currentPendantScreen != PSCREEN_MAIN_MENU) return;

    char st[24];
    neuStatus(st, sizeof(st));
    LinkFault  fault;
    const bool hasFault = espNowLinkFault(fault);
    const bool syncing  = !pendantConnected || !pendantSynced || strcmp(st, "N/C") == 0 || !st[0];

    char mode[48];
    if (hasFault)                         snprintf(mode, sizeof(mode), "fault:%s", fault.big);
    else if (syncing)                     snprintf(mode, sizeof(mode), "sync:%d", (int)pendantConnected);
    else if (strncmp(st, "Alarm", 5) == 0) snprintf(mode, sizeof(mode), "alarm:%s", st);
    else                                  strlcpy(mode, "dro", sizeof(mode));
    const bool dro         = strcmp(mode, "dro") == 0;
    const bool modeChanged = strcmp(mode, s_modeKey) != 0;

    if (dro) {
        NeuDroSnap snap;
        if (!neuDroSnapshot(snap)) return;   // lock busy: try again next tick
        if (!modeChanged && neuDroSame(snap, s_dro)) return;
        // The captions (work offset, Mach column) follow these, not the numbers.
        const bool labelsChanged = modeChanged || snap.numAxes != s_dro.numAxes
                                   || strcmp(snap.wcs, s_dro.wcs) != 0;
        s_dro = snap;
        strlcpy(s_modeKey, mode, sizeof(s_modeKey));
        neuPanelBegin(13, 41, 214, 62);
        if (labelsChanged) {
            neuFillRect(13, 41, 214, 62, NEU_WELL);
            neuDroLabels(10, 38, snap);
        }
        neuDroValues(10, 38, snap);
        neuPanelEnd();
        return;
    }
    if (!modeChanged) return;
    strlcpy(s_modeKey, mode, sizeof(s_modeKey));

    neuPanelBegin(13, 41, 214, 62);
    neuFillRect(13, 41, 214, 62, NEU_WELL);
    if (hasFault) {
        neuCaption("ESP-NOW", 22, 48);
        neuText(fault.big, 22, 78, NEU_B12, fault.col);
        neuSmall(fault.sub, 22, 88, NEU_TEXT2);
    } else if (syncing) {
        neuCaption("Controller", 22, 48);
        neuText(pendantConnected ? "Syncing..." : "Connecting...", 22, 78, NEU_B12,
                pendantConnected ? NEU_HOLD : NEU_TEXT2);
        neuSmall(pendantConnected ? "Reading machine settings" : "Waiting for FluidNC", 22, 88, NEU_TEXT3);
    } else {
        // "Alarm:1" -> caption "ALARM 1", headline the description
        char cap[24];
        strlcpy(cap, st, sizeof(cap));
        char* colon = strchr(cap, ':');
        if (colon) *colon = ' ';
        String desc = alarmDescription(String(st));
        desc.replace(" triggered", "");
        neuCaption(cap, 22, 48, NEU_ALARM);
        neuText(desc.c_str(), 22, 78, NEU_B12, NEU_ALARM);
        neuSmall("Home or unlock to clear", 22, 88, NEU_TEXT2);
    }
    neuPanelEnd();
}

void handleMainMenuTouch(int x, int y) {
    for (int i = 0; i < 8; i++) {
        NeuRect r = menuTileRect(i);
        if (x >= r.x - 4 && x <= r.x + r.w + 4 && y >= r.y - 4 && y <= r.y + r.h + 4) {
            currentPendantScreen = kTiles[i].dest;
            return;
        }
    }
}
