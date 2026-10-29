/* screen_connection.cpp — transport select + link status (ESPNOW_SPEC §6a), neumorphic layout
 * (spec: simulator/js/screens/connection.js)
 *   Transport  segmented UART / ESP-NOW (a change arms a 5 s reboot; tapping back cancels it)
 *   link well  wired details, or ESP-NOW state, machine, signal and receive counters
 *   actions    Pair / Machines (ESP-NOW), or the reboot countdown
 *   nav        Home | Back (FluidNC)
 */
#include "pendant_shared.h"
#include "neu_ui.h"
#include "screen_connection.h"
#include "../Comms.h"              // comms_active_mode(), transport_force_*()

#ifdef USE_ESPNOW
#include "../PeerLink.h"
#endif

#include <Esp.h>                   // ESP.restart()

// Selecting a transport writes NVS and reboots the pendant after a short
// countdown — comms_init() only runs once at boot, so the backend cannot be
// swapped live and there is no useful state to preserve across the change.

static const int CN_TP_Y = 52, CN_TP_H = 36, CN_LK_Y = 96, CN_LK_H = 104, CN_ACT_Y = 210, CN_ACT_H = 44;

enum { NAV_ID_FLUIDNC = NAV_ID_FIRST };
static const NeuNavItem kNav[] = { NAV_HOME, { NAV_ID_FLUIDNC, "FluidNC", ICON_BACK } };
static const int        kNavN  = 2;

// 0 = no restart pending.
static const unsigned long RESTART_DELAY_MS = 5000;
static unsigned long restartAtMs   = 0;
static int           lastShownSecs = -1;

// Seconds still to run, 1..5 (never 0 — at zero we reboot instead of drawing).
static int restartSecsLeft() {
    if (!restartAtMs) return 0;
    const long remain = (long)(restartAtMs - millis());
    if (remain <= 0) return 0;
    return (int)((remain + 999) / 1000);   // round up so "5" shows for a full second
}

// The transport actually running (comms_init() picked it at boot), as a
// selection value — it differs from get_transport_force() during a countdown,
// and the link well keeps describing it until the reboot.
static TransportForce activeTransport() {
    return comms_active_mode() == COMMS_MODE_UART ? TFORCE_UART : TFORCE_ESPNOW;
}

// Everything the link well shows; it repaints only when one of these changes.
struct LinkKey {
    bool     wired, radio, up, paired, got;
    char     host[32];
    uint8_t  mac[6];
    int      channel, bars, rssi;
    uint32_t rx, lost, drop;
    char     baud[16], port[16];
};
static LinkKey s_link;
static bool    s_linkValid = false;

static void linkSnapshot(LinkKey& k) {
    memset(&k, 0, sizeof(k));
    k.wired = comms_active_mode() == COMMS_MODE_UART;
    strlcpy(k.baud, pendantMachine.baudRate.c_str(), sizeof(k.baud));   // Core 1 owned
    strlcpy(k.port, pendantMachine.port.c_str(), sizeof(k.port));
#ifdef USE_ESPNOW
    if (k.wired) return;
    k.radio  = espnow_radio_ready();
    k.up     = espnow_is_connected();
    k.paired = espnow_has_saved_pairing();
    ESPNowProfileInfo info;
    const int idx = espnow_active_profile_index();
    k.got = idx >= 0 && espnow_get_profile((size_t)idx, info);
    if (k.got) {
        strlcpy(k.host, info.hostname, sizeof(k.host));
        memcpy(k.mac, info.mac, sizeof(k.mac));
        k.channel = info.channel;
    }
    k.bars = k.up ? espnow_signal_bars() : 0;
    k.rssi = k.up ? espnow_rssi() : 0;
    k.rx   = espnow_rx_bytes();
    k.lost = espnow_rx_dropped() + espnow_rx_pkt_dropped();
    k.drop = espnow_frag_aborted();
#endif
}

static void linkHead(uint16_t col, const char* word) {
    const int y = CN_LK_Y;
    neuLed(26, y + 18, col, 4);
    neuText(word, 38, y + 24, NEU_B12, col);
}

// Link well contents, drawn in screen coordinates; the band panels clip it.
static void drawLinkContents(const LinkKey& k) {
    const int y = CN_LK_Y;
    neuFillRect(14, y + 4, 213, CN_LK_H - 7, NEU_WELL);
    if (k.wired) {
        linkHead(NEU_RUN, "Wired");
        neuCaption("Baud", 22, y + 40);
        neuCaption("Port", 124, y + 40);
        neuText(k.baud, 22, y + 66, NEU_B9, NEU_TEXT);
        neuText(k.port, 124, y + 66, NEU_B9, NEU_TEXT);
        neuSmall("FluidNC over the RJ12 cable", 22, y + 82, NEU_TEXT3);
        return;
    }
    if (!k.radio) {   // the radio never started: waiting will not fix it
        linkHead(NEU_ALARM, "Radio down");
        neuSmall("ESP-NOW failed to start.", 22, y + 44, NEU_TEXT2);
        neuSmall("Power-cycle the pendant; if it", 22, y + 58, NEU_TEXT3);
        neuSmall("persists, switch to UART.", 22, y + 70, NEU_TEXT3);
        return;
    }
    linkHead(k.up ? NEU_RUN : k.paired ? NEU_HOLD : NEU_ALARM,
             k.up ? "Connected" : k.paired ? "Reconnecting" : "Not paired");
    if (!k.paired) {
        neuSmall("Tap Pair to add a machine", 22, y + 46, NEU_TEXT2);
        return;
    }
    neuText(neuFit(k.got && k.host[0] ? k.host : "(unnamed)", NEU_B9, 140), 22, y + 50, NEU_B9, NEU_TEXT);
    char buf[24];
    if (k.got) {
        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 k.mac[0], k.mac[1], k.mac[2], k.mac[3], k.mac[4], k.mac[5]);
        neuCaption(buf, 22, y + 58, NEU_TEXT3);
    }
    for (int i = 0; i < 4; i++) {
        const int h = 5 + i * 4;
        neuFillRoundRect(190 + i * 7, y + 28 - h, 5, h, 1, i < k.bars ? NEU_RUN : NEU_SH_LIGHT);
    }
    if (k.up) {
        snprintf(buf, sizeof(buf), "%ddBm", k.rssi);
        neuCaption(buf, 218, y + 34, NEU_TEXT3, NEU_RIGHT);
    }
    // Receive-path counters.  LOST: packets the radio or the RX queue never
    // handed over.  DROP: whole messages abandoned in fragment reassembly —
    // invisible to LOST, because those packets DID arrive.  Both read 0 on a
    // healthy link.
    neuFillRect(22, y + 70, 196, 1, NEU_SH_DARK);
    char rx[12], lost[12], drop[12], ch[6];
    snprintf(rx, sizeof(rx), "%lu", (unsigned long)k.rx);
    snprintf(lost, sizeof(lost), "%lu", (unsigned long)k.lost);
    snprintf(drop, sizeof(drop), "%lu", (unsigned long)k.drop);
    if (k.got) snprintf(ch, sizeof(ch), "%d", k.channel);
    else       strlcpy(ch, "-", sizeof(ch));
    const char*    caps[4] = { "RX", "Lost", "Drop", "Ch" };
    const char*    vals[4] = { rx, lost, drop, ch };
    const uint16_t cols[4] = { NEU_TEXT2, k.lost ? NEU_ALARM : NEU_TEXT2, k.drop ? NEU_ALARM : NEU_TEXT2, NEU_TEXT2 };
    for (int i = 0; i < 4; i++) {
        neuCaption(caps[i], 22 + i * 50, y + 77);
        neuSmall(vals[i], 22 + i * 50, y + 87, cols[i]);
    }
}

// The well interior is 97 px — taller than the panel scratch — so it repaints
// as two bands, each pushed whole.  The full contents are drawn into both; the
// panel clips each to its own strip.
static void drawLinkPanel(bool force) {
    LinkKey k;
    linkSnapshot(k);
    if (!force && s_linkValid && memcmp(&k, &s_link, sizeof(k)) == 0) return;
    s_link      = k;
    s_linkValid = true;
    const int top = CN_LK_Y + 4, h = CN_LK_H - 7, h1 = h / 2;
    neuPanelBegin(14, top, 213, h1);
    drawLinkContents(k);
    neuPanelEnd();
    neuPanelBegin(14, top + h1, 213, h - h1);
    drawLinkContents(k);
    neuPanelEnd();
}

static void drawTransportPanel() {
    static const char* const kItems[2] = { "UART", "ESP-NOW" };
    neuCaption("Transport", 12, CN_TP_Y - 12);
    neuClear(10, CN_TP_Y, 220, CN_TP_H);
    // Reflects the STORED choice, not the running one — after a change the
    // selection updates immediately while the countdown explains the gap.
    neuSegmented(10, CN_TP_Y, 220, CN_TP_H, kItems, 2, get_transport_force() == TFORCE_UART ? 0 : 1);
}

// Pair / Machines are only meaningful on the wireless transport.
static void drawActionRow() {
    neuFillRect(6, CN_ACT_Y - 4, 228, CN_ACT_H + 9, NEU_BG);
    if (restartAtMs) {
        char msg[24];
        snprintf(msg, sizeof(msg), "Rebooting in %d s", restartSecsLeft());
        neuInset(10, CN_ACT_Y, 220, CN_ACT_H, 12, 3);
        neuCaption("Transport changed", 22, CN_ACT_Y + 9, NEU_HOLD);
        neuCaption("Tap back to undo", 218, CN_ACT_Y + 9, NEU_TEXT3, NEU_RIGHT);
        neuText(msg, 22, CN_ACT_Y + 35, NEU_B9, NEU_HOLD);
        return;
    }
    if (get_transport_force() != TFORCE_ESPNOW) return;   // UART: nothing to pair
    NeuBtnStyle pair;
    pair.icon = ICON_PLUS;
    neuButton(10, CN_ACT_Y, 104, CN_ACT_H, "Pair", pair);
    neuButton(126, CN_ACT_Y, 104, CN_ACT_H, "Machines");
}

void enterConnection() {
    releasePanelSprites();
    restartAtMs = 0;
}

void exitConnection() {
    releasePanelSprites();
}

void drawConnectionScreen() {
    neuHeader("Connection");
    drawTransportPanel();
    neuInset(10, CN_LK_Y, 220, CN_LK_H, 12, 3);
    drawLinkPanel(true);
    drawActionRow();
    neuNav(kNav, kNavN);
}

void updateConnectionDisplay() {
    if (currentPendantScreen != PSCREEN_CONNECTION) return;
    if (restartAtMs) {
        if ((long)(millis() - restartAtMs) >= 0) {
            dbg_println("Transport changed — restarting");
            delay_ms(50);          // let the last frame land before we go
            ESP.restart();         // never returns
        }
        // Repaint the countdown only when the displayed second changes.
        const int secs = restartSecsLeft();
        if (secs != lastShownSecs) {
            lastShownSecs = secs;
            drawActionRow();
        }
    }
    drawLinkPanel(false);
}

void handleConnectionTouch(int x, int y) {
    // Transport selection — writes NVS, takes effect on restart.
    const int seg = neuSegHit(10, CN_TP_Y, 220, CN_TP_H, 2, x, y);
    if (seg >= 0) {
        const TransportForce want = seg == 0 ? TFORCE_UART : TFORCE_ESPNOW;
        if (want == get_transport_force()) return;   // already selected
        set_transport_force(want);
        // Going back to the transport that is running cancels the countdown —
        // the escape hatch for a mis-tap; anything else (re)arms it.
        restartAtMs   = want == activeTransport() ? 0 : millis() + RESTART_DELAY_MS;
        lastShownSecs = -1;
        drawTransportPanel();
        drawLinkPanel(true);
        drawActionRow();
        return;
    }

#ifdef USE_ESPNOW
    if (!restartAtMs && get_transport_force() == TFORCE_ESPNOW &&
        y >= CN_ACT_Y - 4 && y <= CN_ACT_Y + CN_ACT_H + 4) {
        currentPendantScreen = x < 120 ? PSCREEN_ESPNOW_PAIR : PSCREEN_ESPNOW_MACHINES;
        return;
    }
#endif

    switch (neuNavHit(kNav, kNavN, x, y)) {
    case NAV_ID_HOME:    currentPendantScreen = PSCREEN_MAIN_MENU; break;
    case NAV_ID_FLUIDNC: currentPendantScreen = PSCREEN_FLUIDNC;   break;
    }
}
