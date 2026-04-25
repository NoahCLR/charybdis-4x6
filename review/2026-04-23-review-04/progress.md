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
- Added `RGB_LED_GROUP_TABLE_END` so empty LED group tables can stay
  materialized with only individual rows commented out.
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
  exports and derived counts are no longer scattered after each authored
  table.
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
