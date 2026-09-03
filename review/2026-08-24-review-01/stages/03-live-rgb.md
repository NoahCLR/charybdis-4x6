# Stage 03 — Live RGB Vertical Slice

Status: engineering vertical slice complete; hardware evidence pending

Implementation note: the reader-backed codec, callback-only effective RGB view,
and all current renderer-family migrations have landed early. The RGB orchestrator
now captures one token at the frame boundary. Layer colors/render modes and
pointing-mode colors/locality read through the effective adapter, as do reusable
group bitmaps, their layer/pointing/combo stage rows, the auto-mouse fade
mode/end color, combo-feedback color/locality, and key-feedback colors,
tap-branch ordering, locality, group rows, and tap-commit policy. The view
copies no payload during provider publication, and its captured frame token
refuses access after any later publication. The effective runtime and its
split-safe activation owner are installed only in the side-specific engineering
artifact. Normal firmware still uses the compiled RGB configuration. The
separately gated mutation artifact now compiles the Studio RGB model, routes a
canonical candidate, persists it on both halves, and activates only after exact
durable convergence; real-board proof remains open.

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

This is foundation and early-consumer evidence, not the vertical slice:
normal-firmware promotion, preview/rollback, routed candidate writes, source
pull, UI actions, and real-hardware proof remain open. Persistent split
convergence and provider publication exist behind the engineering gate. The
renderer-migration deliverables below can be complete without making the
end-to-end stage complete.

## Migration Order

Migrate one table family at a time:

1. layer colors and render modes — migrated;
2. pointing-mode colors and locality — migrated;
3. auto-mouse fade mode and end color — migrated;
4. combo feedback color and locality — migrated;
5. key-behavior feedback colors, branch colors, tap-commit mode, and locality —
   migrated;
6. reusable LED groups as canonical bitmaps or the Stage 00-selected format —
   migrated for layer, pointing-mode, combo, and key-feedback consumers;
7. layer, pointing-mode, combo, and key-behavior stage-specific group rows —
   migrated;
8. runtime enable flags for authored feedback stages — migrated for layer,
   pointing mode, auto-mouse, combo, and key feedback.

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

- [ ] Effective RGB profile API (callback view, stale-frame contract, frame
      capture, every current renderer consumer, and gated owner installation
      landed; normal-firmware promotion remains)
- [x] All eight RGB families migrated
- [x] Generation-consistent cache invalidation
- [ ] Volatile preview and rollback
- [x] Persistent apply and split status in the gated engineering path
- [ ] Semantic source/device diff
- [ ] Push, pull, and reset for RGB
- [x] Source gate against direct production renderer-array reads
- [x] Updated Studio UI, docs, screenshots, stage, risks, and progress

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

First-consumer evidence landed on 2026-08-27:

- `rgb_runtime.c` captures one effective token when the first QMK LED chunk
  starts a frame and passes that same token through normal layer rendering,
  auto-mouse destination rendering, and layer preview;
- `rgb_effective_config.c` is the only production seam that reads the compiled
  `layer_colors`, `layer_led_groups`, and `layer_led_group_count` fallbacks;
- live layer rows and canonical reusable-group bitmaps render through the same
  layer compositor, while a token made stale by later publication fails closed;
  and
- the layer render runner enforces the direct-read boundary and distinguishes
  live colors, render mode, and group placement from the compiled fixture.

Second-consumer evidence landed on 2026-08-27:

- `rgb_pd_mode_stage.c` no longer caches or directly reads the compiled
  pointing-mode colors, locality, or LED-group tables; those fallbacks are
  centralized in `rgb_effective_config.c`;
- the pointing-mode stage composes into a caller-owned frame from the same
  captured profile token used by the other migrated paths, and only applies
  that frame after effective reads complete;
- a dedicated live-vs-compiled renderer fixture distinguishes color, locality,
  mode-specific groups, all-mode groups, inherited colors, and canonical group
  bitmaps; and
- publication during the final reader-backed group lookup makes the token stale
  and clears the incomplete overlay before it reaches the LEDs. The source gate
  rejects new direct pointing-table reads outside the adapter.

Third-consumer evidence landed on 2026-08-27:

- the auto-mouse stage resolves the effective stage-enable flag, fade mode, and
  end color from the captured profile frame rather than reading or caching the
  compiled configuration;
- disabling the stage makes the orchestrator continue through ordinary layer
  rendering instead of intercepting the base path;
- the live-vs-compiled renderer fixture distinguishes the compiled
  follow-real-destination mode from the live all-keys end color; and
- a publication triggered by the final reader call is detected before LED
  application, leaving the existing LED output untouched. The source gate now
  rejects direct auto-mouse configuration reads from renderer paths.

Fourth-consumer evidence landed on 2026-08-27:

- both combo underlay and overlay passes resolve the stage-enable flag,
  color/locality, canonical group bitmaps, group colors, and inheritance from
  the captured effective frame;
- the combo stage no longer caches its converted active color and composes into
  a caller-owned frame before the runtime applies any LEDs;
- a dedicated live-vs-compiled fixture distinguishes keys-only compiled
  locality from live key-half locality and distinguishes compiled group
  placement from the live canonical bitmap; and
- disabling the live combo stage clears the composed chunk, while publication
  during the final group lookup fails stale and leaves no partial frame. The
  source gate rejects direct combo-table reads outside the effective adapter.

Fifth-consumer evidence landed on 2026-08-27:

- the key-feedback stage resolves its stage-enable flag, ordered tap-branch
  colors, committed/hold/long-hold colors, locality, canonical group bitmaps,
  semantic group selectors, and group colors from the captured effective
  frame;
- the key runtime's tap-commit decision uses the same adapter and fails closed
  to `KEY_FEEDBACK_TAP_COMMIT_OFF` if publication invalidates the read;
- the stage no longer caches converted colors and composes into a caller-owned
  frame before the runtime applies LEDs;
- a dedicated live-vs-compiled fixture covers all six visible semantics,
  branch-index selection, keys-only versus key-half locality, all-semantic and
  inherited-color groups, and stage disablement; and
- publication on the final group lookup clears the incomplete frame. The
  source gate now rejects direct key-feedback table reads outside the effective
  adapter, completing migration coverage for all current renderer families.

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
