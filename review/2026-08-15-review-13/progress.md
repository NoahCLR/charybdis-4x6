# RGB Preview-Parity Progress

## Why This Review Exists

Review 12 remains open only for a physical pointing check. Finding 13 is a
distinct RGB correctness topic, so it uses Review 13 and leaves the earlier
review's history unchanged.

## 2026-08-15 — Baseline

Passed on clean commit `ff70b7c5` before Finding 13 changes:

- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

## Audit Findings

- Normal layer rendering already resolves solid bases, layer-specific groups,
  universal groups, inheritance, and authored overrides correctly.
- Preview returns early for layers without a solid base, so explicit groups
  never render there.
- Preview ignores `RGB_LAYER_GROUP_ALL` rows.
- Preview converts the inherit sentinel directly to black.
- The normal renderer uses a reusable frame; preview writes directly to the
  driver through duplicated helpers.

## Implementation Sequence

1. [x] Add independently runnable failing parity scenarios.
2. [x] Extract one selection-aware two-phase frame renderer.
3. [x] Reuse the runtime-owned frame for the preview overlay.
4. [x] Preserve stage order and remove obsolete direct-driver helpers.

## Verification Passed

- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh`
- `git diff --check`

## Next Steps

Review 13 is closed. Use its parity fixture as the color oracle for Finding 15
RGB work instrumentation and optimization.

## 2026-08-15 — Red Parity Evidence

`sh tests/host/run_rgb_layer_render_tests.sh` fails against the unchanged
preview renderer in five independently invoked scenarios:

- inherited group color differs from normal selected-layer rendering;
- `RGB_LAYER_GROUP_ALL` is absent from preview;
- a layer without a solid base returns unpainted despite its explicit group;
- a later universal row does not override an earlier layer-specific row;
- full and two-chunk preview output differs from the normal frame.

The fixture does not change authored profile data. It compiles a dedicated host
variant with synthetic layer groups and runs each scenario in a fresh process,
so one mismatch cannot hide the others.

## 2026-08-15 — Implementation and Closure

- `rgb_layer_stage.c` now renders either the effective active set or one
  selected layer through the same base pass and authored group pass.
- Preview receives the runtime-owned primary frame, clears the requested chunk,
  renders one selected layer, and applies only painted entries.
- Universal groups, inheritance, explicit groups without a base, later-row
  overrides, and full/two-chunk output match normal selected-layer rendering.
- A host-only counter proves each selection performs one group-table scan.
- The old direct preview layer painter, group intersection helper, and direct
  group HSV conversion were removed.
- Runtime stage order is unchanged.

Target evidence:

- ordinary image: 150,896 B text, 0 B data, 245,592 B BSS;
- delta from Finding 10: -12 B text and +8 B linked-layout BSS, with no new
  production frame allocation;
- reviewed maxima: 1,904/1,920 B main and 336/768 B split.

## Closure Verdict

**CLOSED — Finding 13 resolved.** Code, mechanical enforcement, documentation,
the complete host suite, ordinary target build, and fresh reviewed-path stack
gate all agree. This folder is immutable closure history after the Finding 13
commit.
