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
- [ ] Switch to `LAYER_RAISE` and tap `KC_LEFT` => one Left Arrow.
- [ ] On `LAYER_RAISE`, hold `KC_LEFT` past `CUSTOM_TAP_HOLD_TERM` but release before `CUSTOM_LONGER_HOLD_TERM` => one `Option+Left` on release.
- [ ] On `LAYER_RAISE`, hold `KC_LEFT` past `CUSTOM_LONGER_HOLD_TERM` => one `Command+Left` at the long-hold threshold and no extra `Option+Left` on release.
- [ ] Repeat the previous two checks with `KC_RIGHT` => `Option+Right` and `Command+Right`.

## Multi-Tap And Interruption

- [ ] Tap `MO(LAYER_LOWER)` once and wait for resolution => `LAYER_LOWER` locks.
- [ ] Tap `MO(LAYER_LOWER)` once, then immediately tap `KC_K` => the pending tap flushes first and `[` is emitted, not `k`.
- [ ] Double tap `MO(LAYER_LOWER)` and release quickly => `KC_MPLY`.
- [ ] Double tap `MO(LAYER_LOWER)`, hold the second tap past `CUSTOM_TAP_HOLD_TERM`, then release => `LAYER_NUM` locks.
- [ ] Tap `MO(LAYER_LOWER)` again while `LAYER_LOWER` is locked => unlocks back to the previous layer state.
- [ ] Double tap `ARROW_MODE` => arrow mode locks.
- [ ] Tap any unrelated key while a single tap on `ARROW_MODE` is still pending => the pending tap resolves instead of keeping the mode key in limbo.

## Layer Lock And Momentary Overlap

- [ ] Lock `LAYER_LOWER` with a single tap on `MO(LAYER_LOWER)`, then hold `MO(LAYER_RAISE)` => raise mappings win while held.
- [ ] Release `MO(LAYER_RAISE)` from the previous step => returns to locked `LAYER_LOWER`.
- [ ] While `LAYER_RAISE` is locked or momentary-active, use `KC_LEFT` or `KC_RIGHT` hold behavior => the custom hold logic still works on the raised layer.
- [ ] Double tap and hold `LT(LAYER_RAISE, KC_SLSH)` past `TAPPING_TERM` => `LAYER_RAISE` locks.
- [ ] Tap `LT(LAYER_RAISE, KC_SLSH)` normally => `/` with no change in tap feel.

## Pointing-Device Mode Overlap

- [ ] On `LAYER_POINTER`, hold `DRAGSCROLL` and move the trackball => scrolling only, no cursor motion.
- [ ] While still holding `DRAGSCROLL`, press and hold `ARROW_MODE` => arrow mode takes over immediately and dragscroll stops.
- [ ] Release `ARROW_MODE` after the previous step => no pointing mode remains active, because the earlier unlocked mode was canceled.
- [ ] Double tap `ARROW_MODE` => arrow mode locks.
- [ ] Tap `ARROW_MODE` once while it is locked => arrow mode unlocks.
- [ ] Double tap `DRAGSCROLL` => dragscroll locks and stays active after release.
- [ ] With dragscroll locked, double tap `ARROW_MODE` => dragscroll unlocks and arrow mode becomes the only active lock.
- [ ] In arrow mode, hold `MS_BTN1`, move the ball to select text, and switch layers while still holding `MS_BTN1` => selection shift stays correct throughout the layer change.
- [ ] In arrow mode, use `MS_BTN2` and `MS_BTN3` => copy/paste shortcuts fire without inheriting stray held modifiers.

## Auto-Mouse, Pointer Layer, Raise/Lower

- [ ] Move the trackball from `LAYER_BASE` => `LAYER_POINTER` auto-activates and the RGB timeout gradient starts.
- [ ] With auto-pointer active but no pointing-device mode running, hold `MO(LAYER_RAISE)` => raise mappings win and pointer-only bindings do not leak through.
- [ ] While a pointing-device mode is held or locked, hold a layer key such as `MO(LAYER_RAISE)` or `LT(LAYER_RAISE, KC_SLSH)` => the pointing mode stays alive and the pointer layer is not dropped underneath it.
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
