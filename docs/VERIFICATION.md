# Noah Keymap Verification

Run this checklist before refactoring the custom runtime in `lib/key/` or `lib/pointing/`.

This keymap's highest-risk behavior lives in `process_record_user()`, `matrix_scan_user()`, auto-mouse state, and split RPC sync. QMK's unit-test harness lives in the main QMK tree, so this userspace keeps an explicit manual regression suite here.

## Preconditions

1. Build the current firmware with `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.
2. Flash the same build to both halves.
3. Test with the source keymap active, not a VIA-remapped variant. If VIA state may be stale, clear it first.
4. Open a plain text editor for key output, plus one scrollable window for pointer-mode checks.
5. On macOS, expect the shortcut checks below to use Command-based behavior.

## Single Tap, Hold, Long Hold

- [ ] Tap `KC_1` once on the base layer => `1`.
- [ ] Hold `KC_1` past `CUSTOM_TAP_HOLD_TERM` => `!` repeats while held and stops on release.
- [ ] Switch to `LAYER_NAV` and tap `KC_LEFT` => one Left Arrow.
- [ ] On `LAYER_NAV`, hold `KC_LEFT` past `CUSTOM_TAP_HOLD_TERM` but release before `CUSTOM_LONGER_HOLD_TERM` => one `Option+Left` on release.
- [ ] On `LAYER_NAV`, hold `KC_LEFT` past `CUSTOM_LONGER_HOLD_TERM` => one `Command+Left` at the long-hold threshold and no extra `Option+Left` on release.
- [ ] Repeat the previous two checks with `KC_RIGHT` => `Option+Right` and `Command+Right`.

## Multi-Tap And Interruption

- [ ] Tap `MO(LAYER_SYM)` once and wait for resolution => `LAYER_SYM` locks.
- [ ] Tap `MO(LAYER_SYM)` once, then immediately tap `KC_K` => the pending tap flushes first and `[` is emitted, not `k`.
- [ ] Double tap `MO(LAYER_SYM)` and release quickly => `KC_MPLY`.
- [ ] Double tap `MO(LAYER_SYM)`, hold the second tap past `CUSTOM_TAP_HOLD_TERM`, then release => `LAYER_NUM` locks.
- [ ] Tap `MO(LAYER_SYM)` again while `LAYER_SYM` is locked => unlocks back to the previous layer state.
- [ ] Double tap `ARROW_MODE` => arrow mode locks.
- [ ] Double tap `PINCH_MODE` and release quickly => pinch mode locks.
- [ ] Double tap and hold `PINCH_MODE` past `CUSTOM_TAP_HOLD_TERM`, then move the trackball => zoom mode is active instead of pinch mode.
- [ ] Tap any unrelated key while a single tap on `ARROW_MODE` is still pending => the pending tap resolves instead of keeping the mode key in limbo.

## Layer Lock And Momentary Overlap

- [ ] Lock `LAYER_SYM` with a single tap on `MO(LAYER_SYM)`, then hold `MO(LAYER_NAV)` => nav mappings win while held.
- [ ] Release `MO(LAYER_NAV)` from the previous step => returns to locked `LAYER_SYM`.
- [ ] While `LAYER_NAV` is locked or momentary-active, use `KC_LEFT` or `KC_RIGHT` hold behavior => the custom hold logic still works on the nav layer.
- [ ] Double tap and hold `LT(LAYER_NAV, KC_SLSH)` past `TAPPING_TERM` => `LAYER_NAV` locks.
- [ ] Tap `LT(LAYER_NAV, KC_SLSH)` normally => `/` with no change in tap feel.

## Pointing-Device Mode Overlap

- [ ] On `LAYER_POINTER`, hold `DRAGSCROLL` and move the trackball => scrolling only, no cursor motion.
- [ ] On `LAYER_POINTER`, hold `PINCH_MODE` and move the trackball => scrolling only, no cursor motion, with Command held for pinch-like zoom behavior.
- [ ] While still holding `DRAGSCROLL`, press and hold `ARROW_MODE` => arrow mode takes over immediately and dragscroll stops.
- [ ] Release `ARROW_MODE` after the previous step => no pointing mode remains active, because the earlier unlocked mode was canceled.
- [ ] Double tap `ARROW_MODE` => arrow mode locks.
- [ ] Tap `ARROW_MODE` once while it is locked => arrow mode unlocks.
- [ ] Double tap `DRAGSCROLL` => dragscroll locks and stays active after release.
- [ ] Double tap `PINCH_MODE` => pinch mode locks and keeps dragscroll + Command active after release.
- [ ] With pinch mode active, double tap and hold `PINCH_MODE` => zoom mode takes over while held, then releases back to no active mode unless pinch was explicitly re-locked.
- [ ] With dragscroll locked, double tap `ARROW_MODE` => dragscroll unlocks and arrow mode becomes the only active lock.
- [ ] In arrow mode, hold `MS_BTN1`, move the ball to select text, and switch layers while still holding `MS_BTN1` => selection shift stays correct throughout the layer change.
- [ ] In arrow mode, use `MS_BTN2` and `MS_BTN3` => copy/paste shortcuts fire without inheriting stray held modifiers.

## Auto-Mouse, Pointer Layer, Nav/Sym

- [ ] Move the trackball from `LAYER_BASE` => `LAYER_POINTER` auto-activates and the RGB timeout gradient starts.
- [ ] With auto-pointer active but no pointing-device mode running, hold `MO(LAYER_NAV)` => nav mappings win and pointer-only bindings do not leak through.
- [ ] While a pointing-device mode is held or locked, hold a layer key such as `MO(LAYER_NAV)` or `LT(LAYER_NAV, KC_SLSH)` => the pointing mode stays alive and the pointer layer is not dropped underneath it.
- [ ] Release all layer keys and stop moving the trackball => `LAYER_POINTER` clears after `AUTO_MOUSE_TIME`.
- [ ] Lock dragscroll, stop moving the trackball, and wait longer than `AUTO_MOUSE_TIME` => dragscroll lock keeps the pointer behavior alive until it is explicitly unlocked.

## Split Sync

- [ ] Connect both halves in the normal split configuration and confirm they are running the same firmware build.
- [ ] Move the trackball until auto-pointer activates => both halves show the same pointer-layer and RGB timeout state.
- [ ] Lock and unlock a pointing-device mode on the master half => the slave half updates its RGB and pointing state immediately.
- [ ] Let the auto-mouse timeout expire with no active locks => both halves leave pointer state at the same time.
- [ ] Repeat the split-sync checks with the other half acting as USB master if you use both roles in practice.

## Signoff

- [ ] `qmk compile -kb bastardkb/charybdis/4x6 -km noah` passes.
- [ ] The changed behavior has been tested on hardware, not only in a compile.
