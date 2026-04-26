# Progress

## 2026-04-23

### Review Opened

- Opened `review/2026-04-23-review-04/` for an RGB runtime and authored RGB
  profile architecture review.
- Did not continue `review/2026-04-23-review-03/` because that folder records a
  closure verdict and is immutable history. This pass is a distinct
  post-closure RGB review.
- Used `prompts/initial-architecture-review.md`.

### Scope

- Reviewed `users/noah/lib/rgb/**`, RGB-related split sync, key-runtime
  feedback surfaces, the authored `rgb_config.c`, the keymap's visible layer
  data, RGB docs, and relevant upstream Charybdis RGB defaults.
- No runtime behavior, keymap data, generated introspection output, or normal
  documentation was changed.

### Completed Passes

- Checked the newest review folder and confirmed it was closed.
- Read RGB runtime orchestration, layer, auto-mouse, preview, PD-mode, combo,
  key-feedback, defaults, helper, and validation modules.
- Checked how the RGB renderer consumes key-runtime semantic feedback.
- Checked split-sync transport for auto-mouse, PD display state, preview,
  combo locality, and key-feedback semantic maps.
- Compared authored PD-mode color rows with the profile comments and
  `RGB_PD_MODE_ACTIVE_HALF_ENABLE` config.
- Checked current keymap layers and upstream default keymap RGB Matrix control
  placement.

### Initial Findings

- No `must-fix` findings found.
- One `should-fix` finding was recorded: PD-mode RGB locality was internally
  inconsistent because comments/config implied key-half feedback while the
  authored rows all render on the right half.
- Two `optional cleanup` recommendations recorded:
  - decide whether to add or document normal RGB Matrix control keys
  - make key-feedback semantic paint priority explicit if new semantics are
    added later

Reconciliation note: the PD-mode comment/config mismatch was resolved in the
implementation follow-up below. The current architecture review now records no
open `should-fix` findings.

### Verification

Passed:

- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `git diff --check`

Skipped under the review-note-only exception:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Runtime/build behavior was not changed in this pass.

### Implementation Follow-Up

- Updated `rgb_config.c` so the PD-mode color comment reflects the current
  fixed pointer-half policy and does not mention backend debug defines.
- Added fixed left/right key-feedback placement.
- Updated the key-feedback renderer to paint fixed left/right halves with the
  same global semantic priority used by both-halves mode.
- Updated RGB validation, profile introspection, `rgb_config.c` comments,
  README, `docs/RGB_CONFIG.md`, and the generated keymap overview.
- Extended `tests/host/run_rgb_layer_render_tests.sh` and
  `tests/host/rgb_layer_render_test.c` with fixed left/right key-feedback
  variants.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`
- `git diff --check`

No required checks were skipped for the implementation follow-up.

### Next Steps

1. Choose whether ordinary RGB Matrix controls should be reachable from an
   authored physical key path or intentionally left to VIA remapping.
2. If new key-feedback semantic states are added later, make feedback paint
   priority explicit instead of relying on enum ordering.
3. Keep the staged RGB runtime; this pass did not identify a reason to rewrite
   the pipeline.

## 2026-04-24

### PD Trigger-Key RGB Follow-Up

- Added exact-key PD RGB locality so pd-mode overlays can paint the exact key
  footprint that triggered the current effective PD mode.
- Extended the pd-mode owner contract from side-only display state to include
  an owner bitmap for RGB display consumers.
- Mirrored the pd-mode owner bitmap through the base split-runtime sync packet
  so the slave half can render exact-key locality.
- Updated RGB validation, the pd-mode renderer, authored RGB comments, README,
  RGB docs, profile introspection, and the generated keymap overview.
- Extended pd-mode, split-sync, and RGB render host coverage for direct,
  combo-footprint, fallback, and remote exact-key behavior.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

### PD Held-Action Projection Follow-Up

- Reproduced the crash path as a key-runtime/PD projection mismatch, not split
  sync: the physical `PINCH_MODE` key could project Pinch immediately while its
  authored double-hold branch registered `ZOOM_MODE`.
- Updated the key-runtime core so direct and implicit PD holds still attach PD
  leases on press, while explicit held-action PD branches attach and release
  their PD projection when the held action actually registers/unregisters.
- Added focused host regressions for `PINCH_MODE` double-tap hold into
  `ZOOM_MODE` and for the user-visible Volume -> Pinch/Zoom -> Volume
  alternation.
- Updated the runtime debug fixture so handled-key PD tests resolve
  `pd_mode_for_keycode()` instead of treating PD keycodes as non-PD actions.
- Regenerated `docs/KEYMAP-OVERVIEW.md` after the full host suite found stale
  generated PD color rows from the previous exact-key RGB follow-up.

Verification passed:

- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

### Next Steps

1. Flash and try the Volume/Pinch/Zoom alternation on hardware.
2. Keep explicit held-action PD branches covered as more `.mode` key behavior
   options are added.

### Pinch Double-Tap Salvo Follow-Up

- After hardware testing narrowed the repro to repeated `PINCH_MODE`
  double-tap salvos, tried moving the second-tap action off `VIA_MACRO_6` and
  onto the equivalent direct `LAG(KC_8)` chord.
- Hardware still froze with the direct chord, ruling out the VIA macro playback
  path as the root cause. Restored `VIA_MACRO_6` on the double-tap path.
- The same keymap shape was confirmed present at
  `cd8050f3b5970dba0711d8503b5bccf1c1054d6e`, so the remaining hardware
  failure is treated as a long-lived same-key PD lifecycle issue rather than a
  regression introduced by the RGB/key-feedback work.
- Changed the authored `PINCH_MODE` single-tap hold from implicit immediate
  Pinch ownership to explicit `PRESS_AND_HOLD_UNTIL_RELEASE(PINCH_MODE)`.
  Quick Pinch tap/double-tap prefixes no longer enter Pinch or take its
  mode-owned GUI lifecycle; Pinch activates only after the first hold threshold
  is crossed.
- Added focused host coverage for repeated Pinch double-tap salvos in both the
  PD/key-runtime fixture and the real authored-profile integration test, plus
  coverage that quick Pinch taps defer the mode-owned GUI path while real Pinch
  holds still mask mode-owned GUI during concurrent keyboard processing.
- Updated `docs/KEYMAP.md`, `docs/ADDING_PD_MODE.md`, and regenerated
  `docs/KEYMAP-OVERVIEW.md`.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_macro_payload_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

### Next Steps

1. Flash and hammer repeated `PINCH_MODE` double taps on hardware.
2. If the freeze remains reproducible, inspect same-key PD ownership/lifecycle
   transitions after the explicit Pinch-hold change.

### Same-Key PD Lifecycle Root-Cause Follow-Up

- Kept the authored `PINCH_MODE` keymap on the explicit-hold containment path
  and added a separate host-test legacy Pinch variant that restores the old
  implicit first-hold shape only inside
  `run_pd_mode_key_runtime_integration_tests.sh`.
- Reproduced the deeper invariant failure in host: when a held PD mode was
  preempted by another PD mode, the PD engine moved to the new active mode but
  the key runtime could retain a stale held-action owner for the old mode until
  that old physical key was released.
- Updated key-runtime effect projection so registering a held PD action, or
  activating a PD lock/tap, first unregisters held PD actions for other modes.
  This makes the PD engine, key-runtime shadow projection, held-action owner,
  PD owner key, pointer anchor, and Pinch-owned GUI lifecycle move together at
  the preemption boundary.
- Added legacy-Pinch assertions for Pinch -> Volume, Volume -> Pinch, repeated
  Pinch double-tap salvos, and legacy Pinch double-tap hold into Zoom. The
  tests assert active PD mode, key-runtime shadow PD mode, held action owner,
  PD owner key, pointer anchor, and Pinch-managed GUI state at each boundary.

Verification passed:

- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_qmk_contract_checks.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

### Next Steps

1. Flash a build with the legacy-Pinch fixture behavior if we want hardware
   proof that the root invariant is fixed independent of the safe authored
   keymap containment.
2. Decide separately whether the authored `PINCH_MODE` single hold should stay
   explicit or return to implicit after hardware validation.

### PD Clash Trace Follow-Up

- Added structured trace events for PD key press/release commands, local PD
  owner press/release/clear changes, and key-runtime held PD
  register/unregister/preemption.
- Enabled `NOAH_RUNTIME_TRACE_ENABLE` in the focused
  `run_pd_mode_key_runtime_integration_tests.sh` host runner so the Pinch/Zoom
  integration path compiles the same trace instrumentation used to inspect the
  suspected hardware clash.
- Confirmed the current source tree has the legacy implicit first Pinch hold in
  `keymap.c`. Regenerated profile introspection output so generated docs match
  that repro shape instead of masking it by moving `PINCH_MODE` back to
  `[0].hold`.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

### Next Steps

1. Flash a trace-enabled build and reproduce the repeated Pinch taps on
   hardware.
2. Compare the final trace tail for stale `PINCH_MODE`, `ZOOM_MODE`, or
   `VOLUME_MODE` held-PD events against the actual active PD mode at freeze.
3. Decide whether to keep the legacy implicit first-hold repro shape or restore
   the explicit first-hold containment after the root clash is identified.

### Stacked PD Lease-Owner Fix

- Turned the stacked-PD suspicion into a failing host regression: with legacy
  implicit Pinch active, a duplicate same-key Pinch press could cancel the old
  press token while the already-registered held `PINCH_MODE` action remained
  owned by the canceled token.
- Confirmed that the following release no longer saw its own held PD action,
  leaving `PD_MODE_PINCH` active after the key was released. This matches the
  hardware symptom better than the earlier final-state-only tests because it
  catches the illegal intermediate owner-token handoff.
- Updated held-action and repeat lease activation so re-registering the same
  key/action reassigns the existing lease to the current token instead of
  leaving it attached to a canceled token.
- Extended the legacy Pinch fixture through the stacked transition into the
  second-tap `ZOOM_MODE` hold, proving the same physical key can recover from
  the duplicate press and then hand off cleanly to the other PD mode.

Verification passed:

- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

### Next Steps

1. Flash this candidate fix and hammer repeated `PINCH_MODE` taps on hardware;
   unlike the trace-only pass, this one changes the suspected runtime behavior.
2. If hardware still freezes, use the structured PD/key-runtime trace events
   added in this thread to inspect the remaining owner transition.

### Stacked PD Tap-Path Containment

- Hardware still reproduced the freeze after the same-key lease-owner fix,
  which ruled out owner-token handoff as the whole problem.
- Moved the containment from authored data into the behavior materializer:
  when a pd-mode key has a first-tap override and a later tap-count hold can
  enter a different pd mode, the first mode is materialized as a normal
  threshold hold instead of an immediate implicit hold.
- Kept the authored `PINCH_MODE` row in the legacy repro shape:
  `[0].tap = TAP_SENDS(KC_TRNS)` and `[1].hold =
  PRESS_AND_HOLD_UNTIL_RELEASE(ZOOM_MODE)`. The runtime now supplies the
  safe first-hold containment for this stacked-pd shape.
- Added lookup coverage that proves stacked pd rows lose the
  `HANDLED_KEY_FLAG_IMPLICIT_HOLD` path and receive
  `PRESS_AND_HOLD_UNTIL_RELEASE(<base mode>)` on the first tap.
- Updated the legacy Pinch integration expectations: quick Pinch prefixes stay
  out of Pinch/GUI ownership, real first holds enter Pinch at threshold, and
  double-tap holds still enter Zoom with one coherent owner.
- Updated user-facing docs and the keymap comments so the authored behavior
  surface matches the runtime rule.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

### Next Steps

1. Flash this build and hammer repeated `PINCH_MODE` quick taps and
   double-tap holds on hardware.
2. If hardware still freezes, capture the trace tail and compare whether the
   remaining clash is outside the key-runtime/pd-mode ownership path.

### RGB Locality Naming Migration

- Added shared `rgb_locality_t` values for interaction-local RGB placement:
  `RGB_BOTH_HALVES`, `RGB_LEFT_HALF`, `RGB_RIGHT_HALF`, `RGB_KEY_HALF`, and
  `RGB_KEYS_ONLY`.
- Migrated PD-mode colors, combo feedback, and key-behavior feedback from
  separate `.mode` enums to `.locality`, with no legacy enum aliases.
- Kept layer coverage and auto-mouse fade destination on `.mode` because those
  settings are not interaction-locality choices.
- Updated the PD, combo, and key-feedback render stages, RGB validation,
  authored RGB config, profile introspection, generated overview, docs, and
  host fixtures to use the shared locality terminology.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

### Next Steps

1. Flash the RGB-locality migration build and confirm PD, combo, and
   key-behavior feedback still render in the same places as before.
2. If future interaction RGB surfaces are added, use `rgb_locality_t` directly
   instead of creating another surface-specific placement enum.

### Combo And Key-Feedback LED Groups

- Added authored LED group support to combo feedback with
  `combo_feedback_led_group_t`.
- Added authored LED group support to key-behavior feedback with
  `key_behavior_feedback_led_group_t`,
  `key_behavior_feedback_group_semantic_t`.
- Kept the group rendering rule consistent with existing layer and PD groups:
  render the normal locality/color first, then repaint matching custom LED
  groups last inside that substage.
- Combo feedback groups render inside the live combo underlay or overlay
  substage, so preview/PD ownership still decides whether they sit below or
  above those indicators.
- Key-behavior feedback groups render only when their semantic category is
  visibly active, including the same flash-phase gating as the main feedback
  color.
- Updated RGB validation, focused host tests, profile introspection, authored
  RGB comments, docs, and this active review note for the new authoring
  surface.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

### Next Steps

1. Decide whether the current profile should enable actual combo/key-feedback
   group rows, or keep the feature as an authored option for later tuning.
2. If real group rows are added, regenerate the profile overview and re-run
   RGB render/validation plus real profile validation.

### Shared RGB LED Group Section

- Moved the authored LED reference map in `rgb_config.c` into one central
  `LED Map` section.
- Replaced the reusable physical LED arrays with inline
  `.led_group = RGB_LED_GROUP(...)` authoring inside each stage-specific LED
  group table.
- Added `RGB_LED_GROUP_TABLE(...)` so empty LED group tables can stay
  materialized with only individual rows commented out, without exposing a
  sentinel row in `rgb_config.c`.
- Tightened the sentinel to a designated `.led_group = {.count = 0}`
  initializer after the firmware compile caught QMK's `-Wmissing-braces`
  warning policy.
- Added shared `rgb_led_group_t` value ownership so layer, PD-mode, combo
  feedback, and key-behavior feedback rows expose one `.led_group` field
  instead of loose `.leds` and `.count` members.
- Added reusable physical `RGB_LED_GROUP_*` definitions under the LED map so
  stage rows can name groups like `RGB_LED_GROUP_TRACKBALL` instead of
  repeating LED indices.
- Added `MATERIALIZE_RGB_CONFIG()` at the bottom of `rgb_config.c` so runtime
  exports, feedback policy bridges, and derived counts are no longer scattered
  after each authored table.
- Kept the optional layer, PD-mode, combo feedback, and key-behavior feedback
  render-table examples in their own feature sections.
- Clarified the feature sections so each LED group surface has a visible
  stage-specific heading in `rgb_config.c`.
- Updated `docs/RGB_CONFIG.md` so LED groups are authored directly in the
  stage-specific table that chooses when and how they light.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `python3 -m py_compile tools/profile_introspect.py`
- `python3 -c 'import tools.profile_introspect as p; ...'`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

### Next Steps

1. Enable actual layer, PD-mode, combo feedback, or key-behavior feedback group
   rows from the relevant feature section when tuning the profile.
2. Add reusable physical groups under the LED map, then reference them with
   `.led_group = RGB_LED_GROUP_*` in the relevant render table.

### Interaction RGB Stage Gates

- Added explicit user-facing gates for PD-mode feedback and combo feedback:
  `RGB_PD_MODE_FEEDBACK_ENABLE` and `RGB_COMBO_FEEDBACK_ENABLE`.
- Kept `RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE` and
  `RGB_AUTOMOUSE_GRADIENT_ENABLE` as the existing key-behavior and auto-mouse
  stage gates.
- Kept preview internal to the key-behavior feedback path instead of exposing
  a separate preview toggle.
- Updated `rgb_runtime.c` so disabled interaction feedback stages are skipped
  in post-init and in per-frame rendering, rather than being called as empty
  no-op stages.
- Gated authored PD-mode and combo feedback RGB tables, defaults, validation,
  profile introspection, and docs behind the matching flags.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

Next steps:

1. If a user disables one of the RGB feedback stages, keep any future authored
   table examples behind the same feature flag.
2. Hardware-test any disabled-stage profile before treating it as a final user
   configuration.

### Tap Commit RGB Feedback

- Added a distinct tap-commit feedback pulse kind and semantic so authored tap
  branches can show a short confirmation separate from multi-tap pending and
  hold/long-hold activity.
- Added `tap_committed_color` and `KEY_FEEDBACK_GROUP_TAP_COMMITTED` to the
  key-behavior RGB authoring surface, with the current profile using green for
  tap commits.
- Suppressed tap-commit pulses for layer-affecting and PD-mode-affecting tap
  actions because those actions already have persistent layer or PD feedback.
- Added an authored tap-commit feedback mode so profiles can disable commit
  pulses, pulse only double-tap and higher branches, or pulse every committed
  tap branch; the current profile uses `KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS`.
- Materialized the runtime policy hook from `MATERIALIZE_RGB_CONFIG()` so
  `rgb_config.c` stays declarative and does not hand-author bridge functions.
- Carried the tap-commit pulse flag through deferred release dispatches so the
  pulse drains with the delayed tap output instead of firing early while a
  sibling tap-release blocker is still active.
- Preserved the winning tap count before clearing pending multi-tap release
  state so `KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS` still works for branches
  that resolve on release.
- Kept the packed key-feedback semantic map at three bits; the new semantic
  uses the remaining value in that encoding.
- Updated runtime tests, RGB render/validation tests, profile introspection,
  authored RGB comments, generated layer overview assets, README/RGB docs, and
  this active review note.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `python3 -m py_compile tools/profile_introspect.py`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

Next steps:

1. Flash and hardware-test a normal tap, a deferred tap-release overlap, a
   layer-state tap, and a PD-mode tap to confirm the green pulse appears only
   on non-state tap commits.
2. If another key-feedback semantic is needed later, plan the packed semantic
   map width and priority rules first.

### Tap Branch Confirmation RGB Feedback And Split Packet Grouping

- Kept neutral unresolved key-behavior pending feedback in
  `tap_pending_color`.
- Added `RGB_TAP_BRANCH_COLORS(...)` for the short committed-branch pulse that
  runs after a tap index commits and before tap/hold/long-hold action feedback.
- Added a packed tap-branch map beside the packed key-feedback semantic map so
  different committed keys can show different tap-branch colors without consuming
  the remaining semantic encoding space.
- Updated key-feedback RGB rendering so `RGB_KEYS_ONLY` can paint each key
  footprint with its own committed branch color, while half/global localities
  use the highest visible committed branch inside the rendered scope.
- Split key-feedback sync into explicit semantic and branch transactions:
  `PUT_SPLIT_KEY_FEEDBACK_SEMANTIC_SYNC` and
  `PUT_SPLIT_KEY_FEEDBACK_BRANCH_SYNC`. `PUT_VIA_KEYMAP_SYNC` now follows them.
- Updated authored RGB config, profile introspection, generated profile docs,
  RGB docs, README, keymap docs, split tests, RGB render tests, validation
  tests, and runtime trace tests.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `python3 -m py_compile tools/profile_introspect.py`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Next steps:

1. Flash both halves together because the custom split transaction ID list
   changed.
2. Hardware-test single/double/triple tap branches on a key with visible
   multi-tap behavior to confirm branch-confirmation colors match the authored
   RGB list.

### Tap-Hold Branch Pending Feedback Regression

- Hardware feedback showed normal branch confirmation working, but tap-hold
  branch confirmation could appear swallowed by the following hold feedback.
- Root cause: the branch location needed to be represented as its own commit
  pulse rather than as unresolved pending color. Otherwise hold resolution could
  take over before the branch location was visible.
- Updated key-feedback projection so pending stays neutral and committed
  branches use a queued `TAP_BRANCH_COMMITTED` pulse before tap/hold/long-hold
  action feedback.
- Added runtime regressions that stage second-press tap-hold and terminal
  tap-only paths and assert branch confirmation precedes the action feedback.
- Updated README, interaction/RGB/keymap docs, authored RGB comments, and this
  review note to state that tap-hold branches show neutral pending first,
  committed branch confirmation second, and hold feedback after that.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

Next steps:

1. Flash and hardware-test the tap-hold branch confirmation on the affected key.
2. If a future hold semantic should intentionally override branch confirmation
   earlier, add an explicit feedback-priority helper before changing the current
   semantic priority behavior.

### Terminal Tap-Only Branch Confirmation

- Reframed tap branch feedback as committed branch-location confirmation, not
  "waiting for the next tap" feedback.
- Updated terminal tap-only branches so they resolve after the normal pending
  window instead of on press. This lets RGB show neutral pending for the same
  duration as other pending tap branches, then emit branch confirmation before
  any tap-commit pulse after the branch resolves.
- Kept scan from flushing the selected tap series while that branch's physical
  press is still active, so a held final tap-only branch remains visible and
  the pending window starts from release.
- Updated the transparent/key-behavior lookup contract and key-runtime
  scenario tests from press-resolved terminal branches to release-resolved
  terminal branches.
- Updated profile docs, generated keymap overview wording, authored keymap/RGB
  comments, and the architecture note to describe neutral pending, branch
  confirmation, and action feedback as separate stages.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 -m py_compile tools/profile_introspect.py`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

Next steps:

1. Flash and confirm the final tap-only branch shows its branch color for the
   normal pending duration, then commit color when commit feedback is enabled.
2. If branch and commit colors feel too close together on very quick taps,
   consider a dedicated sequenced feedback pulse rather than changing tap
   dispatch timing again.

## 2026-04-25

### Tap Branch Naming Cleanup

- Renamed the visible branch-feedback API pieces from pending-branch wording to
  unresolved/branch wording: `KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH`,
  `KEY_FEEDBACK_GROUP_UNRESOLVED_TAP_BRANCH`, and `RGB_TAP_BRANCH_COLORS(...)`.
- Kept `tap_pending_color` as the neutral unresolved pending feedback surface.
- Updated RGB validation messages, profile introspection, generated docs,
  authored RGB comments, and host tests to use the new branch-oriented names.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 -m py_compile tools/profile_introspect.py`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

Next steps:

1. Keep future branch-location feedback names centered on committed branch
   confirmation unless the runtime behavior changes again.
2. If another key-feedback semantic is added later, revisit explicit priority
   handling before consuming more packed semantic space.

### Profile Introspection LED Group Coverage

- Audited authored RGB color surfaces against generated
  `docs/KEYMAP-OVERVIEW.md` coverage.
- Found that active layer LED groups and PD-mode LED groups were parsed by the
  runtime config but not represented in the generated overview or swatch asset
  set.
- Added first-class profile-introspection model fields for layer and PD-mode
  LED groups, including owner, named/inline LED group, LED ids, count, authored
  HSV, and generated preview color.
- Updated generated layer SVG previews so active layer LED groups repaint their
  configured LED ids on top of the normal layer preview color. LED 56 has an
  explicit generated marker for trackball LED group previews.
- Updated generated docs so layer, PD-mode, combo-feedback, and key-feedback
  LED group sections are present even when the current profile has no active
  rows.
- Updated `docs/tooling/PROFILE_INTROSPECT.md` to describe LED-group preview
  coverage.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 -m py_compile tools/profile_introspect.py`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_profile_introspection_checks.sh`
- synthetic LED group parser/render mapping check for active layer and PD-mode
  group rows
- `git diff --check`

Skipped under the Python-tooling/docs exception:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Runtime/build behavior was not changed in this pass.

Next steps:

1. If a future hardware revision changes the physical LED order, update the
   introspection LED-to-layout map together with `rgb_config.c`'s LED map.
2. If PD-mode, combo-feedback, or key-feedback stages need full board-image
   previews later, add separate stage-preview SVGs instead of overloading the
   per-layer images.

## 2026-04-26

### Tap Feedback And Direct Dragscroll Regression Follow-Up

- Added real-profile regression coverage for the `LEFT_THUMB` double-tap hold
  `KC_ESC` path: unresolved pending feedback, committed branch pulse, steady
  hold-pending feedback, and release dispatch are now checked together.
- Scoped pending multi-tap preflight flushing to foreign non-handled key
  presses. This keeps independent authored key pending chains intact while
  making terminal tap-only actions such as `KC_LEFT_GUI` triple-tap
  `OSM(MOD_LSFT)` dispatch before the next ordinary key reaches QMK.
- Added authored NAV and POINTER `DRAGSCROLL` quick-tap salvos to verify direct
  PD-mode activation leaves no stale active slot, held owner, local lock,
  dragscroll backend state, or auto-mouse key tracker.
- Linked the real-profile thumb-layer integration runner against the actual
  key-feedback map implementation so feedback semantics can be asserted in the
  same authored-profile scenario tests.
- Updated the architecture note to record the handled-vs-non-handled
  multi-tap flush boundary and the release-hold feedback coverage.

Verification passed:

- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

Next steps:

1. Flash and verify `KC_LEFT_GUI` triple-tap Shift OSM against a real normal
   follow-up key.
2. Try repeated `DRAGSCROLL` quick taps on hardware from both NAV and POINTER
   layers to confirm the crash path is gone.

### Hardware Regression Reconciliation

The first 2026-04-26 host pass was insufficient. Hardware testing showed the
ESC feedback path, Shift OSM path, and repeated `DRAGSCROLL` path were still
not fixed, so this follow-up treats that earlier result as an audit-time
snapshot rather than a resolved state.

Root causes found in this pass:

- Delayed `OSM(MOD_LSFT)` dispatch armed QMK one-shot state, then
  `dispatch_delayed_action_at()` restored the saved keyboard mod state over
  the emitted one-shot state. The next normal key therefore did not see the
  intended Shift OSM.
- The real-profile OSM regression test only logged delayed actions; it did not
  model the one-shot side effect, so the previous host test could pass while
  the hardware behavior remained broken.
- `LEFT_THUMB` double-tap hold to `KC_ESC` emitted branch feedback only if a
  scan crossed `tap_hold_term` before release. If release itself was the first
  post-threshold event, the branch pulse was coupled to tap-commit feedback and
  was suppressed for the release-hold action path.
- Terminal no-action / hold-only branches were preserved as pending multi-tap
  chains after quick release. That stale branch state matched the repeated
  direct `DRAGSCROLL` crash shape better than the earlier cleanup-only theory.

Fixes landed:

- `users/noah/lib/key/runtime/delayed_action.c` now preserves emitted one-shot
  state when replaying delayed QMK behavior keycodes.
- `users/noah/lib/key/runtime/core/runtime.c` now carries branch feedback
  independently from tap-commit feedback, so release-hold actions can pulse the
  committed branch before clearing.
- Terminal no-action / hold-only multi-tap branches no longer stay in the
  preserve-chain path after quick release. Preserve-chain is limited to
  branches that can still accept another tap, plus the terminal tap-only
  feedback window.
- `tests/host/real_profile_thumb_layer_lock_integration_test.c` now covers
  release-crossing-threshold ESC feedback, delayed OSM side effects, and
  per-cycle `DRAGSCROLL` quick-tap state.

Verification passed:

- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

Next steps:

1. Flash and retest the exact three hardware regressions: double-tap hold
   `LEFT_THUMB` to `KC_ESC`, triple-tap `KC_LEFT_GUI` to Shift OSM followed by
   a normal key, and repeated quick `DRAGSCROLL` press/release.
2. If any of those still fail on hardware, use the existing runtime trace
   points around preflight, delayed dispatch, release resolution, and PD
   lifecycle with these narrowed repros.

### Release-Hold Feedback Sequencing

- Hardware testing confirmed Shift OSM is fixed, but the release-hold feedback
  sequence still made branch confirmation and `.hold` action feedback feel
  visually overlapped.
- Root cause: `TAP_ON_RELEASE_AFTER_HOLD(...)` branches could clear the
  release-hold slot while the branch-confirmation pulse was still active. When
  that happened, the steady hold-pending semantic disappeared before it had its
  own clean visible window.
- Updated pending multi-tap release planning so release-hold actions can queue
  hold-tier action feedback after the branch-confirmation pulse when release
  itself crosses the hold threshold.
- Updated pending multi-tap scan planning so the scanned release-hold-pending
  path also queues hold-tier feedback behind the branch pulse. If the key is
  released during the branch pulse, the queued hold feedback still remains
  visible afterward.
- Added real-profile coverage for both `LEFT_THUMB` release-crossing-threshold
  and release-during-branch cases: branch color first, hold feedback second,
  then no stale runtime state.
- Updated `docs/RGB_CONFIG.md` and the architecture note to describe the queued
  release-hold feedback behavior.

Verification passed:

- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

Next steps:

1. Flash and retest the double-tap hold `LEFT_THUMB` to `KC_ESC` path. The
   intended sequence is branch color, then hold/action feedback color, even if
   release happens during the branch pulse.

### Same-Key Terminal Tap Interruption

- Hardware testing narrowed the remaining freeze to exact third presses on
  `PINCH_MODE` and `VOLUME_MODE`, while `DRAGSCROLL` and the thumb keys did not
  reproduce the same failure.
- Root cause: the third same-key press could interrupt a terminal second-tap
  branch and dispatch that previous branch action inside the new physical press
  transition. On `PINCH_MODE` this mixed the second-tap zoom chord with the new
  PD-mode/key-transparent press; on `VOLUME_MODE` it mixed the second-tap mute
  action with the new direct Volume press.
- `DRAGSCROLL` does not hit this exact path because its second branch has no
  terminal tap action to flush, and the thumb keys continue into authored
  higher tap branches instead of interrupting a terminal one.
- Updated same-key terminal tap interruption so the previous branch's delayed
  action queues as a pending-release dispatch owned by the current physical
  press token. The previous branch drains after that third press releases
  rather than inside the third press transition.
- Kept foreign-key pending tap flushing immediate, preserving the earlier
  Shift OSM fix where a normal follow-up key must see the one-shot state before
  QMK handles it.
- Updated key-runtime trace output so deferred delayed-action flags are shown
  as flags rather than as inflated repeat counts.
- Added generic key-runtime coverage for the same-key terminal interruption and
  real-profile coverage for exact third presses on `PINCH_MODE` and
  `VOLUME_MODE`.
- The first full-host pass failed because generated profile introspection
  output was stale for the current authored combo set. Regenerated the profile
  overview and reran the full suite successfully.

Verification passed:

- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

Next steps:

1. Flash and test exact three-press `PINCH_MODE` and `VOLUME_MODE` on hardware.
2. If either still freezes, capture the trace tail around the third press and
   compare pending-release drain against PD key press/release events.
2. If the two feedback windows still feel too compressed on hardware, consider
   increasing `RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS` rather than
   changing dispatch timing.

### Higher-Tier Feedback Alignment

- Hardware testing showed the release-hold feedback sequence was better, but
  still not aligned with behavior: releasing while an older feedback color was
  visible could still leave `LOCK_LAYER(LAYER_NUM)` active because the
  long-hold threshold had already committed underneath that feedback.
- Root cause: older branch or lower-tier pulses were allowed to keep painting
  after a higher hold tier actually fired. For `LEFT_THUMB`, the double-tap
  hold branch can move from release-hold `KC_ESCAPE` into the long-hold
  `LOCK_LAYER(LAYER_NUM)` action while a previous branch pulse is still active.
- Added a replace-active feedback pulse path for threshold hold actions. If a
  higher tier commits after branch feedback was already shown, the higher-tier
  feedback replaces the older pulse immediately instead of waiting behind it.
- Stopped queueing hold feedback from the scan-only release-hold-pending path;
  hold feedback is queued on release when needed, while a still-held key uses
  live token state until a higher tier actually commits.
- Added real-profile coverage that deliberately starts branch feedback late,
  crosses the `LAYER_NUM` long-hold threshold while that branch pulse is still
  visible, and asserts the visible semantic becomes long-hold feedback with no
  stale tap-branch color.
- Updated README, `docs/KEYMAP.md`, `docs/RGB_CONFIG.md`, and the architecture
  note to state that higher-tier behavior replaces older feedback once it
  commits.

Verification passed:

- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

Next steps:

1. Flash and verify that once NUM has actually committed, the visible feedback
   no longer suggests the earlier ESC hold path is still available.

### Branch Confirmation Timing Reconciliation

Reconciliation note: the previous release-hold and higher-tier feedback
sections describe branch confirmation as an RGB pulse. That was the
audit-time implementation. The current tree supersedes it by making branch
confirmation part of the key-behavior model timing itself.

- Added global `CUSTOM_TAP_BRANCH_CONFIRM_TERM` and per-row
  `.branch_confirm_term = KEY_BEHAVIOR_TERM(ms)` support. Omitted rows inherit
  the global term; `KEY_BEHAVIOR_TERM(0)` skips branch confirmation for that
  row.
- Moved committed-branch confirmation into `tap_series_t` runtime state. RGB
  now projects `KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED` from that model
  state instead of from a standalone feedback pulse.
- Delayed tap, hold, long-hold, release-hold, and PD-mode actions until the
  branch-confirm window completes. A late scan still resolves immediately if
  the authored window has already elapsed.
- Kept foreign-key flush immediate so terminal tap outputs such as
  `OSM(MOD_LSFT)` still dispatch before the next ordinary key reaches QMK.
- Removed the previous replace-active branch-pulse plumbing and updated
  scenario, runtime-debug, real-profile, PD-mode, release-matrix, and RGB tests
  around the new model state.
- Widened runtime trace snapshot counters from `uint8_t` to `uint16_t` after
  the longer branch-confirm replay scenarios exposed the old 255-event cap.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 -m py_compile tools/profile_introspect.py`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

Next steps:

1. Flash and verify that branch color duration now lines up with the action
   model for `LEFT_THUMB`, stacked PD modes, and terminal tap branches.
