# Eight Live-Configurable PD Modes — Migration Plan

Status: firmware/runtime, schema-2 persistence, migration and the eight-slot
live editor are implemented. Automated verification and local browser QA are
recorded below; physical migration and release acceptance remain outstanding.
This plan follows [Live Edit App Direction](../LIVE_EDIT_APP_DIRECTION.md).

## 1. Outcome and agreed scope

A user opens Charybdis Live, reads eight PD-mode slots from the keyboard, changes
their names, motion behavior and RGB settings, assigns activation actions, and
saves one complete profile to both halves. Reboot, backup and restore preserve
the same configuration without access to the repository.

- Exactly eight stable slot IDs, `0..7`, displayed as slots 1–8.
- Exactly eight corresponding PD RGB configurations, keyed by those same IDs.
- The six existing modes migrate with their behavior and references preserved;
  two slots start disabled and available for new modes.
- Each configured slot exposes momentary activation and a lock-toggle action.
  Existing key behaviors continue to own tap, hold and multi-tap gestures.
- Normal cursor movement is the fallback outside the slot bank. Auto-sniping
  remains its existing layer/CPI policy and consumes no PD slot.
- One active mode at a time. Existing mode replacement, same-mode unlock and
  lock exclusivity semantics remain the migration contract.
- The first new user-created modes map directions to keys or shortcuts.
- One firmware upgrade introduces the capability. Ordinary supported edits
  thereafter use the existing draft, review and verified Apply workflow.

The two engine families are **directional actions** and **scrolling**. Optional
modifier and button policies cover the extra behavior of Arrow and Pinch.
The final implementation must not depend on a slot being named “Arrow” or
“Pinch” to obtain those capabilities.

Not included in this delivery: arbitrary scripts/C hooks, macro programs as
motion outputs, mode/layer changes emitted recursively by motion, simultaneous
PD modes, new gesture-recognition engines, automatic foreground-app switching,
or volatile device preview. Profile Studio remains frozen.

## 2. Baseline and migration mapping

These are repository factory defaults, not a claim about the currently connected
keyboard. Migration must retain live overrides and obtain the source firmware's
effective compiled settings where they are not already portable.

| Slot ID | Initial name | Engine | Behavior that must survive |
| --- | --- | --- | --- |
| 0 | Dragscroll | Scrolling | Current local scroll algorithm, inversion, axis hysteresis, timing, CPI and lock-owned auto-mouse behavior |
| 1 | Volume | Directional, vertical | Volume down/up outputs, threshold 60, inherited DPI by default, pointer-layer anchoring |
| 2 | Brightness | Directional, vertical | Brightness down/up outputs, threshold 60, inherited DPI by default, pointer-layer anchoring |
| 3 | Zoom | Directional, vertical | Cmd-minus/Cmd-equals outputs, threshold 80, default DPI 400, existing modifier behavior |
| 4 | Arrow | Directional, dominant axis | Arrows, X/Y thresholds 40/50, default DPI 400, vertical Alt masking, selection/copy/paste and typing-layer preference |
| 5 | Pinch | Scrolling | Same local scroll tuning as Dragscroll, owned left Cmd, managed-only modifier masking and lock-owned auto-mouse behavior |
| 6 | Empty | Disabled | Inert activation actions and a retained, editable RGB configuration |
| 7 | Empty | Disabled | Inert activation actions and a retained, editable RGB configuration |

The current engine dispatch lives in
[`pd_mode_manifest.h`](../../users/noah/lib/pointing/defs/pd_mode_manifest.h),
[`pd_runtime.c`](../../users/noah/lib/pointing/runtime/pd_runtime.c), and the
files under `users/noah/lib/pointing/modes/`. Volume, Brightness and Zoom already
share the threshold helper; Arrow shares its bounded accumulator machinery.

Both Dragscroll and Pinch call this repository's
[`pd_mode_dragscroll.c`](../../users/noah/lib/pointing/modes/pd_mode_dragscroll.c).
Their activation does not enable the separate native Charybdis dragscroll
switch. Native `DRAGSCROLL_MODE`/`DRAGSCROLL_MODE_TOGGLE` in the sibling QMK fork
remain a separate compatibility surface; do not inadvertently activate both
engines. Hardware access and fork assumptions stay in `users/noah/lib/compat/`.

Stock scroll tuning is in
[`users/noah/config.h`](../../users/noah/config.h): reverse Y; H/V thresholds
2/3; H/V divisors 6/8; output interval 8 ms; buffer expiry 80 ms; start ratio
7:4; sustain ratio 5:4; axis timeout 55 ms; cross-axis decay divisor 4. Preserve
the algorithm and its exact boundary behavior before offering alternative tuning.

## 3. Proposed configuration model

The fields below define the intended semantics. The standalone
[candidate encoding](pd-mode-domain-v1.md) specifies names, numeric ranges and
error codes. Phase 0 must resolve capacity and migration before freezing it for
persistent writers. Device-reported capabilities constrain the editor.

| Area | Configuration | Rules |
| --- | --- | --- |
| Identity | Stable slot ID, bounded UTF-8 name, disabled/directional/scrolling kind | Exactly eight canonical records; names are display data, never identity |
| Activation | Momentary action and lock-toggle action per slot | Stable semantic references; tap/hold gestures stay in key behaviors |
| CPI | Inherit normal pointer CPI or explicit supported CPI | Preserve current precedence: scrolling CPI, then active sniping CPI, then directional override/normal CPI |
| Pointer layer | Keep pointer layer available or prefer typing layer | Derive validated lifecycle policy; preserve scrolling's lock-owned toggle and other modes' anchor behavior |
| Directional motion | Four optional tap outputs; horizontal, vertical or dominant-axis selection; X/Y thresholds | Positive nonzero thresholds for used axes; cursor motion consumed; unused directions emit nothing |
| Tap modifiers | Per-output ambient-modifier policy: inherit, mask selected modifiers, or emit exact shortcut | Preserve physical and other owners' modifiers after every emission |
| Scrolling | H/V inversion, thresholds, divisors, start/sustain ratios, output interval, axis timeout, buffer expiry and cross-axis decay | Preserve sticky single-axis behavior; reject zero divisors, invalid ratios and unsafe arithmetic ranges |
| Scroll modifiers | Optional owned modifier set while active | Apply Pinch's managed-only isolation to concurrent keyboard events and buffered replay; user-held modifiers remain visible |
| Button overrides | Three bounded entries for mouse buttons 1–3: pass through, consume, tap shortcut, or hold modifiers | Enough for Arrow; press/release ownership is explicit; no arbitrary executable callback |
| RGB | One existing-style HSV/locality row per slot plus slot-targeted group references | RGB remains owned by its profile domain; the mode editor edits that same record |

The initial directional output vocabulary is none, ordinary keyboard keys,
supported modifier shortcuts, and supported consumer/system HID taps. Firmware
and app use the same allowlist. Raw custom actions, firmware control commands,
PD/layer controls, macro triggers and unbalanced holds are not accepted as
motion outputs. Reuse the existing action/ownership dispatch paths rather than
injecting synthetic input back through the physical key behavior interpreter.

Arrow's initial button rows are BTN1 = hold right Shift, BTN2 = exact Cmd+C tap,
BTN3 = exact Cmd+V tap. Its horizontal arrows inherit modifiers; its vertical
arrows mask both Alt modifiers. New modes may reuse these facilities.

All directional modes retain the current compiled output ceilings: four taps
per successful pointing poll and 32 queued whole steps plus sub-step residual
per accumulator. Direction reversal, axis replacement and mode exit discard
obsolete work according to the current rules. These ceilings are firmware
safety policy, not editable per-slot values.

### Slot operations and RGB identity

- Use a fixed slot order for this release. Renaming never changes slot IDs.
- “Create” configures an empty slot. “Duplicate” copies into a chosen empty
  slot, including its RGB configuration and slot-specific group assignments,
  without copying activation bindings or changing the original slot.
- Clearing/disabling a referenced slot shows its layout, behavior and combo
  uses. The draft must remove or replace those activation references before
  Apply. Disabled slot actions are also inert in firmware as a defensive rule.
- Disabled slots retain their RGB rows but produce no overlay. RGB assignments
  to disabled slots are valid dormant data. An “all modes” group applies to
  any active configured slot, including newly configured slots 6 and 7.
- Existing RGB locality and activation-owner key/half behavior remain intact.
- Names, mode data and RGB participate in the same undo/redo, review, conflict
  detection, backup and logical Apply. No second copy of color in the PD domain.

## 4. Runtime ownership and safe publication

Keep the existing state/lifecycle owner, key-runtime bridge and action ownership
ledgers. Replace compile-time mode-specific data with a bounded effective slot
table. Firmware engines and validated policies remain compiled capabilities;
profile data selects and configures them.

Materialize immutable slot configuration when publishing a profile generation.
Keep transient accumulator, gesture and output-lease state separate from the
configuration and out of backups. Since only one mode is active, prefer one
active engine state where this preserves all release bookkeeping. A late
release must resolve against the activation that acquired its output, even if
another slot is now active; reset must not lose that association.

No EEPROM/profile decoding or allocation on ordinary pointing polls. Validate
before publication; publish the mode and RGB views coherently. Cache-copy
failure must expose an explicit unavailable/failure state rather than a partial
table or a mixture of generations. Required invalidator order and the provider's
behavior-bearing domain checks are part of the implementation contract.

Use the existing strict profile activation boundary. Audit direct PD activation
as well as key-runtime-owned paths: mode activity, locks, owned modifiers,
button overrides and retained motion must all be accounted for. The first
release waits for all active/locked PD modes to exit before publication. Show
an actionable reason such as “Unlock Arrow to finish applying”; do not force
release a user-owned key or modifier to make a save finish. Distinguish durable
commit, pending activation and verified completion in the app.

Changing modes clears old motion debt and releases only old mode-owned outputs.
Test button-held mode switches, a replacement mode using the same modifier,
late physical releases, role changes and teardown. A host app disconnect is
not a keyboard mode-exit event: committed modes continue working independently
of the app. Split loss/recovery follows the existing owner lifecycle and must
not introduce stuck output or divergent slot interpretation.

Extend transient split/runtime readout to validate the eight stable IDs and
preserve owner bitmaps and locality. Persistent definitions travel through the
existing profile replication and logical-generation machinery. Mixed or
unsupported firmware capabilities must fail compatibility checks before Apply.

## 5. Schema, compatibility and upgrade contract

This is a versioned profile feature, not an unversioned increase of constants.
Current code has six-mode action/RGB limits, a four-domain blob, a 4,064-byte
custom payload ceiling, five mode-DPI scalars, generated native keycodes and
app-side six-mode name/address arithmetic. Audit each together.

### Encoding and limits

Introduce a dedicated versioned PD-mode domain. Reserve its domain ID and
capability bit only after auditing all existing allocations. Specify the blob,
RGB-domain, compiled-readback, action-ABI and portable-document versions that
change; a new domain does not automatically require changing the HID envelope.
Extend domain arrays, validator masks, RGB limits, effective providers and
compiled readback together. Both firmware and app reject unknown engines,
reserved bits, duplicate/missing IDs, malformed names and invalid references.

The supported slot mask is distinct from the configured/enabled slot mask:
all eight IDs exist even when some slots are disabled. Complete new-format
exports explicitly contain all eight slots and all eight RGB records.

The selected storage plan preserves the full lower 8 KiB VIA bank and grows
both profile slots to 5 KiB: 18 KiB logical EEPROM and 36 KiB wear-level backing.
This supersedes the initial preference for unchanged geometry. The complete
capacity proof and backup/restore requirements live in the
[PD-domain specification](pd-mode-domain-v1.md). Independent domain maxima need
not all fit simultaneously; the app must report total capacity before upload.
Never truncate an old profile to make room. Eight slots remain the product scope.

The first executable candidate adds 790 bytes. A valid old complete profile
with 32 combos and a full IR macro payload would require 4,170 bytes, exceeding
the current ceiling by 106. The size preflight detects this without changing
the source document. The planned 5,088-byte payload ceiling fits even a full
4,064-byte old payload plus that growth, with 234 bytes remaining. Schema-2
firmware now selects 5,088 bytes; the legacy bridge retains 4,064.

The current `NQ` storage header also only has four domain-mask bits; bit 4
already stores origin. Opt-in `NR` format-3 support now resolves the packing,
binding schema 2.0 and validating domain versions per format. PD-enabled writers
select format 3; bridge writers retain format 2. See the domain specification for the exact packing and the
remaining owner/wire/split integration gates.

Measure cache/provider/validator growth and fresh linked memory per half; check
reviewed stack paths and hardware cadence. Follow
[memory-budgets.md](memory-budgets.md). Policy margin is not physical RAM
capacity, and a boot allocator span is not runtime high-water evidence.

### Preserve action identities

Keep semantic IDs 0–5 mapped as above. Freeze existing native mode keys, lock
keys, layer locks, macro keys and user-trigger values with golden fixtures.
Simply extending `NOAH_PD_MODE_LIST` would shift parts of the generated enum.
Use an explicit native mapping and audit a collision-free allocation for the
four new slot-6/7 actions. If a preserved mapping is impossible for a supported
ABI, require an explicit tested translation rather than silently renumbering.

The capability/action vocabulary must be independent of names, enabled slots
and configured outputs. A blank profile and a populated profile expose the
same eight-slot vocabulary. Replace app-side enum arithmetic with the negotiated
mapping. Audit native references in VIA layout, behavior targets/actions,
combos and any supported serialized macro instructions; reject opaque references
that cannot be translated safely.

### Migrate current values without inventing missing state

Existing settings scalars 10–14 seed the new slot CPI values. The old shared
dragscroll CPI seeds both Dragscroll and Pinch; thereafter they can differ.
Move authoritative CPI ownership into the PD domain. Specify a versioned
retirement/normalization of the old scalars so two editors cannot disagree.
Preserve zero/inherit semantics and current sniping precedence.

Old exports do not contain all compiled thresholds, scroll tuning or Arrow/
Pinch policies. An action-ABI digest alone does not identify those values.
Support exact migration from device-reported parameters or a verified source
firmware identity that covers those parameters. Unknown values must be reported
as unavailable, not replaced silently with the destination firmware's defaults.

Before upgrading a daily-use board, retain its old complete backup and firmware
pair. If the installed firmware cannot report the missing settings, provide a
specifically compatible PD migration-readback build retaining its storage and
identity, or establish a verified baseline from that exact build. A bridge
compiled with today's defaults cannot recover unknown historic constants. The
existing `--snapshot-bridge` is a five-layer migration tool, not already a PD
migration bridge.

Stage the upgraded document for review, migrate the six colors and group
references, and create explicit disabled slots/RGB defaults for IDs 6–7. Export
the materialized result before installing a potentially incompatible image.
After upgrading both halves, import through the normal logical Apply and verify
readback and runtime behavior. Preserve old backups for a documented downgrade
path; never restore an eight-slot document into old firmware by dropping slots.

| Combination | Required behavior |
| --- | --- |
| New app, old firmware | Existing supported editing continues; eight-slot creation is visibly unavailable |
| Old app, new firmware/profile | Existing incompatibility handling prevents destructive writes; test this explicitly |
| New firmware, old stored record | Recognized version/identity migration or explicit recovery; never silent default replacement presented as success |
| New app, old export | Migrate only when missing PD data is verified; otherwise retain the file and explain the missing information |
| New app/firmware, new export | Exact eight-slot round-trip including disabled slots, RGB and all references |
| Mismatched halves or unknown ABI | Reject Apply before mutation; retain the draft and recovery artifact |

Changing the action/default digests can affect boot record selection even when
EEPROM geometry is unchanged. Test that transition and downgrade recovery before
distributing the new pair. Keep authoritative specs current as each contract
lands: [Profile Wire](profile-wire-v1.md), [portable profiles](portable-profile-v1.md),
[authority](authority-state-table.md), [split profiles](profile-split-v1.md),
[logical Apply](logical-profile-transaction-v1.md), and
[field classification](field-classification.md).

## 6. Delivery sequence and exit criteria

Each implementation phase extends tests in the same pass. Infrastructure is
not a completed product slice until its stated device workflow is demonstrated.
Do not flash or mutate a connected keyboard merely to implement this plan.

| Phase | Work | Exit evidence |
| --- | --- | --- |
| 0 — Freeze contracts and baseline | Capture existing behavior fixtures; specify slot/policy semantics, numeric bounds, native IDs, versions, migration, storage sizing and resource/performance thresholds | Reviewed encoding/compatibility tables, old/new fixtures, per-profile size accounting and a viable upgrade route; no unresolved identity or storage blocker |
| 1 — Establish the shared engines | Parameterize current directional and scroll handlers; implement reusable modifier/button policies; retain old entry points temporarily as adapters | Old and new engines produce equivalent reports, actions and cleanup for all six presets, including timing boundaries and ownership interactions |
| 2 — Publish and read eight slots | Add codecs/validators, compiled defaults, effective cache, safe activation, eight RGB rows, action mappings and split support; app reads names and fields from device | All eight slots read correctly through the posted model and rendered UI; six defaults work; disabled slots are inert; incompatible clients cannot overwrite them |
| 3 — First editable device slice | Build slot list/detail editor, create/duplicate/clear, directional outputs, name/DPI/RGB controls, activation picker, draft/review/undo and complete save/export/import | Create Media in slot 6, bind hold/lock, configure RGB, Apply to both halves, reboot and export/import with exact readback; existing six modes remain usable |
| 4 — Complete scroll and editing controls | Expose scroll tuning, owned modifiers, per-output modifier filters, pointer-layer policy and button overrides with capability/range validation | Edit Dragscroll; independently edit Pinch; reproduce Arrow in slot 7 with its button policies; names/slot positions do not select behavior |
| 5 — Complete migration and retire adapters | Execute supported upgrade/recovery cases; move six factory definitions into authored data; remove per-mode implementation/dispatch dependencies and duplicate DPI ownership | All six run as configuration records; restore works on blank authored-profile firmware; all acceptance rows below pass; no permanent legacy-handler dependency |

Phase 3 prioritizes the requested custom directional shortcuts. Phases 4–5 are
required to finish the agreed migration; preserving an opaque Arrow handler
indefinitely is not completion.

### Placement of work

| Responsibility | Location |
| --- | --- |
| Authored factory slot records | `keyboards/bastardkb/charybdis/4x6/keymaps/noah/`; read by firmware's compiled-default provider |
| Engines, mode state, lifecycle and pointer policy | `users/noah/lib/pointing/` |
| Modifier/button output ownership | Existing `users/noah/lib/action/` and `users/noah/lib/state/ownership/` APIs |
| Schema, validation, effective cache, activation and transactions | `users/noah/lib/profile/` |
| QMK native mapping, hardware integration and HID adapter | `users/noah/lib/compat/` |
| RGB identity/locality and transient peer state | `users/noah/lib/rgb/` and `users/noah/lib/split/` |
| Live codecs and wire formats | `tools/charybdis-live/core/schema/` and `core/protocol/` |
| Portable data, references, diff and drafts | Live `core/model/` and `core/session/` |
| Slot editor | New bounded module(s) in live `webview/`; extension relays messages |

The live app must not read authored firmware files. Runtime code must not include
`noah_keymap.h`; authored translation units must not include `noah_runtime.h`.
Wire new firmware source files into `users/noah/source_manifest.mk` and mirrored
host gates immediately. Extend introspection for any new authored surface, and
regenerate existing outputs whenever its current authored inputs change.

## 7. Verification and acceptance

### Required automated evidence

| Area | Meaningful cases |
| --- | --- |
| Motion parity | All six presets; signed 16-bit extremes; reversal; equal diagonals; axis switches; zero-motion backlog drain; scroll wrap-safe timing and exact timeout boundaries |
| Modifier/button ownership | Physical Cmd/Alt/Shift plus owned holds; copy/paste isolation; two owners of the same modifier; intercepted presses with release after switching modes; mode lock/unlock cleanup |
| Publication | Every entry path, active/locked modes, queued output, pending releases, peer wait, cache failure and generation transition; no force-release shortcut |
| Schema and capacity | Exact eight rows and eight RGB records; slot 7; disabled records; unsupported actions; malformed UTF-8; invalid numeric combinations; truncated/oversized payloads; C/JS shared fixtures |
| Identity/migration | Existing native IDs fixed; every supported ABI; missing legacy PD values; old stored-record selection; complete reference retention; blank/populated firmware vocabulary equality |
| UI and drafts | Device-to-rendered-model values including zero; duplicate/clear uses; retained edits after failed Apply or reconnect; stale-base rejection; undo/redo of PD and RGB together; unsupported firmware |
| Persistence and split | Complete readback equality, both-half convergence, reboot/USB-role changes, interruption on both sides of the logical decision, recovery retries and incompatible peers |

Run matching concrete runners while editing, not only at the end:

```sh
# Pointing, lifecycle, ownership and RGB
sh tests/host/run_pd_mode_tests.sh
sh tests/host/run_pd_mode_handlers_tests.sh
sh tests/host/run_pd_runtime_tests.sh
sh tests/host/run_pointer_layer_policy_tests.sh
sh tests/host/run_pd_mode_key_runtime_integration_tests.sh
sh tests/host/run_split_runtime_sync_tests.sh
sh tests/host/run_action_dispatch_tests.sh
sh tests/host/run_action_lifecycle_tests.sh
sh tests/host/run_keyboard_mod_ownership_tests.sh
sh tests/host/run_owned_keycode_tests.sh
sh tests/host/run_held_action_tests.sh
sh tests/host/run_rgb_validation_tests.sh
sh tests/host/run_rgb_layer_render_tests.sh
sh tests/host/run_effective_rgb_runtime_tests.sh

# Profile contracts, activation and persistence
sh tests/host/run_profile_blob_v1_tests.sh
sh tests/host/run_profile_rgb_v1_tests.sh
sh tests/host/run_profile_settings_v1_tests.sh
sh tests/host/run_profile_pd_v1_tests.sh
sh tests/host/run_profile_compiled_defaults_v1_tests.sh
sh tests/host/run_profile_validator_v1_tests.sh
sh tests/host/run_profile_validator_work_budget_tests.sh
sh tests/host/run_profile_activation_policy_tests.sh
sh tests/host/run_effective_profile_provider_tests.sh
sh tests/host/run_profile_owner_tests.sh
sh tests/host/run_profile_wire_v1_tests.sh
sh tests/host/run_profile_candidate_transaction_tests.sh
sh tests/host/run_profile_split_reconciler_tests.sh
sh tests/host/run_qmk_via_logical_profile_tests.sh
sh tests/host/run_qmk_contract_checks.sh
sh tests/host/run_feature_gate_compile_tests.sh
sh tests/host/run_real_profile_validation_tests.sh
```

Add dedicated PD-domain tests and new-version codec coverage, and include them
in the full host suite. Preserve old-version tests for migration. From
`tools/charybdis-live/`, run `npm run check` and
`npm run keycodes -- --check`; the latter is a developer check against the
vendored catalog, not an app runtime repository dependency. Visually exercise
the actual slot editor, picker labels, RGB controls and validation states.

For passes changing existing introspection inputs, run
`python3 tools/profile_introspect.py --write` and
`python3 tools/profile_introspect.py --check`. Before handing back implementation,
run `git diff --check` and `sh tests/host/run_all_host_tests.sh`. Only after
required host checks pass, run:

```sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
sh tests/host/run_all_profile_compile_tests.sh
sh tools/build-firmware-pair.sh
sh tests/host/run_firmware_memory_budget_checks.sh
sh tests/host/run_firmware_stack_budget_checks.sh
sh tests/host/run_live_profile_owner_stack_budget_checks.sh
```

The generic compile does not prove the side-specific live owner. Retain and
measure fresh owner-enabled artifacts per half; run the memory gate against
each intended fresh target, before a later build replaces its ELF. The current
pair script reuses one target name and leaves the left build last, so a single
post-pair measurement is only left-half evidence. Builds may write generated
artifacts in sibling QMK/build directories; report this separately from source
edits. No sibling source change is planned.

### Hardware release gate

Record firmware identity, app version, profile digest, both-half state, actions,
expected/observed results and recovery artifacts for each case:

1. Six migrated defaults preserve actual movement feel, buttons, modifiers,
   activation gestures, pointer-layer behavior and RGB locality.
2. Both new slots can be configured, bound, locked, unlocked and colored; slot 7
   specifically proves the upper-bound identity path.
3. A fresh app with no firmware workspace reads and edits the saved slots.
4. Complete backup/restore survives reboot and USB-role changes, including
   restore to firmware with empty authored behaviors/combos.
5. Locked/held modes make Apply wait with a useful reason; ordinary release
   permits completion; modifiers and mouse buttons never remain stuck.
6. Disconnect/power interruption before and after the durable logical decision
   recovers the old or new complete generation, with no mode/RGB/VIA mixture.
7. Mixed-version halves, stale drafts, unsupported old clients and invalid
   profiles refuse mutation without losing the prior configuration.
8. Pointing cadence, scan/RGB cost, save time and per-half memory/stack evidence
   meet thresholds established in phase 0. Account for the inherited
   [pointing-cadence issue](pointing-cadence-known-issue.md); do not claim that
   this migration resolves it without measurement.

## 8. Progress and next step

- 2026-09-17: agreed eight mode slots, eight RGB configurations, migration of
  all six existing modes, and directional shortcuts as the first editing slice.
  Inspected current engines, local scrolling ownership, schema/ABI coupling,
  activation safety and live-app layers. This plan is documentation only.
- 2026-09-19: started phase 0. Implemented standalone eight-slot C validation
  and live-app encoding/decoding, independent six-preset golden bytes, native-ID
  regression assertions, and complete-profile capacity preflight. Added the
  validator to the source manifest and host suite. Differential tests cover
  7,765 payloads under normal and sanitized C builds; current profile writers
  continue rejecting the new domain. No mode runtime or authored input changed.
- Found two integration constraints: the candidate adds 790 bytes and cannot
  fit every valid old profile; the deployed storage header has only four domain
  bits. Documented both in the candidate specification. Neither is resolved by
  merely increasing the mode count.
- 2026-09-19, continuation: implemented opt-in `NR` headers and format-specific
  domain-version validation in all three store scanners. Added exhaustive
  identity packing, schema rejection, durable prepare and partial-write/reboot
  tests. Selected two 5 KiB slots preserving the complete VIA bank; preflight
  now reports current and planned capacity separately and proves a maximum old
  payload plus 790 bytes fits. Production geometry and writer format stay put.
- 2026-09-20: integrated schema-2 owner/candidate/split storage, authored six
  presets plus two disabled slots, immutable validated cache, configurable
  directional/scroll engines, per-slot DPI and pointer policies, button leases,
  eight RGB rows and preserved native action IDs. The live app now edits all
  slots in its shared draft and migrates complete backups using bridge readback.
- Removed the duplicate boot profile view to retain the unchanged owner-state
  limit. Deliberately increased the PD artifact's static-RAM policy by 3 KiB
  for the agreed geometry/cache expansion; legacy policy is unchanged. See
  [fresh linked accounting](memory-budgets.md).
- Next: complete the physical two-half upgrade/interruption matrix below,
  measure cadence and allocator/stack high-water, and verify the backup/restore
  round trip on the connected keyboard. No physical flash is implied by the
  completed implementation or host evidence.

### Foundation checkpoint verification — 2026-09-19

Passed:

```sh
sh tests/host/run_pd_mode_handlers_tests.sh
sh tests/host/run_profile_compiled_defaults_v1_tests.sh
sh tests/host/run_profile_pd_v1_tests.sh
sh tests/host/run_feature_gate_compile_tests.sh
sh tests/host/run_all_host_tests.sh
# From tools/charybdis-live/:
npm run check
npm run keycodes -- --check
# From the repository root:
qmk compile -kb bastardkb/charybdis/4x6 -km noah
sh tests/host/run_firmware_memory_budget_checks.sh
git diff --check
```

The live-app check passed all 355 tests. The full host suite includes normal
and ASan/UBSan runs of the 7,765-case PD corpus. The generic firmware build
initially hit sandbox restrictions writing generated QMK files; it passed on
retry with build-directory access. No sibling source was edited. Build artifacts
were generated in the sibling QMK checkout and copied to this userspace.

The fresh generic artifact passed its memory policy; this is not side-specific
owner-enabled or runtime high-water evidence. Pair builds, dedicated stack
builds, device migration, visual editor checks and hardware cadence/acceptance
remain for integration. The all-profile wrapper was not repeated because its
only configured target is the same `noah` target just compiled. No authored
introspection input changed, so regeneration was unnecessary. No board was
flashed or live configuration written.

Keep progress here and decisions beside their governing contracts. The direction
document retired dated review folders; this feature does not recreate them.

### Storage-header checkpoint verification — 2026-09-19

The store's schema/format admission, identity packing, synchronous validation,
bounded boot scan and marker-last commit now cover `NR`. The source manifest,
deployed schema, compiled-default/action digests and authored data are unchanged
by this continuation. Targeted checks passed:

```sh
sh tests/host/run_profile_store_tests.sh
sh tests/host/run_profile_candidate_store_backend_tests.sh
sh tests/host/run_profile_store_runtime_tests.sh
sh tests/host/run_profile_owner_tests.sh
sh tests/host/run_profile_split_reconciler_tests.sh
sh tests/host/run_feature_gate_compile_tests.sh
# From tools/charybdis-live/:
npm run check
npm run keycodes -- --check
```

The live suite has 356 passing tests. The full host suite passed after the code
changes; its final pass also includes the corrected stack manifest. Firmware
builds passed for the generic target, the owner-enabled left stack target and
the owner-enabled right target:

```sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
sh tests/host/run_live_profile_owner_stack_budget_checks.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah \
  -e NOAH_PHYSICAL_HALF=right -e FORCE_MASTER=yes \
  -e TARGET=bastardkb_charybdis_4x6_noah_pd_header_right
```

The left stack script built successfully but initially rejected an obsolete
manifest edge: `commit_step_candidate` no longer inlines
`noah_profile_store_prepare_commit_step`. Linked disassembly proves both direct
edges through that function. The manifest now includes its 104-byte frame;
no budget or check was relaxed. Re-evaluating the same fresh ELF/map passed:

```sh
python3 tests/host/firmware_stack_budget_tool_test.py
python3 tools/check_firmware_stack_budget.py \
  --manifest tools/firmware_stack_budget_live_profile_owner.json \
  --elf ../bastardkb-qmk/.build/bastardkb_charybdis_4x6_noah_live_mutation_stack_left.elf \
  --map ../bastardkb-qmk/.build/bastardkb_charybdis_4x6_noah_live_mutation_stack_left.map \
  --nm arm-none-eabi-nm --objdump arm-none-eabi-objdump
```

The named host commit path is 1,064 bytes; largest named main-process path is
1,264/1,920 bytes and split-slave path is 280/768 bytes. These are reviewed-path
policy checks on the left engineering artifact, not global or runtime maxima.
Memory gates passed on each fresh target using
`sh tests/host/run_firmware_memory_budget_checks.sh`, with
`NOAH_MEMORY_BUDGET_TARGET` set to each side-specific name where appropriate.
SRAM0–3 `.data + .bss` is 55,776 bytes on that left target and 55,796 bytes on
the ordinary right target, against the unchanged 57,344-byte regression policy.
Neither includes the planned additional 2,048-byte wear-level cache.

No hardware was flashed or live data mutated. Sibling QMK changes are generated
build artifacts only. Larger-geometry builds, hardware high-water/cadence and
physical migration/recovery checks remain pending; this is not deployment
approval for the new geometry. No introspection input changed. The all-profile
wrapper has only the generic target already built, so it was not repeated.

### Integrated implementation verification — 2026-09-21

The eight-slot implementation is complete in the worktree. It includes the
Pointing modes editor, eight RGB rows, configured directional and scroll
engines, six migrated factory presets, two empty slots, held/toggled activation,
per-slot DPI and pointer policy, modifier/button ownership, shared drafts and
field-by-field review, schema-2 persistence, split/VIA generation binding,
complete backup/restore and the old-geometry source-readback migration bridge.

Verification actually run and passed:

```sh
sh tests/host/run_pd_mode_key_runtime_integration_tests.sh
sh tests/host/run_profile_activation_policy_tests.sh
sh tests/host/run_pd_profile_integration_tests.sh
sh tests/host/run_pd_mode_handlers_tests.sh
sh tests/host/run_profile_owner_tests.sh
sh tests/host/run_feature_gate_compile_tests.sh
sh tests/host/run_profile_compiled_defaults_v1_tests.sh
sh tests/host/run_all_host_tests.sh
# From tools/charybdis-live:
npm run check
npm run keycodes -- --check
# From repository root:
python3 tools/profile_introspect.py --write
python3 tools/profile_introspect.py --check
qmk compile -kb bastardkb/charybdis/4x6 -km noah
sh tests/host/run_live_profile_owner_stack_budget_checks.sh
sh tools/build-firmware-pair.sh --pd-snapshot-bridge
sh tools/build-firmware-pair.sh
sh tests/host/run_firmware_memory_budget_checks.sh
git diff --check
```

The final full host suite exited successfully. The live-app check passes 365
tests. Configured key-runtime coverage now includes slot 8 hold/toggle and
scan-owned DPI synchronization, disabled-slot activation preserving an existing
lock, overlapping holds and Pinch modifier replay. Activation-policy tests
cover all eight active/locked bits, late button releases and saturating intent
counts. The cache/validator runner checks bounded reads, every read-failure
position and corrupted records, including sanitizers and the ARM state budget.

A local headless Chrome harness exercised the actual generated editor: eight
forms, tab switching with unfinished values, restored scroll control visibility,
and Keep mode producing a valid shared draft without browser exceptions. Both
the overview and expanded controls were captured and inspected. Durable DOM
and session tests cover the same behavior and read-only controls. The connected
browser service was unavailable, so this used an isolated temporary profile;
that browser process was stopped. No server or user browser session was changed.

The stack manifest now follows PD cache initialization, committed EEPROM reads
and record validation. The final instrumented left build passes: the largest
reviewed main-process path is 1,264/1,920 bytes; the largest reviewed split-slave
path is 280/768 bytes. These are named-path checks, not global stack maxima.
Normal right and left pair builds each measure 58,708 bytes of `.data + .bss`
and a 203,432-byte SRAM0–3 boot core-memory span per half. The instrumented left
artifact measures 58,696 bytes and 203,440 bytes respectively. Memory gates were
run for the generic build, bridge, normal right/left and instrumented owner
(`NOAH_MEMORY_BUDGET_TARGET=bastardkb_charybdis_4x6_noah_live_mutation_stack_left`).
See [the explicit PD resource policy](memory-budgets.md): the former 57,344-byte
tripwire failed and was deliberately revised only for this geometry/cache
feature to 60,416 bytes. Owner/validator/provider state policies remain unchanged.

The bridge and new pairs are in `../builds/feat/pd_modes/`:

- `1_charybdis_right_pd_snapshot_bridge.uf2`
- `1_charybdis_left_pd_snapshot_bridge.uf2`
- `1_charybdis_right.uf2`
- `1_charybdis_left.uf2`

Builds wrote generated files to the sibling QMK checkout and the artifact
directory; no sibling source was edited. The single-target all-profile compile
wrapper was not repeated because its sole `noah` target was compiled directly.
The generic stack wrapper was not repeated; the expanded owner stack gate
covers this feature's publication and storage paths. No board was flashed, no
live configuration was written, and physical power-loss, migration, cadence,
allocator/stack high-water and reconnect/role-swap acceptance remain unperformed.
Next: follow the README's verified original/migrated backup procedure, then
execute that physical acceptance matrix with the matching firmware pair.

### Hardware startup regression — 2026-09-21

The user completed bridge export and flashed the first final pair, then reported
pointer freezes and repeated white startup lighting before importing anything.
USB was connected to the right half only. A read-only HID probe observed the
schema-2 ABI, an unavailable PD runtime, and a USB disconnect/reconnect. That
supports a boot reset loop; the current diagnostic interface does not retain
reset cause, so a watchdog reset is inferred rather than directly measured.

The compiled-backed cache warmup was missing from the previous host integration
coverage. Its 39 bounded reads each replayed the entire preceding RGB and
behavior payload, including cubic behavior canonicalization, synchronously in
one scan. A new test against the real authored profile reproduced 1,443 behavior
row sorts during that callback. This is a plausible starvation path for the
750 ms scan watchdog, even though stack and byte-read budgets passed.

The reader now seeks directly to the final fixed-size PD payload for PD-only
reads, using the same payload encoder and record validation as full export.
The regression test requires zero behavior-row sorts during real cache warmup,
checks all eight cached records and every PD read boundary against the full
serialization, and runs with ASan/UBSan. The golden bytes, CRC/FNV, action ABI,
profile schema and EEPROM geometry are unchanged. Existing converted backups
remain valid. The watchdog timeout remains unchanged.

Targeted compiled-default, PD integration and feature-gate runners passed.
The new regression assertion was first observed failing on the old read path,
then passing with the direct PD read path.

Additional verification completed successfully:

```sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
sh tests/host/run_live_profile_owner_stack_budget_checks.sh
NOAH_MEMORY_BUDGET_TARGET=bastardkb_charybdis_4x6_noah_live_mutation_stack_left sh tests/host/run_firmware_memory_budget_checks.sh
sh tools/build-firmware-pair.sh
sh tests/host/run_firmware_memory_budget_checks.sh
git diff --check
```

The revised compiled-cache stack path is 776 bytes; largest reviewed main and
split-slave paths remain 1,264/1,920 and 280/768 bytes. These are reviewed-path
policies, not physical stack high-water measurements. Fresh normal left linked
accounting remains 58,708 bytes of SRAM0–3 `.data + .bss`, with a 203,432-byte
boot core-memory span; the instrumented left remains 58,696 / 203,440 bytes.
The span is not a measurement of unused runtime memory. Both memory gates pass
without changing any resource policy.

Replacement artifacts in `../builds/feat/pd_modes/` (409,088 bytes each):

| Artifact | SHA-256 |
| --- | --- |
| `2_charybdis_right.uf2` | `2ede543f4d2a0c15e3b819901c2c3541391849ff6c6b0c0730c2610410d8d1ec` |
| `2_charybdis_left.uf2` | `f37d9dc8ba04b7cf4c93458a7581232940d1f56cc81afd8573c8f088fd4e9d15` |

Builds wrote generated output into sibling QMK/builds directories; no sibling
source was edited. No board has been flashed by the agent and no live profile
write has been performed. The bridge was not rebuilt because this correction
is in the schema-2 PD read path; legacy/bridge serializer tests still passed.
Next: flash both replacement halves and confirm stable startup before importing
the existing converted backup. Hardware confirmation of the reset fix, and the
remaining migration/interruption/cadence/high-water acceptance, are pending.

### Complete-profile import rejection — 2026-09-21

The user confirmed firmware pair 2 has stable startup, then encountered
`DEVICE_REJECTED / VALIDATION_REJECTED` applying the converted backup. Read-only
status showed a ready compiled runtime, both peers known/converged and no
committed candidate; the app had already aborted the rejected transaction.

Local validation of the user's unchanged 2,428-byte profile reproduced failure
at byte 1,304, the first byte of the settings body. Its settings-v2 domain and
body headers were correct. Firmware's incremental settings consumer still
hardcoded body version 1 despite admitting the settings-v2 domain header.
The consumer now uses `NOAH_SETTINGS_VERSION`, matching both the selected schema
and firmware's own settings serializer. This is a firmware compatibility bug;
no backup conversion or profile editing is required.

Regression coverage now includes settings-v2 validation/publication, rejection
of a legacy body header, rejection of each nonzero retired DPI field, and a
complete app-generated PD profile passing through the real compiled firmware
compatibility and incremental validator. Both new tests were observed failing
before the fix. After the fix, both the reusable fixture and the user's exact
backup passed, normally and under ASan/UBSan. The private backup is not committed
as a test fixture; the runner can additionally check a local binary with
`NOAH_TEST_PD_IMPORT=/path/to/profile.bin`.

Targeted checks passed:

```sh
sh tests/host/run_profile_settings_v1_tests.sh
NOAH_TEST_PD_IMPORT=/tmp/pd-rejected-profile.bin sh tests/host/run_profile_compiled_defaults_v1_tests.sh
```

Candidate admission also retained the old 4,064-byte ceiling while schema 2
advertised 5,088 bytes. It did not cause this rejection because the user's
profile is 2,428 bytes, but it would reject otherwise-valid larger profiles.
Admission and chunk bounds now use the schema-selected payload ceiling, with
tests at 4,064, 4,065, 5,088 and 5,089 bytes.

Final verification completed successfully:

```sh
sh tests/host/run_pd_profile_integration_tests.sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
sh tests/host/run_live_profile_owner_stack_budget_checks.sh
NOAH_MEMORY_BUDGET_TARGET=bastardkb_charybdis_4x6_noah_live_mutation_stack_left sh tests/host/run_firmware_memory_budget_checks.sh
sh tools/build-firmware-pair.sh
sh tests/host/run_firmware_memory_budget_checks.sh
git diff --check
```

The stack and memory measurements are unchanged from pair 2 and both policies
pass. Corrected artifacts in `../builds/feat/pd_modes/` are 409,088 bytes each:

| Artifact | SHA-256 |
| --- | --- |
| `3_charybdis_right.uf2` | `8e4f3664c9bafab043551a62b615e34fc8172264e8bde4be06483818fb20e5d9` |
| `3_charybdis_left.uf2` | `15c3c46cd93c0c05cc83dba3fbe4252f773d7ce08263b4bc4e01bb75381b929f` |

Builds wrote generated output into sibling QMK/builds directories; no sibling
source was edited. No firmware flash or live profile write was performed by the
agent. Next: flash both halves with pair 3 and retry the unchanged converted
backup; hardware Apply/readback remains pending.

### Pointing-mode editor flow — 2026-09-21

The editor now reuses the existing keycode picker for all four directional
actions and mouse-button shortcut overrides. Picker modifier chords are
canonicalized before entering the draft, so selecting Cmd + Z stores the same
`G(KC_Z)` expression accepted by the device editor. Numeric modified-key
readback is reconstructed as an editable QMK expression, and the picker can
reopen that wrapper with the correct modifier selected.

The normal slot flow is name, movement type, DPI, then directional actions or
scroll reversal/modifiers. A single collapsed Advanced section contains
pointer-layer policy, active-axis thresholds, per-action modifier handling,
scroll gesture ratios/timing and mouse-button overrides. This only changes the
presentation: every existing field remains mounted, restores with unfinished
draft state, and serializes into the same PD domain.
