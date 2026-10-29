/* screen_espnow.cpp — pairing wizard + machine list (ESPNOW_SPEC §6b/§6c), neumorphic layout
 * (spec: simulator/js/screens/espnow.js)
 *
 * Pairing:   steps well (what to do on the controller) · status well · action buttons
 *            nav Home | Back   (Back cancels a pairing in progress)
 * Machines:  up to 5 rows; the active machine is pressed in.  Tap a row to use it.
 *            nav Back | Pair | Forget (forget asks first)
 */
#include "pendant_shared.h"
#include "neu_ui.h"
#include "screen_espnow.h"
#include "../Comms.h"

#ifdef USE_ESPNOW
#include "../PeerLink.h"

// ── ESP-NOW pairing wizard (ESPNOW_SPEC.md §6b) ──────────────────────────────
//
// The pairing ACTION happens on the controller ($espnow/pair), not here — which
// is unusual enough that the screen leads with the instructions rather than
// burying them.  The pendant only advertises and waits.
//
// State is derived from PeerLink rather than tracked locally, so the screen can
// never disagree with the link.

static const int PAIR_STEP_Y = 38, PAIR_STEP_H = 100, PAIR_STAT_Y = 146, PAIR_STAT_H = 74;
static const int PAIR_ACT_Y = 230, PAIR_ACT_H = 40;

enum { NAV_ID_PBACK = NAV_ID_FIRST, NAV_ID_PAIR, NAV_ID_FORGET };
static const NeuNavItem kPairNav[] = { NAV_HOME, { NAV_ID_PBACK, "Back", ICON_BACK } };
static const int        kPairNavN  = 2;

// Pairing window is opened on entry and closed on exit so the radio is never
// left advertising after the user walks away from the screen.
//
// A window that never ends is worse than one that fails: on hardware the screen
// sat in "Waiting..." indefinitely with no automatic way out.  Give it a bound
// and surface a Retry/Cancel state when it expires.
static const unsigned long PAIR_TIMEOUT_MS = 90000;   // 90 s to go run $espnow/pair
static unsigned long pairStartedMs = 0;

enum PairView { PV_WAIT, PV_WORKING, PV_OK, PV_FAIL, PV_NORADIO };
static PairView lastPairView = PV_WAIT;
static long     lastPairSecs = -1;

static void macStr(const uint8_t* mac, int channel, char* out, size_t n) {
    int len = snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    if (channel >= 0) snprintf(out + len, n - len, " ch%d", channel);
}

void enterEspNowPair() {
    releasePanelSprites();
    pairStartedMs = millis();
    espnow_start_pairing();
}

void exitEspNowPair() {
    // Only cancel if it did not succeed — cancelling a completed pair would
    // throw away the profile we just stored.
    if (!espnow_pairing_complete()) espnow_cancel_pairing();
    releasePanelSprites();
}

static PairView pairView() {
    // If the radio never came up there is nothing to listen with, so say so
    // instead of counting down 90 s of "Waiting..." that cannot succeed.
    if (!espnow_radio_ready())      return PV_NORADIO;
    if (espnow_pairing_complete())  return PV_OK;
    if (espnow_is_reconnecting())   return PV_WORKING;
    // Only the idle wait times out — a handshake already in progress is left to
    // finish rather than being cut off part-way.
    if (pairStartedMs && (millis() - pairStartedMs) >= PAIR_TIMEOUT_MS) return PV_FAIL;
    return PV_WAIT;
}

static void drawStepsContents(PairView v) {
    neuFillRect(14, PAIR_STEP_Y + 4, 213, PAIR_STEP_H - 7, NEU_WELL);
    neuCaption("On the controller", 22, PAIR_STEP_Y + 9);
    struct { const char* text; bool done; } steps[3] = {
        { "Open the FluidNC console", v != PV_WAIT },
        { "Run  $espnow/pair",        v == PV_OK || v == PV_WORKING },
        { "Keep the pendant nearby",  v == PV_OK },
    };
    for (int i = 0; i < 3; i++) {
        const int y = PAIR_STEP_Y + 26 + i * 20;
        if (steps[i].done) {   // green disc with a tick
            neuFillCircle(28, y + 3, 6, NEU_RUN);
            neuDrawLine(25, y + 3, 27, y + 5, NEU_WELL);
            neuDrawLine(27, y + 5, 31, y + 1, NEU_WELL);
        } else {
            char num[2] = { (char)('1' + i), 0 };
            neuDrawCircle(28, y + 3, 6, NEU_TEXT3);
            neuSmall(num, 26, y, NEU_TEXT3);
        }
        neuSmall(steps[i].text, 40, y, steps[i].done ? NEU_TEXT3 : NEU_TEXT);
    }
    neuSmall("or WebUI > Settings > ESP-NOW", 40, PAIR_STEP_Y + 86, NEU_TEXT3);
}

// The steps well (93 px inside) is taller than the panel scratch: two bands.
static void drawSteps(PairView v) {
    const int top = PAIR_STEP_Y + 4, h = PAIR_STEP_H - 7, h1 = h / 2;
    neuPanelBegin(14, top, 213, h1);
    drawStepsContents(v);
    neuPanelEnd();
    neuPanelBegin(14, top + h1, 213, h - h1);
    drawStepsContents(v);
    neuPanelEnd();
}

static void drawPairStatus(PairView v) {
    const char* head;
    uint16_t    col;
    const char* sub;
    switch (v) {
        case PV_OK:      head = "Paired";      col = NEU_RUN;   sub = nullptr;                        break;
        case PV_WORKING: head = "Pairing...";  col = NEU_JOG;   sub = "Exchanging keys (P-256)";      break;
        case PV_FAIL:    head = "Timed out";   col = NEU_ALARM; sub = "No controller responded";      break;
        case PV_NORADIO: head = "Radio down";  col = NEU_ALARM; sub = "ESP-NOW failed to start";      break;
        default:         head = "Waiting...";  col = NEU_HOLD;  sub = "Listening for the controller"; break;
    }
    neuPanelBegin(14, PAIR_STAT_Y + 4, 213, PAIR_STAT_H - 7);
    neuFillRect(14, PAIR_STAT_Y + 4, 213, PAIR_STAT_H - 7, NEU_WELL);
    neuLed(26, PAIR_STAT_Y + 20, col, 4);
    neuText(head, 38, PAIR_STAT_Y + 26, NEU_B12, col);
    if (v == PV_OK) {
        ESPNowProfileInfo info;
        const int idx = espnow_active_profile_index();
        if (idx >= 0 && espnow_get_profile((size_t)idx, info)) {
            char mac[32];
            macStr(info.mac, info.channel, mac, sizeof(mac));
            neuText(neuFit(info.hostname[0] ? info.hostname : "(unnamed)", NEU_B9, 190), 22, PAIR_STAT_Y + 50, NEU_B9, NEU_TEXT);
            neuCaption(mac, 22, PAIR_STAT_Y + 56, NEU_TEXT3);
        }
    } else if (sub) {
        neuSmall(sub, 22, PAIR_STAT_Y + 40, NEU_TEXT2);
        if (v == PV_FAIL) neuSmall("Check FluidNC is v4.0.4 or newer", 22, PAIR_STAT_Y + 54, NEU_TEXT3);
        if (v == PV_WAIT) {
            const long left = ((long)PAIR_TIMEOUT_MS - (long)(millis() - pairStartedMs)) / 1000;
            char buf[24];
            snprintf(buf, sizeof(buf), "Gives up in %ld s", left < 0 ? 0 : left);
            neuSmall(buf, 22, PAIR_STAT_Y + 54, NEU_TEXT3);
        }
    }
    neuPanelEnd();
}

static void drawPairActions(PairView v) {
    neuFillRect(6, PAIR_ACT_Y - 4, 228, PAIR_ACT_H + 9, NEU_BG);
    NeuBtnStyle o;
    if (v == PV_OK) {
        o.color = NEU_RUN;
        o.icon  = ICON_PLAY;
        neuButton(10, PAIR_ACT_Y, 220, PAIR_ACT_H, "Use this machine", o);
    } else if (v == PV_FAIL) {
        o.color = NEU_ACCENT;
        o.icon  = ICON_REFRESH;
        neuButton(10, PAIR_ACT_Y, 104, PAIR_ACT_H, "Retry", o);
        NeuBtnStyle c;
        c.color = NEU_TEXT2;
        neuButton(126, PAIR_ACT_Y, 104, PAIR_ACT_H, "Cancel", c);
    } else {
        o.color = NEU_TEXT2;
        neuButton(10, PAIR_ACT_Y, 220, PAIR_ACT_H, "Cancel pairing", o);
    }
}

void drawEspNowPairScreen() {
    const PairView v = pairView();
    neuHeader("Pair Machine");
    neuInset(10, PAIR_STEP_Y, 220, PAIR_STEP_H, 12, 3);
    neuInset(10, PAIR_STAT_Y, 220, PAIR_STAT_H, 12, 3);
    drawSteps(v);
    drawPairStatus(v);
    drawPairActions(v);
    lastPairView = v;
    neuNav(kPairNav, kPairNavN);
}

void updateEspNowPairDisplay() {
    if (currentPendantScreen != PSCREEN_ESPNOW_PAIR) return;
    // Redraw the changing regions only — never fillScreen on a periodic tick.
    const PairView v    = pairView();
    const long     secs = (long)(millis() / 1000);
    if (v != lastPairView) {   // step ticks and buttons only change on transition
        drawSteps(v);
        drawPairStatus(v);
        drawPairActions(v);
        lastPairView = v;
    } else if (v == PV_WAIT && secs != lastPairSecs) {
        drawPairStatus(v);     // the countdown
    }
    lastPairSecs = secs;
}

void handleEspNowPairTouch(int x, int y) {
    const PairView v = pairView();
    if (y >= PAIR_ACT_Y - 4 && y <= PAIR_ACT_Y + PAIR_ACT_H + 4) {
        if (v == PV_OK) {                        // Use this machine
            currentPendantScreen = PSCREEN_CONNECTION;
            return;
        }
        if (v == PV_FAIL && x < 120) {           // Retry — restart the window
            pairStartedMs = millis();
            espnow_start_pairing();
            drawEspNowPairScreen();
            return;
        }
        espnow_cancel_pairing();                 // Cancel / Cancel pairing
        currentPendantScreen = PSCREEN_CONNECTION;
        return;
    }
    switch (neuNavHit(kPairNav, kPairNavN, x, y)) {
    case NAV_ID_HOME:  currentPendantScreen = PSCREEN_MAIN_MENU; break;
    case NAV_ID_PBACK: currentPendantScreen = v == PV_OK ? PSCREEN_ESPNOW_MACHINES : PSCREEN_CONNECTION; break;
    }
}

// ── Paired-machine list (ESPNOW_SPEC.md §6c) ─────────────────────────────────

static const int MCH_ROW0_Y = 38, MCH_ROW_H = 40, MCH_PITCH = 45, MCH_MAX = 5;

static int            selectedRow   = -1;    // row targeted by Forget
static bool           confirmForget = false;
static NeuDialogRects forgetDialog;

// Change key for the rows: count, which one is active, and its live signal.
struct MchKey { int count, active; bool up; int bars; };
static MchKey s_mchKey;

static void machinesKey(MchKey& k) {
    memset(&k, 0, sizeof(k));
    k.count  = (int)espnow_profile_count();
    k.active = espnow_active_profile_index();
    k.up     = espnow_is_connected();
    k.bars   = k.up ? espnow_signal_bars() : 0;
}

static int machinesNav(NeuNavItem* items) {
    const int n = (int)espnow_profile_count();
    items[0] = NAV_BACK;
    items[1] = { NAV_ID_PAIR, n >= MCH_MAX ? "Full" : "Pair", ICON_PLUS, 0, false, false, n >= MCH_MAX };
    items[2] = { NAV_ID_FORGET, "Forget", ICON_TRASH, 0, false, false, selectedRow < 0 || selectedRow >= n };
    return 3;
}

void enterEspNowMachines() {
    releasePanelSprites();
    selectedRow   = espnow_active_profile_index();
    confirmForget = false;
}

void exitEspNowMachines() {
    releasePanelSprites();
}

static int machineRowY(int i) { return MCH_ROW0_Y + i * MCH_PITCH; }

// full = redraw the surface too (active machine changed); otherwise repaint only
// the flat face, through the panel sprite.
static void drawMachineRow(int i, bool full) {
    ESPNowProfileInfo info;
    if (!espnow_get_profile((size_t)i, info)) return;
    const int  y      = machineRowY(i);
    const bool active = espnow_active_profile_index() == i;
    if (full) {
        neuClear(10, y, 220, MCH_ROW_H);
        if (active) neuInset(10, y, 220, MCH_ROW_H, 10, 3);
        else        neuRaised(10, y, 220, MCH_ROW_H, 10, 3);
    } else {
        const int fx = active ? 14 : 12, fy = y + (active ? 4 : 2), fw = active ? 211 : 216, fh = MCH_ROW_H - 6;
        neuPanelBegin(fx, fy, fw, fh);
        neuFillRect(fx, fy, fw, fh, active ? NEU_WELL : NEU_FACE);
    }
    char buf[32];
    neuText(neuFit(info.hostname[0] ? info.hostname : "(unnamed)", NEU_B9, 140), 22, y + 19, NEU_B9,
            active ? NEU_ACCENT : NEU_TEXT);
    macStr(info.mac, -1, buf, sizeof(buf));
    neuCaption(buf, 22, y + 26, NEU_TEXT3);
    // Signal is only meaningful for the machine we are actually linked to;
    // the others show as unknown rather than implying they are unreachable.
    const bool live = active && espnow_is_connected();
    const int  bars = live ? espnow_signal_bars() : 0;
    for (int b = 0; b < 4; b++) {
        const int h = 4 + b * 3;
        neuFillRoundRect(196 + b * 6, y + 19 - h, 4, h, 1, b < bars ? NEU_RUN : NEU_SH_LIGHT);
    }
    if (active) {
        neuCaption(live ? "Active" : "Offline", 218, y + 26, live ? NEU_RUN : NEU_HOLD, NEU_RIGHT);
    } else {
        snprintf(buf, sizeof(buf), "ch%u", (unsigned)info.channel);
        neuCaption(buf, 218, y + 26, NEU_TEXT3, NEU_RIGHT);
    }
    if (!full) neuPanelEnd();
}

void drawEspNowMachinesScreen() {
    neuHeader("Machines");
    const int n = (int)espnow_profile_count();
    for (int i = 0; i < n; i++) drawMachineRow(i, true);
    if (n == 0) {
        neuInset(10, MCH_ROW0_Y, 220, 60, 12, 3);
        neuText("No machines paired yet", 120, MCH_ROW0_Y + 30, NEU_B9, NEU_TEXT2, NEU_CENTER);
        neuSmall("Tap Pair to add one", 120, MCH_ROW0_Y + 40, NEU_TEXT3, NEU_CENTER);
    }
    char hint[40];
    snprintf(hint, sizeof(hint), "Tap a machine to use it  %d/%d", n, MCH_MAX);
    neuCaption(hint, 120, 266, NEU_TEXT3, NEU_CENTER);
    NeuNavItem nav[3];
    neuNav(nav, machinesNav(nav));
    machinesKey(s_mchKey);
    if (confirmForget) {
        ESPNowProfileInfo info;
        const bool  got     = espnow_get_profile((size_t)selectedRow, info);
        const char* lines[] = { got ? info.hostname : "", "You will need to pair it again" };
        forgetDialog = neuDialog("Forget machine?", lines, 2, "Cancel", "Forget", NEU_ALARM);
    }
}

void updateEspNowMachinesDisplay() {
    if (currentPendantScreen != PSCREEN_ESPNOW_MACHINES) return;
    if (confirmForget) return;            // never repaint under an open dialog
    MchKey k;
    machinesKey(k);
    if (memcmp(&k, &s_mchKey, sizeof(k)) == 0) return;
    s_mchKey = k;
    for (int i = 0; i < k.count; i++) drawMachineRow(i, false);
}

void handleEspNowMachinesTouch(int x, int y) {
    // Confirm dialog owns all input while it is up (v2.1.10 lesson).
    if (confirmForget) {
        if (neuDialogHit(forgetDialog.cancel, x, y)) {
            confirmForget = false;
            drawEspNowMachinesScreen();
            suppressTouchBounce();
        } else if (neuDialogHit(forgetDialog.ok, x, y)) {
            if (selectedRow >= 0) espnow_remove_profile((size_t)selectedRow);
            confirmForget = false;
            selectedRow   = espnow_active_profile_index();
            drawEspNowMachinesScreen();
            suppressTouchBounce();
        }
        return;
    }

    const int n = (int)espnow_profile_count();
    for (int i = 0; i < n; i++) {
        if (isTouchInBounds(x, y, 10, machineRowY(i), 220, MCH_ROW_H)) {
            selectedRow = i;
            espnow_select_profile((size_t)i);   // switch machine; link re-establishes
            drawEspNowMachinesScreen();
            return;
        }
    }

    NeuNavItem nav[3];
    switch (neuNavHit(nav, machinesNav(nav), x, y)) {
    case NAV_ID_BACK:
        currentPendantScreen = PSCREEN_CONNECTION;
        break;
    case NAV_ID_PAIR:
        if (n < MCH_MAX) currentPendantScreen = PSCREEN_ESPNOW_PAIR;
        break;
    case NAV_ID_FORGET:
        if (selectedRow >= 0 && selectedRow < n) {
            confirmForget = true;
            drawEspNowMachinesScreen();
            suppressTouchBounce();   // a held tap must not land on the dialog
        }
        break;
    }
}

#else   // !USE_ESPNOW — screens compile away to no-ops

void enterEspNowPair() {}
void exitEspNowPair() {}
void drawEspNowPairScreen() {}
void handleEspNowPairTouch(int, int) {}
void updateEspNowPairDisplay() {}
void enterEspNowMachines() {}
void exitEspNowMachines() {}
void drawEspNowMachinesScreen() {}
void handleEspNowMachinesTouch(int, int) {}
void updateEspNowMachinesDisplay() {}

#endif  // USE_ESPNOW
