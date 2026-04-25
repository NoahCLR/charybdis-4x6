# RGB Runtime Architecture Review

## Scope

This review records an initial architecture pass over the RGB runtime and
authored RGB profile as of 2026-04-23.

The pass used `prompts/initial-architecture-review.md`. It did not continue
`review/2026-04-23-review-03/` because that folder is closed history for a
whole-userspace file-map review. This is a separate post-closure RGB runtime
and profile review.

The initial pass was review-only. Follow-up implementation in this same active
thread added fixed left/right key-feedback placement, added exact-key PD RGB
locality, migrated PD/combo/key-feedback RGB placement to one shared locality
enum, added combo/key-feedback LED group authoring, added explicit
interaction-feedback stage gates, and updated the RGB authoring docs.

## Findings

### Must-Fix

None.

### Should-Fix

None.

### Optional Cleanup

- The runtime and upstream keyboard support normal QMK RGB Matrix effects, but
  the authored keymap does not place RGB Matrix control keycodes anywhere in
  the visible layer data at
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c:414`. By contrast,
  the upstream default 4x6 keymap exposes `RM_NEXT`, `RM_TOGG`, and `RM_PREV`
  at `../bastardkb-qmk/keyboards/bastardkb/charybdis/4x6/keymaps/default/keymap.c:49`.
  If "expected RGB modes" includes user-facing effect/toggle controls, the
  firmware has the underlying QMK modes but this profile lacks an authored
  physical path. This can remain a conscious VIA-only choice, but it should be
  documented or a small control cluster should be added.
- Key-feedback paint priority is currently encoded by enum ordinal ordering:
  `KEY_FEEDBACK_SEMANTIC_MULTI_TAP_PENDING` is last in
  `users/noah/lib/key/runtime/feedback.h:30`, and the RGB renderer selects the
  greatest semantic at `users/noah/lib/rgb/stages/rgb_key_feedback_stage.c:140`
  and `users/noah/lib/rgb/stages/rgb_key_feedback_stage.c:159`. Current
  behavior is tested and coherent, but future feedback states would be safer
  with an explicit priority helper or static assertions that document the
  intended ordering.

## Resolved During Follow-Up

- PD-mode RGB authoring comments now match the profile: `pd_mode_colors[]` uses
  `RGB_RIGHT_HALF` locality for every pointing mode, and the user-facing
  `rgb_config.c` comment describes that pointer-half policy without mentioning
  backend debug defines. Verification: `python3 tools/profile_introspect.py
  --check`, `sh tests/host/run_real_profile_validation_tests.sh`,
  `sh tests/host/run_all_host_tests.sh`, `qmk compile -kb
  bastardkb/charybdis/4x6 -km noah`, and `git diff --check`.
- Key-behavior feedback now has fixed-half placement options in addition to
  both halves, key half, and exact key. Code references:
  `users/noah/lib/rgb/core/rgb_helpers.h`,
  `users/noah/lib/rgb/stages/rgb_key_feedback_stage.c`,
  `users/noah/lib/rgb/core/rgb_validation.c`, and
  `tests/host/rgb_layer_render_test.c`.
- PD-mode RGB now has exact-key locality in addition to fixed left/right,
  both halves, and key-half locality. Code references:
  `users/noah/lib/pointing/runtime/pd_mode_state.c`,
  `users/noah/lib/state/runtime/split_runtime_sync.c`,
  `users/noah/lib/rgb/stages/rgb_pd_mode_stage.c`, and
  `tests/host/rgb_layer_render_test.c`.
- The key-runtime PD projection now follows held-action PD branches instead of
  assuming the physical keycode remains the active mode. This covers the actual
  `PINCH_MODE` double-hold path to `ZOOM_MODE` and keeps the key-runtime shadow
  coherent with the PD engine. Code references:
  `users/noah/lib/key/runtime/core/runtime.c` and
  `tests/host/pd_mode_key_runtime_integration_test.c`.
- `PINCH_MODE` was tested with an explicit single-hold containment path,
  `PRESS_AND_HOLD_UNTIL_RELEASE(PINCH_MODE)`, while keeping the double-hold
  branch on `ZOOM_MODE`. The current source tree is intentionally back on the
  legacy implicit first-hold shape for hardware repro work; generated
  introspection docs now match that repro shape. Code references:
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`,
  `tests/host/pd_mode_key_runtime_integration_test.c`, and
  `tests/host/real_profile_thumb_layer_lock_integration_test.c`.
- Same-key and overlapping PD lifecycle handoff now clears stale held PD
  owners at the preemption boundary. When a held PD action or PD lock/tap
  activates a different mode, the key runtime unregisters held actions for
  other PD modes before the new mode takes ownership. This keeps active PD
  mode, shadow projection, held-action leases, owner key, pointer anchor, and
  Pinch-owned GUI lifecycle aligned. Code references:
  `users/noah/lib/key/runtime/core/runtime.c` and
  `tests/host/pd_mode_key_runtime_integration_test.c`.
- Stacked PD keys now preserve owner-token coherence when the same physical
  key re-registers an already-active held action. If a duplicate same-key press
  arrives while legacy implicit Pinch is active, the existing held-action lease
  is reassigned to the current token so release resolution can still see and
  unregister the held PD action before branching into the second-tap
  `ZOOM_MODE` path. Code references:
  `users/noah/lib/key/runtime/core/runtime.c` and
  `tests/host/pd_mode_key_runtime_integration_test.c`.
- PD, combo feedback, and key-behavior feedback now share one RGB locality API:
  `.locality` plus `RGB_BOTH_HALVES`, `RGB_LEFT_HALF`, `RGB_RIGHT_HALF`,
  `RGB_KEY_HALF`, and `RGB_KEYS_ONLY`. Layer coverage and auto-mouse fade keep
  their existing `.mode` fields because they are not interaction-locality
  settings. Code references: `users/noah/lib/rgb/core/rgb_helpers.h`,
  `users/noah/lib/rgb/stages/rgb_pd_mode_stage.c`,
  `users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c`,
  `users/noah/lib/rgb/stages/rgb_key_feedback_stage.c`,
  `users/noah/lib/rgb/core/rgb_validation.c`, and
  `tools/profile_introspect.py`.
- Combo feedback and key-behavior feedback now support authored LED groups in
  the same last-within-substage shape as layer and PD groups. Combo groups
  repaint after combo locality inside the live combo underlay or overlay
  substage. Key-feedback groups repaint after feedback locality when their
  semantic category is visibly active. All layer, PD-mode, combo, and
  key-feedback group rows use the shared `.led_group` field backed by
  `rgb_led_group_t`; profile authors can define reusable physical
  `RGB_LED_GROUP_*` macros below the LED map and reuse them across stages.
  Code references:
  `users/noah/lib/rgb/core/rgb_helpers.h`,
  `users/noah/lib/rgb/core/rgb_config_defaults.c`,
  `users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c`,
  `users/noah/lib/rgb/stages/rgb_key_feedback_stage.c`,
  `users/noah/lib/rgb/core/rgb_validation.c`, and
  `tests/host/rgb_layer_render_test.c`.
- Interaction RGB feedback stages now have explicit user-facing gates:
  `RGB_PD_MODE_FEEDBACK_ENABLE`, `RGB_COMBO_FEEDBACK_ENABLE`,
  `RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE`, and `RGB_AUTOMOUSE_GRADIENT_ENABLE`.
  When disabled, the related stage is omitted from the RGB render path rather
  than called as an empty stage. Preview remains internal to the key-behavior
  feedback path.

## Non-Findings

- The RGB runtime shape is coherent. `users/noah/lib/rgb/core/rgb_runtime.c`
  keeps top-level rendering in a fixed order: base/layer state first, then any
  enabled combo underlay, preview, pointing-mode overlay, combo overlay, and
  key-feedback overlay. That ordering matches the docs and the host render
  tests.
- The runtime is coherent with the tap engine. RGB does not re-interpret
  `key_behaviors[]` directly; it consumes semantic outputs from
  `users/noah/lib/key/runtime/feedback.c:147` and
  `users/noah/lib/key/runtime/feedback.c:240`. The tap engine owns preview
  layer selection, pending multi-tap truth, hold/long-hold semantics, pulses,
  and combo locality. RGB only broadens or narrows that truth at paint time.
- Split behavior is correctly layered. The master computes preview, combo, and
  key-feedback state, while `split_runtime_sync` transports those surfaces to
  the slave. The slave renderer uses the mirrored semantic map instead of
  trying to duplicate key-runtime decisions.
- The authored RGB surface is broad enough for this firmware: layer all keys
  vs mapped-only, shared `.led_group` LED groups for layer/PD/combo/key
  feedback, reusable physical `RGB_LED_GROUP_*` names under the LED map,
  auto-mouse fade destination modes, shared
  `rgb_locality_t` placement for pointing-mode, combo-feedback, and
  key-feedback overlays, pointing-mode LED groups, and diagnostic override
  color are all present.
- All six registered pointing modes have profile colors:
  `DRAGSCROLL`, `VOLUME_MODE`, `BRIGHTNESS_MODE`, `ARROW_MODE`, `PINCH_MODE`,
  and `ZOOM_MODE`.

## Prior Finding Status

No prior RGB-runtime findings were open. The newest review folder before this
pass, `review/2026-04-23-review-03/`, was already closed and remains immutable
history.

## Current Architecture Assessment

The RGB runtime is in good shape architecturally. It is staged, testable, and
mostly data-driven. The tap-engine relationship is especially strong: RGB
renders semantic state exported by the key runtime instead of duplicating tap,
hold, longer-hold, multi-tap, combo-origin, or PD ownership logic.
The PD projection exported by the key runtime now also tracks explicit
held-action branches whose PD mode differs from the physical trigger key.
Stacked pd-mode keys now have runtime containment instead of relying on the
keymap to author an explicit first hold. If a pd-mode key has a first-tap
override and a later tap-count hold can enter a different pd mode, the
materializer gives the first mode thresholded
`PRESS_AND_HOLD_UNTIL_RELEASE(<base mode>)` behavior and keeps it out of the
immediate implicit-hold path. The current `PINCH_MODE` row can therefore stay
in the compact legacy shape while quick Pinch tap/double-tap prefixes remain
outside Pinch's mode-owned GUI lifecycle until a real first hold crosses the
threshold.
The earlier same-key lifecycle issue is still covered separately with a legacy
Pinch host-test fixture: if a held pd mode is preempted by another pd mode, PD
mode preemption clears stale held owners immediately instead of leaving the old
key-runtime owner live until physical release.
That fixture also covers duplicate same-key press handoff for stacked PD keys:
re-registering an already-held pd mode must transfer lease ownership to the
current press token, otherwise the eventual release cannot observe and
unregister the active PD lifecycle.

The current interaction RGB authoring surface is coherent for the main feedback
surfaces. PD, combo feedback, and key-behavior feedback all use `.locality`
with shared `RGB_*` locality values. Layer coverage and auto-mouse fade still
use `.mode` because they are not interaction-locality settings. PD, combo,
key-behavior, and auto-mouse feedback can be disabled with their stage-level
config flags; preview is intentionally not a separate user-facing toggle.

## Recommended Next Refactor Sequence

1. If ordinary RGB Matrix controls are expected on-board, add a small authored
   path for `RM_TOGG`, `RM_NEXT`, and `RM_PREV` or document that VIA remapping
   is the intended control surface.
2. If new key-feedback semantic states are added later, first make feedback
   priority explicit instead of relying on enum ordering.
3. Keep the current staged RGB runtime. No rewrite is warranted from this pass.
