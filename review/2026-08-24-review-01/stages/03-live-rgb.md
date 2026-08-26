# Stage 03 — Live RGB Vertical Slice

Status: blocked on Stage 02

Implementation note: the reader-backed codec and a callback-only effective RGB
view have landed early. The view copies no payload during provider publication,
and its captured frame token refuses access after any later publication. It is
not installed in production and no renderer family has migrated yet, so the
compiled RGB configuration remains the exact runtime behavior.

## Objective

Make every Milestone A RGB surface read from one generation-consistent effective
profile and make Profile Studio preview, persist, compare, push, and pull those
fields without flashing.

This is the first visible end-to-end win.

## Entry Criteria

- Stage 02 codecs, store, protocol, provider publication, invalidation, and
  split convergence pass.
- RGB domain fields and capacities are frozen for Profile Wire v1.
- Volatile RGB preview and rollback semantics are accepted.
- Profile Studio fake device supports RGB candidates and status transitions.

## Schema Codec Foundation

Matching desktop and reader-backed firmware domain `0x10` v1 codecs landed
early on 2026-08-25 without advancing this stage's blocked status. They freeze
the exact Milestone A RGB payload, adapt the complete parsed Profile Studio
RGB model, canonicalize 58-bit group bitmaps independently of C macro names,
preserve renderer row order, and compose through the generic canonical profile
blob. The firmware validates and exposes rows through bounded reads without a
payload-sized RAM buffer. Exact fields, enum ids, capacities, canonical rules,
and deferrals are documented in
`tools/charybdis-profile-studio/live-link/rgb-domain-v1.md`.

This is foundation evidence, not the vertical slice: production owner
installation, all eight renderer migrations, preview/rollback, candidate
writes, persistence, split convergence, source pull, and UI actions remain
open. No deliverable below is complete from this foundation alone.

## Migration Order

Migrate one table family at a time:

1. layer colors and render modes;
2. pointing-mode colors and locality;
3. auto-mouse fade mode and end color;
4. combo feedback color and locality;
5. key-behavior feedback colors, branch colors, tap-commit mode, and locality;
6. reusable LED groups as canonical bitmaps or the Stage 00-selected format;
7. layer, pointing-mode, combo, and key-behavior stage-specific group rows;
8. runtime enable flags for authored feedback stages.

Each pass removes direct production reads of the matching compiled symbol and
adds a source or compile gate against regression.

## Runtime Contracts

- One RGB frame observes one profile generation.
- Color conversion caches and mapped-layer caches are invalidated only after a
  complete generation is published.
- An RGB preview never mutates the last committed persistent profile.
- Preview disconnect, cancel, timeout, schema mismatch, and reset restore the
  effective committed or compiled state.
- Split rendering observes committed or explicitly mirrored preview semantics
  chosen in Stage 00; no half mixes fields from different generations.
- Compiled safety ceilings such as maximum brightness continue to bound live
  values.
- Empty and maximum LED groups stay bounded to valid global LED indices.

## Profile Studio Contracts

- Existing RGB controls remain the editing surface.
- A connected compatible board offers Preview live and Apply actions according
  to D-013.
- The UI distinguishes draft, volatile preview, locally committed, source
  updated, and peer converged.
- Rapid color changes are coalesced or rate-limited and cannot create
  unbounded HID or EEPROM traffic.
- Pull updates the supported source syntax without destroying comments or
  unrelated formatting.
- Device/source diff is semantic, not textual.

## Likely Files

- users/noah/lib/profile/runtime/ RGB view and publication
- users/noah/lib/rgb/core/rgb_runtime.c
- users/noah/lib/rgb/core/rgb_validation.c
- users/noah/lib/rgb/stages/
- users/noah/lib/rgb/automouse/
- users/noah/lib/rgb/core/rgb_config_defaults.c
- users/noah/lib/rgb/core/rgb_config_helpers.h
- keyboards/.../rgb_config.c only for default materialization contract changes
- tools/charybdis-profile-studio/ RGB operation coordination
- RGB and profile host tests
- Profile Studio docs and screenshots

## Deliverables

- [ ] Effective RGB profile API (callback view and stale-frame contract landed;
      installed renderer consumer remains)
- [ ] All eight RGB families migrated
- [ ] Generation-consistent cache invalidation
- [ ] Volatile preview and rollback
- [ ] Persistent apply and split status
- [ ] Semantic source/device diff
- [ ] Push, pull, and reset for RGB
- [ ] Source gate against direct production array reads
- [ ] Updated Studio UI, docs, screenshots, stage, risks, and progress

Early effective-view evidence landed on 2026-08-26:

- the provider callback copies only the validated RGB view and identity into a
  double bank; it performs no reader calls or derived color/map work;
- compiled and validated behavior-only generations explicitly retain the
  direct authored RGB fallback instead of replaying the compiled virtual blob;
- a captured live frame contains no pointer into the runtime bank and becomes
  stale after any later publication, including another compiled-fallback
  publication; and
- focused tests cover normal, ASan/UBSan, and Cortex-M0+ builds, zero-I/O
  invalidation, installed/fallback selection, reader-backed access after
  capture, fail-closed identity mismatch, and frame staleness.

## Verification

Required targeted coverage:

- existing RGB validation and render runners
- effective-profile default/live/fallback tests for every table family
- generation switch during render attempt
- cache invalidation ordering
- group bitmap bounds, all-target rows, inheritance color, empty tables, and
  maximum capacities
- preview coalescing, cancellation, disconnect rollback, apply, partial source
  failure, and peer-pending UI tests
- Profile Studio npm run check
- resting screenshots plus real hover verification for changed tooltips
- split runtime and profile convergence tests
- real profile validation and introspection if authored inputs change
- feature gates
- full host suite
- firmware compile
- fresh resource gates
- real-board RGB preview, persist, reboot, unplug/replug, both USB orientations,
  and role-swap matrix
- git diff --check

## Exit Criteria

- Every current Profile Studio RGB surface changes the real keyboard without a
  flash.
- The change can be previewed, cancelled, persisted, rebooted, pushed, pulled,
  and reset according to the documented UX.
- Both halves display one coherent generation and converge after disruption.
- No production RGB consumer bypasses the effective profile.
- Compiled defaults render exactly as before when no live profile is valid.
- Tests, screenshots, target budgets, firmware compile, and hardware matrix are
  recorded.

## Handoff

Stage 04 reuses the same candidate, provider, source/device, and status
architecture. RGB-specific preview shortcuts must not become the behavior
activation path.
