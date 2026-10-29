/*
 * colors.js — RGB565 colour constants.
 *
 * The block between the GENERATED markers below is produced by sync.py from the
 * firmware's colour #defines (src/cnc_pendant_config.h + src/screens/screen_probe.h),
 * so a colour tweak in the firmware propagates to the sim automatically:
 *     python3 simulator/sync.py
 * Don't hand-edit inside the markers — your changes will be overwritten. Add any
 * sim-only colour aliases BELOW the END marker instead.
 */

// === BEGIN GENERATED COLORS (sync.py — from firmware #defines) ===
const COLOR_BACKGROUND = 0x0000;
const COLOR_DARKER_BG = 0x2104;
const COLOR_TITLE = 0xFD20;
const COLOR_GRAY_TEXT = 0x7BEF;
const COLOR_ORANGE = 0xFD20;
const COLOR_GREEN = 0x07E0;
const COLOR_DARK_GREEN = 0x0360;
const COLOR_CYAN = 0x07FF;
const COLOR_BLUE = 0x1C9F;
const COLOR_RED = 0xF800;
const COLOR_YELLOW = 0xFFE0;
const COLOR_WHITE = 0xFFFF;
const COLOR_BUTTON_GRAY = 0x31A6;
const COLOR_BUTTON_ACTIVE = 0x5AEB;
const COLOR_TEAL = 0x032C;
const COLOR_TEAL_BRIGHT = 0x05B6;
const COLOR_INDIGO = 0x52B9;
// === END GENERATED COLORS ===

// Hand-maintained: LGFX built-in alias (not a firmware #define).
const TFT_RED = 0xF800;
