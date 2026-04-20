# Regression: authored-key overlaps can wedge the runtime after layer/pd-mode/modifier transitions

Date: 2026-04-19  
Status: resolved in current tree; retain as historical regression snapshot

## Reconciliation Note

This document is an audit-time regression snapshot. The current production tree
now runs the `runtime_v2` reducer as the only key-runtime authority, and the
legacy slot/index files referenced later in this note were deleted during the
2026-04-20 cutover. Treat those legacy file references as historical context,
not as descriptions of the live runtime surface.

## Resolution Note

The current tree no longer treats this regression as open:

- `users/noah/lib/runtime_v2/runtime_v2.h` and `.c` are now the sole
  authoritative key-runtime state surface.
- `tests/host/pd_mode_key_runtime_integration_test.c` and
  `tests/host/real_profile_thumb_layer_lock_integration_test.c` now exercise
  the original overlap families directly, including `NAV -> DRAGSCROLL`,
  `KC_LEFT_GUI` second-tap-hold overlap, and quiescent-release cleanup.
- `tests/host/runtime_debug_test.c` still asserts that the runtime debug
  surfaces observe no stale pending-release, modifier, layer, or pd-mode state
  after those flows settle.
- The closure verification pass completed with:
  `sh tests/host/run_feature_gate_compile_tests.sh`,
  `sh tests/host/run_real_profile_validation_tests.sh`,
  `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`,
  `sh tests/host/run_runtime_debug_tests.sh`,
  `sh tests/host/run_all_host_tests.sh`, and
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.

## Summary

A regression introduced between `fdc2d77` and `29156345db90973cfc4422de1384c1a684aa94e2` causes the keyboard to enter a stuck or "wedged" state when certain authored key-behavior keys interact with momentary layers, pd modes, and multi-tap modifier holds.

At audit time this was a real hardware-visible regression. It was
timing-sensitive, affected multiple authored key families, and was not
reliably reproduced by the then-current host test harness.

## Observed Behavior

The board can wedge under several related scenarios:

- Enter `LAYER_NAV` and press `DRAGSCROLL`; the board can wedge immediately.
- Activate the authored `KC_LEFT_GUI` double-tap-hold path so it emits `KC_LEFT_ALT`, then use nav arrows repeatedly; the board can wedge after repeated use.
- Related overlap failures have also shown child taps being queued until a parent hold is released, instead of dispatching immediately.
- After the wedge, the keyboard behaves as if some internal runtime state never unwound correctly. Symptoms include delayed or queued output, stuck behavior after release, and in the worst case a board that appears to require a replug to recover.

## Expected Behavior

- Authored keys should settle cleanly across press, hold, scan, and release even when layers, pd modes, and modifiers overlap.
- A physical key release should always fully unwind the runtime state created by that physical key press, even if the active layer or pd-mode state changes while the key is held.
- Entering or leaving nav, dragscroll, or authored modifier holds must never leave the board in a stuck state.

## Known Repro Families

- Raw or authored entry into `LAYER_NAV`, followed by `DRAGSCROLL`
- Authored pd-mode keys while a momentary layer is held
- `KC_LEFT_GUI` second-tap hold to `KC_LEFT_ALT` combined with repeated authored nav-arrow usage
- More generally: overlaps involving authored multi-tap state, layer ownership, pd-mode ownership, and modifier ownership

## Scope / Pattern

This does not look like a random firmware crash or a simple keymap typo. It looks like a shared runtime-state bug.

Common ingredients across the failing cases are:

- authored key-behavior keys
- momentary layer ownership
- pd-mode activation or lock behavior
- multi-tap pending-hold state
- modifier ownership
- timing around `tap_hold_term`, scan-time hold promotion, and release ordering

The bug is systemic because it appears across:

- key behavior keys
- layer holds
- pd modes
- modifier holds
- timing-sensitive combinations of the above

## Most Likely Technical Description

The most plausible failure mode is a deterministic state leak caused by disagreement between:

- the raw physical key event path and QMK-side pre-userspace effects
- the userspace key-runtime slot / release / ownership cleanup path

In practical terms, the likely sequence is:

1. A physical key is pressed under one logical keycode/layer context.
2. Layer, pointer-layer, pd-mode, or modifier state changes while that key is still held.
3. Pre-userspace QMK behavior and the userspace runtime no longer agree on the meaning of later events.
4. A release path fails to fully unwind one of the shared states.
5. The board appears wedged because later events are blocked, misrouted, or queued behind poisoned runtime state.

## Most Likely State That Leaks

One or more of these is probably left live when it should be cleared:

- active key-runtime slot ownership
- pending multi-tap state
- deferred release queue entries
- held-action ownership
- momentary layer ownership
- pd-mode active/locked state
- auto-mouse tracker/toggle/anchor state
- managed modifier ownership

## Most Likely Affected Seams

- `users/noah/lib/key/runtime/key_runtime_process.c`
- `users/noah/lib/key/runtime/key_runtime_transition.c`
- `users/noah/lib/key/runtime/key_runtime_slot_release_active.c`
- `users/noah/lib/pointing/runtime/pointer_layer_policy.c`
- `users/noah/lib/pointing/runtime/pd_mode_lifecycle.c`
- `../bastardkb-qmk/quantum/pointing_device/pointing_device_auto_mouse.c`

## Regression Window

- Last known good: `fdc2d77`
- First known bad: `29156345db90973cfc4422de1384c1a684aa94e2` on 2026-04-19

That commit introduced momentary-layer interruption behavior and changed release/blocker semantics around authored holds, making it the most likely regression source.

## Why Timing Matters

The bug is sensitive to timing because the runtime behavior depends on:

- `tap_hold_term`
- scan-time hold promotion
- pending multi-tap commit windows
- deferred release handling
- auto-mouse / pointer-layer state at the exact time of press and release

Small differences in timing change which branch owns cleanup, which is why some paths wedge only after repeated use or specific overlap orderings.

## Why Current Tests Miss It

Current host tests can pass while the hardware still wedges. That means the host harness is missing at least one real hardware seam, most likely:

- exact pre-userspace `process_auto_mouse(...)` behavior/order
- exact physical-key press/release identity across changing layer stacks
- some interaction between QMK layer/pointer side effects and userspace runtime cleanup

So this is not just "one more edge case test missing"; it is a real mismatch between the modeled event flow and the hardware event flow.
