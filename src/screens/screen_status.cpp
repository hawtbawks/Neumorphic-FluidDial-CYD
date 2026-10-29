/* screen_status.cpp — neumorphic layout (spec: simulator/js/screens/status.js)
 *   state card  y 38 h 50   LED + state word; progress % on the right while a job runs
 *   job card    y 96 h 52   current / loaded file + progress bar
 *   DRO         y 156       work + machine
 *   feed/spindle fields y 232 h 40
 *   nav         Home | FluidNC
 */
#include "pendant_shared.h"
#include "neu_ui.h"
#include "screen_status.h"

enum { NAV_ID_FLUIDNC = NAV_ID_FIRST };
static const NeuNavItem kNav[] = { NAV_HOME, { NAV_ID_FLUIDNC, "FluidNC", ICON_GEAR } };
static const int        kNavN  = 2;

// Everything the four panels show, copied under stateMutex once per update.
struct StatusSnap {
    char status[24];
    char file[SD_PATH_MAX];
    int  jobPercent;
    int  feedRate;
    int  spindleRPM;
    char spindleDir[8];
    bool inInches;
};
static bool statusSnapshot(StatusSnap& s) {
    memset(&s, 0, sizeof(s));
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(5)) != pdTRUE) return false;
    strlcpy(s.status, pendantMachine.status.c_str(), sizeof(s.status));
    strlcpy(s.file, pendantMachine.currentFile.c_str(), sizeof(s.file));
    s.jobPercent = pendantMachine.jobPercent;
    s.feedRate   = pendantMachine.feedRate;
    s.spindleRPM = pendantMachine.spindleRPM;
    strlcpy(s.spindleDir, pendantMachine.spindleDir.c_str(), sizeof(s.spindleDir));
    s.inInches   = pendantMachine.inInches;
    xSemaphoreGive(stateMutex);
    return true;
}

// Change keys, one per panel.  valid = false forces the next update to paint.
struct StateKey { char status[24]; bool connected, synced, job; int pct; };
struct FileKey  { char file[SD_PATH_MAX]; char loaded[SD_PATH_MAX]; int pct; };
struct FsKey    { int feed, rpm; char dir[8]; bool inch; };
static StateKey   s_stateKey;
static FileKey    s_fileKey;
static FsKey      s_fsKey;
static NeuDroSnap s_droKey;
static bool       s_stateValid, s_fileValid, s_fsValid, s_droValid;

static void invalidate() { s_stateValid = s_fileValid = s_fsValid = s_droValid = false; }

void enterStatus() {
    releasePanelSprites();
    invalidate();
}

void exitStatus() {
    releasePanelSprites();
}

void drawStatusScreen() {
    neuHeader("Status");
    invalidate();
    neuInset(10, 38, 220, 50, 12, 3);
    neuInset(10, 96, 220, 52, 12, 3);
    neuInset(10, 156, 220, NEU_DRO_H, 12, 3);
    neuInset(10, 232, 106, 40, 8, 3);
    neuInset(124, 232, 106, 40, 8, 3);
    updateStatusMachineStatus();
    updateStatusCurrentFile();
    updateStatusAxisPositions();
    updateStatusFeedSpindle();
    neuNav(kNav, kNavN);
}

void updateStatusMachineStatus() {
    if (currentPendantScreen != PSCREEN_STATUS) return;
    StatusSnap s;
    if (!statusSnapshot(s)) return;   // lock busy: try again next tick
    const bool job = s.file[0] != 0;
    StateKey   k;
    memset(&k, 0, sizeof(k));
    strlcpy(k.status, s.status, sizeof(k.status));
    k.connected = pendantConnected;
    k.synced    = pendantSynced;
    k.job       = job;
    k.pct       = s.jobPercent;
    if (s_stateValid && memcmp(&k, &s_stateKey, sizeof(k)) == 0) return;
    s_stateKey   = k;
    s_stateValid = true;

    NeuChip chip;
    neuChipState(chip);
    neuPanelBegin(13, 41, 214, 44);
    neuFillRect(13, 41, 214, 44, NEU_WELL);
    if (strncmp(s.status, "Alarm", 5) == 0 && pendantSynced && pendantConnected) {
        neuLed(28, 56, NEU_ALARM, 4);
        neuText("Alarm", 42, 64, NEU_B12, NEU_ALARM);
        neuSmall(alarmDescription(String(s.status)).c_str(), 22, 73, NEU_TEXT2);
    } else {
        char word[24];
        if (strcmp(chip.label, "OFFLINE") == 0)   strlcpy(word, "Connecting", sizeof(word));
        else if (strcmp(chip.label, "SYNC") == 0) strlcpy(word, "Syncing", sizeof(word));
        else {
            strlcpy(word, s.status, sizeof(word));
            word[strcspn(word, ":")] = 0;
        }
        neuLed(28, 63, chip.color, 5);
        // Big when it fits; a long word ("Connecting") steps down a size rather than being cut short.
        const int     maxW = job ? 120 : 176;
        const NeuFont font = neuTextW(word, NEU_B18) <= maxW ? NEU_B18 : NEU_B12;
        neuText(neuFit(word, font, maxW), 42, neuCapBase(38, 50, font), font, chip.color);
        if (job) {
            char pct[8];
            snprintf(pct, sizeof(pct), "%d%%", s.jobPercent);
            neuCaption("Progress", 218, 50, NEU_TEXT3, NEU_RIGHT);
            neuText(pct, 218, 76, NEU_B12, NEU_TEXT, NEU_RIGHT);
        }
    }
    neuPanelEnd();
}

static const char* baseName(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

// Trim a path from the LEFT so the file name survives.
static void fitSmall(const char* s, int maxChars, char* out, size_t n) {
    const int len = (int)strlen(s);
    if (len <= maxChars) strlcpy(out, s, n);
    else                 snprintf(out, n, "...%s", s + len - (maxChars - 3));
}

void updateStatusCurrentFile() {
    if (currentPendantScreen != PSCREEN_STATUS) return;
    StatusSnap s;
    if (!statusSnapshot(s)) return;
    FileKey k;
    memset(&k, 0, sizeof(k));
    strlcpy(k.file, s.file, sizeof(k.file));
    strlcpy(k.loaded, pendantSdCard.loadedFile.c_str(), sizeof(k.loaded));   // Core 1 owned
    k.pct = s.jobPercent;
    if (s_fileValid && memcmp(&k, &s_fileKey, sizeof(k)) == 0) return;
    s_fileKey   = k;
    s_fileValid = true;

    neuPanelBegin(13, 99, 214, 46);
    neuFillRect(13, 99, 214, 46, NEU_WELL);
    if (k.loaded[0] && !s.file[0]) {
        char path[40];
        fitSmall(k.loaded, 32, path, sizeof(path));
        neuCaption("Ready", 22, 106, NEU_RUN);
        neuCaption("Press green", 218, 106, NEU_TEXT3, NEU_RIGHT);
        neuText(neuFit(baseName(k.loaded), NEU_B9, 196), 22, 128, NEU_B9, NEU_TEXT);
        neuSmall(path, 22, 134, NEU_TEXT3);
    } else if (s.file[0]) {
        char pct[8];
        snprintf(pct, sizeof(pct), "%d%%", s.jobPercent);
        neuCaption("Job", 22, 106);
        neuText(neuFit(baseName(s.file), NEU_B9, 150), 22, 128, NEU_B9, NEU_TEXT);
        neuText(pct, 218, 128, NEU_B9, NEU_ACCENT, NEU_RIGHT);
        neuProgressBar(22, 136, 196, s.jobPercent / 100.0f);
    } else {
        neuCaption("Job", 22, 106);
        neuText("No job running", 22, 131, NEU_S9, NEU_TEXT3);
    }
    neuPanelEnd();
}

void updateStatusAxisPositions() {
    if (currentPendantScreen != PSCREEN_STATUS) return;
    NeuDroSnap snap;
    if (!neuDroSnapshot(snap)) return;
    if (s_droValid && neuDroSame(snap, s_droKey)) return;
    s_droKey   = snap;
    s_droValid = true;
    neuPanelBegin(14, 160, 212, 61);   // labels + values: everything neuDroLabels/Values fill
    neuDroLabels(10, 156, snap);
    neuDroValues(10, 156, snap);
    neuPanelEnd();
}

// Repaints a field's interior through the panel sprite; the focus-ring outline
// (always off here) falls outside the panel and is clipped away.
static void statusField(int x, const char* cap, const char* value, const char* unit) {
    neuPanelBegin(x + 4, 236, 99, 33);
    neuFieldValue(x, 232, 106, 40, cap, value, unit);
    neuPanelEnd();
}

void updateStatusFeedSpindle() {
    if (currentPendantScreen != PSCREEN_STATUS) return;
    StatusSnap s;
    if (!statusSnapshot(s)) return;
    FsKey k;
    memset(&k, 0, sizeof(k));
    k.feed = s.feedRate;
    k.rpm  = s.spindleRPM;
    strlcpy(k.dir, s.spindleDir, sizeof(k.dir));
    k.inch = s.inInches;
    if (s_fsValid && memcmp(&k, &s_fsKey, sizeof(k)) == 0) return;
    s_fsKey   = k;
    s_fsValid = true;

    char feed[12], rpm[12], cap[20];
    snprintf(feed, sizeof(feed), "%d", s.feedRate);
    snprintf(rpm, sizeof(rpm), "%d", s.spindleRPM);
    snprintf(cap, sizeof(cap), "Spindle %s", s.spindleRPM > 0 ? s.spindleDir : "");
    statusField(10, "Feed", feed, s.inInches ? "in/m" : "mm/m");
    statusField(124, cap, rpm, "rpm");
}

void handleStatusTouch(int x, int y) {
    switch (neuNavHit(kNav, kNavN, x, y)) {
    case NAV_ID_HOME:    currentPendantScreen = PSCREEN_MAIN_MENU; break;
    case NAV_ID_FLUIDNC: currentPendantScreen = PSCREEN_FLUIDNC;   break;
    }
}
