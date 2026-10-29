/* screen_fluidnc.cpp — neumorphic layout (spec: simulator/js/screens/fluidnc.js)
 *   info well   y 38 h 162   six key/value rows: versions, network, free heap, link status
 *   Connection  raised row -> Connection screen
 *   Display     raised row: tap (or turn the dial) to flip the screen rotation
 *   nav         Home | Status
 */
#include "pendant_shared.h"
#include "neu_ui.h"
#include "screen_fluidnc.h"
#include "../Comms.h"          // comms_active_mode()
#include "../FileParser.h"     // request_eth_status()
#include <Preferences.h>

extern Preferences preferences;

enum { NAV_ID_STATUS = NAV_ID_FIRST };
static const NeuNavItem kNav[] = { NAV_HOME, { NAV_ID_STATUS, "Status", ICON_STATUS } };
static const int        kNavN  = 2;

static const int FNC_CONN_Y = 208, FNC_ROT_Y = 244, FNC_ROW_H = 30;
static const int FNC_ROWS = 6, FNC_ROW0_Y = 44, FNC_PITCH = 26;

// What each info row shows; a row repaints only when its own value changes.
struct FncRow { char value[40]; uint16_t color; };
static FncRow s_rows[FNC_ROWS];
static bool   s_rowsValid = false;
static char   s_rotation[16];

void enterFluidNC() {
    releasePanelSprites();
    s_rowsValid = false;

    // Network details are fetched lazily, on entry to the only screen that shows
    // them — never as part of the connect-edge burst, which lands while a wired
    // controller is still booting.
    //
    // requestNetworkInfo() is a no-op unless $I reported no network at all (a
    // wired controller on FluidNC <= v4.1.1); on anything else the Mode= line
    // already carried everything.  The wired IP and link state then follow from
    // "$Ethernet/Status", re-asked on each entry so an unplugged cable is noticed
    // rather than read as still up.
    if (pendantConnected) {
        requestNetworkInfo();
        if (pendantMachine.netMode == "Ethernet") request_eth_status();
    }
}

void exitFluidNC() {
    releasePanelSprites();

    // Persist rotation to NVS only on screen exit. Writing on every encoder
    // detent during a fast spin would otherwise hammer flash; coalescing into
    // a single write per visit eliminates that risk.
    if (pendantMachine.rotationDirty) {
        preferences.begin("pendant", false);
        preferences.putInt("rotation", pendantMachine.rotation);
        preferences.end();
        pendantMachine.rotationDirty = false;
    }
}

static void fncRow(int y, const char* label, const char* value, NeuIcon icon) {
    neuClear(10, y, 220, FNC_ROW_H);
    neuRaised(10, y, 220, FNC_ROW_H, 10, 3);
    neuIcon(icon, 30, y + FNC_ROW_H / 2, NEU_ACCENT);
    neuText(label, 46, neuCapBase(y, FNC_ROW_H, NEU_B9), NEU_B9, NEU_TEXT);
    neuCaption(value, 202, y + 11, NEU_TEXT2, NEU_RIGHT);
    neuIcon(ICON_CHEVRON, 214, y + FNC_ROW_H / 2, NEU_TEXT3);
}

void updateFluidNCDisplay() {
    if (currentPendantScreen != PSCREEN_FLUIDNC) return;

    String fluidDialVer, fluidNCVer, connStatus, dispRotation, ip, ssid, netMode, netStatus;
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(5)) != pdTRUE) return;   // try again next tick
    fluidDialVer = pendantMachine.fluidDialVersion;
    fluidNCVer   = pendantMachine.fluidNCVersion;
    connStatus   = pendantMachine.connectionStatus;
    dispRotation = pendantMachine.displayRotation;
    ip           = pendantMachine.ipAddress;
    ssid         = pendantMachine.wifiSSID;
    netMode      = pendantMachine.netMode;
    netStatus    = pendantMachine.netStatus;
    xSemaphoreGive(stateMutex);

    // "---" until the controller has actually reported a value, so an unread
    // field can never be mistaken for a real one.  A wired controller never
    // reports an SSID, so on Ethernet that row names the link instead.
    const bool hasIp   = ip.length() && ip != "---";
    const bool hasSsid = ssid.length() && ssid != "---";
    const bool isEth   = netMode == "Ethernet";
    const bool ethDown = isEth && netStatus.startsWith("Link down");
    char heap[16];
    snprintf(heap, sizeof(heap), "%u KB", (unsigned)(ESP.getFreeHeap() / 1024));

    const char* labels[FNC_ROWS] = { "FluidDial", "FluidNC", "IP address",
                                     isEth ? "Network" : "WiFi SSID", "Free heap", "Link" };
    FncRow rows[FNC_ROWS];
    memset(rows, 0, sizeof(rows));
    strlcpy(rows[0].value, fluidDialVer.c_str(), sizeof(rows[0].value));
    rows[0].color = NEU_TEXT;
    strlcpy(rows[1].value, fluidNCVer.length() ? fluidNCVer.c_str() : "---", sizeof(rows[1].value));
    rows[1].color = fluidNCVer.length() ? NEU_TEXT : NEU_TEXT3;
    strlcpy(rows[2].value, hasIp ? ip.c_str() : "---", sizeof(rows[2].value));
    rows[2].color = hasIp ? NEU_TEXT : NEU_TEXT3;
    strlcpy(rows[3].value, isEth ? (ethDown ? "Ethernet, no link" : "Wired Ethernet") : (hasSsid ? ssid.c_str() : "---"),
            sizeof(rows[3].value));
    rows[3].color = ethDown ? NEU_HOLD : (isEth || hasSsid) ? NEU_TEXT : NEU_TEXT3;
    strlcpy(rows[4].value, heap, sizeof(rows[4].value));
    rows[4].color = NEU_TEXT;
    strlcpy(rows[5].value, connStatus.c_str(), sizeof(rows[5].value));
    rows[5].color = pendantConnected ? NEU_RUN : NEU_ALARM;

    // The well is too tall for the panel scratch; each row is its own band
    // (the separator above it included), so a row repaints without flicker.
    for (int i = 0; i < FNC_ROWS; i++) {
        if (s_rowsValid && memcmp(&rows[i], &s_rows[i], sizeof(FncRow)) == 0) continue;
        s_rows[i] = rows[i];
        const int y = FNC_ROW0_Y + i * FNC_PITCH, by = y - 2;
        const int bh = i == FNC_ROWS - 1 ? 42 + 155 - by : FNC_PITCH;
        neuPanelBegin(14, by, 213, bh);
        neuFillRect(14, by, 213, bh, NEU_WELL);
        if (i) neuFillRect(22, y - 2, 196, 1, NEU_SH_DARK);
        neuCaption(labels[i], 22, y + 8);
        neuText(neuFit(rows[i].value, NEU_B9, 120), 218, y + 17, NEU_B9, rows[i].color, NEU_RIGHT);
        neuPanelEnd();
    }
    s_rowsValid = true;

    if (strcmp(s_rotation, dispRotation.c_str()) != 0) {
        strlcpy(s_rotation, dispRotation.c_str(), sizeof(s_rotation));
        fncRow(FNC_ROT_Y, "Display", s_rotation, ICON_GEAR);
    }
}

void drawFluidNCScreen() {
    neuHeader("FluidNC");
    s_rowsValid  = false;
    s_rotation[0] = 0;
    neuInset(10, 38, 220, 162, 12, 3);
    fncRow(FNC_CONN_Y, "Connection", comms_active_mode() == COMMS_MODE_UART ? "UART" : "ESP-NOW", ICON_LINK);
    updateFluidNCDisplay();
    neuNav(kNav, kNavN);
}

void handleFluidNCTouch(int x, int y) {
    // Just assign — handlePendantTouch() observes the change and runs navigateTo() once.
    if (isTouchInBounds(x, y, 10, FNC_CONN_Y, 220, FNC_ROW_H)) {
        currentPendantScreen = PSCREEN_CONNECTION;
        return;
    }
    if (isTouchInBounds(x, y, 10, FNC_ROT_Y, 220, FNC_ROW_H)) {
        toggleDisplayRotation();   // same as a dial turn
        suppressTouchBounce();     // the whole screen just turned under the finger
        return;
    }
    switch (neuNavHit(kNav, kNavN, x, y)) {
    case NAV_ID_HOME:   currentPendantScreen = PSCREEN_MAIN_MENU; break;
    case NAV_ID_STATUS: currentPendantScreen = PSCREEN_STATUS;    break;
    }
}
