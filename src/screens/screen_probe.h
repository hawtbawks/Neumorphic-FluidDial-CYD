#pragma once

#include "neu_ui.h"

// ── Probe type model (pendantProbeV2.probeTypeIdx) ──────────────────────────
//   0 = Z-Height Touch Plate  — Z only.            Routine: Z Surface
//   1 = XYZ Touch Plate       — Z + XY edges.      Routines: Z Surface, XYZ Corner
//   2 = 3D Touch Probe        — full ball probe.   Routines: all four
// Only the 3D probe applies a ball-radius edge/Z offset; both plate types use
// the plate thickness for the Z offset.
enum {
    PROBE_TYPE_ZPLATE   = 0,
    PROBE_TYPE_XYZPLATE = 1,
    PROBE_TYPE_3D       = 2,
    PROBE_TYPE_COUNT    = 3,
};
inline bool probeIs3D()    { return pendantProbeV2.probeTypeIdx == PROBE_TYPE_3D; }

// Effective 3D-probe tip offset: ball radius PLUS stylus deflection.  Deflection
// is the lateral over-run measured by the calibration routine (the ball centre
// at trigger sits `deflection` outside the true face); adding it to the radius
// corrects an edge zero.  Deflection defaults to 0 (no change from a plain ball
// radius) and may be NEGATIVE for a probe that under-reads — the calibration
// stores whichever sign makes an edge probe land true.  (Centre-finding —
// Bore/Boss — is unaffected: the offset cancels across opposed faces.)
inline float probeTipOffset3D() {
    float o = pendantProbeV2.ballDia / 2.0f + pendantProbeV2.deflection;
    return o > 0.0f ? o : 0.0f;
}
inline bool probeIsPlate() { return pendantProbeV2.probeTypeIdx != PROBE_TYPE_3D; }
// Number of probe routines a given type exposes (Z, +Corner, +Bore/Boss).
inline int  probeRoutineCount() {
    return pendantProbeV2.probeTypeIdx == PROBE_TYPE_3D       ? 4
         : pendantProbeV2.probeTypeIdx == PROBE_TYPE_XYZPLATE ? 2
                                                              : 1;
}

// SCR0 — probe hub
void enterProbe();
void exitProbe();
void drawProbeScreen();
void updateProbeScreen();
void updateProbeSharedFields();   // redraw only the shared fields (flicker-free dial edit)
void handleProbeTouch(int x, int y);

// NVS persistence — shared across all probe screens
void saveProbeSettings();
void loadProbeSettings();

// Work-area selection shared by every routine screen: cycle G54..G57 (selection
// only), and make the selected system active on the controller so a probe's
// G10 L20 zero takes effect in the system on screen.  Every routine calls
// probeActivateWcs() before it probes.
void probeCycleWorkArea();
void probeActivateWcs();

// Dial acceleration helper — returns the adjusted step multiplier
float probeDialStep(int delta, float baseStep);

// Crash-safe two-pass approach along one axis: fast seek to contact, back off a
// little, then slow re-probe.  Ends just PAST the fine trigger by the machine's
// deceleration distance (see probeReturnToTrigger) — the exact trigger point is
// left in #5061/#5062/#5063.  No trailing retract.  Stays in the caller's G91
// frame; seekDist is signed (e.g. "Z", -maxZ for a downward probe).
void probeSeekFine(const char* axis, float seekDist, float seekF, float fineF);

// Move back to the exact G38.2 trigger point (#5061/#5062/#5063) before zeroing a
// WCS axis.  G38.2 *reports* the trigger position, but the machine coasts past it
// while decelerating unless FluidNC's hard_stop is set, so zeroing where it came
// to rest bakes that overshoot straight into the work offset.  Always a retreat
// away from the work, so safe at rapid.  Must be called in G90.  axis = "X"|"Y"|"Z".
void probeReturnToTrigger(const char* axis);

// ── Probe kit: shared neumorphic layout (spec: simulator/js/screens/probe_kit.js) ──
//   position strip  y 38  h 24   live work X / Y / Z
//   left column     x 10  w 110  sequence well (y 70 h 96) + diagram well (y 174 h 76)
//   right column    x 128 w 102  setting fields, then the result card (tap = next WCS)
//   hint            y 258        amber reminder, red when something blocks probing
//   nav             Home | Back (probe hub) | Probe
constexpr int PRK_POS_Y = 38, PRK_POS_H = 24, PRK_RX = 128, PRK_RW = 102, PRK_HINT_Y = 258;
extern const NeuRect PRK_SEQ;
extern const NeuRect PRK_DIAG;

enum { PRK_NAV_PROBE = NAV_ID_FIRST, PRK_NAV_SAVE, PRK_NAV_CFG };
extern const NeuNavItem PROBE_ROUTINE_NAV[3];
extern const NeuNavItem PROBE_CFG_NAV[3];

void prkPosStrip(bool force);
void prkSequence(const char* const* steps, int n, const NeuRect& r = PRK_SEQ);
void prkDiagramWell(const NeuRect& r = PRK_DIAG);
// Result card: what the routine sets, and in which work system.  Tap cycles G54..G57.
void prkResultCard(const NeuRect& r, const char* what);
void prkResultValue(const NeuRect& r, const char* what);
void prkHint(const char* msg, bool red = false);
void prkConfirm(const char* routine);
// Values are stored in mm; inch mode shows them converted.  Writes the value
// text to out and returns the unit.
const char* prkLen(float mm, int decMm, char* out, size_t n);

// Common touch handling for a routine screen.  Returns true when handled.
struct PrkTouch {
    const char*    name;                   // shown in the confirm dialog
    const NeuRect* fields;                 // in focus-index order
    int            nFields;
    const NeuRect* result;                 // result card (tap = next WCS), or null
    const char*    resultWhat;
    void (*run)();                         // sends the G-code
    void (*redraw)();                      // full repaint
    void (*redrawFields)();                // fields only
    bool (*extra)(int x, int y);           // screen-specific controls, or null
};
bool prkHandleTouch(int x, int y, const PrkTouch& o);

// Diagram parts, drawn inside a diagram well.
void prkProbeTool(int cx, int tipY, int len);
void prkArrowDown(int x, int y, int len, uint16_t c);
void prkArrowLeft(int x, int y, int len, uint16_t c);
void prkArrowRight(int x, int y, int len, uint16_t c);
