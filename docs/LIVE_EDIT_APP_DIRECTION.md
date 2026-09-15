# Live Edit App — Direction

Founding document for the `feat/live-edit-app` branch. It replaces the review
scaffolding as the thing that carries the direction.

The end goal is unchanged. It lives in
[`tooling/PROFILE_STUDIO_PRODUCT_GOAL.md`](tooling/PROFILE_STUDIO_PRODUCT_GOAL.md)
(the product contract) and
[`architecture/device-resident-profile.md`](architecture/device-resident-profile.md)
(the technical authority). This document records how we get there and what we
decided along the way.

## The Direction In One Sentence

The keyboard becomes the source of truth, and the live app becomes a client of
the keyboard rather than a client of the repository.

## Current Product Status

The live app reads configuration from the keyboard without a firmware workspace
at runtime. Layout, behaviours, combos and RGB have working editors and device
save paths. Complete export/import includes both macro banks and global settings;
eight-layer naming and reference-preserving reordering are implemented. The user
reports that the new workflow appears to work on their keyboard. This is useful
manual feedback, not completion of the hardware acceptance matrix.

| Product surface | Current state |
| --- | --- |
| Layout and eight layers | Read/write; names and overlay order travel with complete profiles |
| Key behaviours, combos and RGB | Read/write editors; custom-profile saves verify readback and both halves |
| Macros | Studio's existing builder, recorder and preview wired to both device banks; verified save and draft retention |
| Global policy | Defaults panels cover all 28 portable scalars, including startup layers, combo matching and device-reported lighting/key options; unsupported firmware features stay read-only |
| Backup and restore | Complete supported snapshots, review, recovery file and verified restore; interrupted restores can be retried |
| Drafts and Apply | Eight-layer profiles share one draft, semantic change review, undo/redo and a coordinated verified Apply; unfinished forms stay local until kept |
| Recovery and release readiness | Atomic logical Apply, differential transfer, reboot recovery fencing and verified recovery-base reuse are implemented; physical interruption acceptance, external VIA adoption, guided recovery, broad hardware acceptance and standalone packaging remain pending |

Studio's existing Defaults controls now use complete-profile readback and the
verified restore path, including native options reported by firmware. Editing,
undo/redo and change review now share a complete-profile draft. Apply binds VIA
and custom storage to one durable logical generation and blocks activation while
recovery is incomplete. Finish physical decision-boundary interruption tests,
external VIA adoption, guided recovery, the inherited pointing cadence work,
and blank-firmware restore acceptance before calling the product complete.

## Why This Branch Exists

Profile Studio was being asked to be two products at once: a `.c` authoring
tool driven by parsing the repository, and a live device editor driven by what
is committed on the keyboard. Those have different sources of truth, different
data models, and different users. Serving both from one 14,539-line extension
made each worse.

So we split them:

- **Profile Studio** returns to what it was at `refactor/aug` and freezes
  there. It authors `keymap.c`, `config.h`, and `rgb_config.c`. It is the
  `.c` tool.
- **The live app** is new, owns live firmware editing, and never parses the
  repository.

This is not a change of direction. The product contract already said the VS
Code extension "is the development shell for that product, not its final
authority model" and required "a reusable application core that does not depend
on parsing an open repository". This branch does that work first instead of
last, because doing it later means doing every intervening feature twice.

## What Already Exists

The separation was closer than it looked:

- `live-link/` is 6,433 lines across 14 modules with 3,666 lines of tests. It
  has **zero `vscode` imports**, and its only external dependency is `node-hid`,
  isolated behind a four-method adapter seam (`listDevices` / `connect`, then
  `write` / `onReport` / `onDisconnect` / `close`). It does not exist on
  `refactor/aug` at all.
- The live-edit contamination of `extension.js` is **675 insertions and 20
  deletions** against `refactor/aug`. Reverting it is surgical.
- The firmware live-profile stack — Profile Wire, the dual-slot profile store,
  the runtime owner, the split reconciler — is roughly 15,000 lines across 96
  files and is exactly what the live app talks to over HID. None of it rolls
  back.

## Decisions

### D-L01 — The branch keeps the firmware and rolls back only the studio shell

Branched from `refactor/live_edit`. Only `extension.js`, `package.json`, and
`README.md` in `tools/charybdis-profile-studio/` revert to their
`refactor/aug` state. Reverting the whole directory would delete `live-link/`,
which does not exist on that branch.

### D-L02 — The live app is a second VS Code extension for now

It ships as its own extension with its own activation, sharing nothing with
Profile Studio at runtime. Because `live-link` has no `vscode` imports,
repackaging later as a standalone desktop app is a shell and adapter swap
rather than a rewrite.

This defers the product goal's "no firmware workspace" requirement. The trigger
to repackage is the first time a non-developer needs to run it.

### D-L03 — `live-link/` moves into the live app as sole owner

After the rollback, Profile Studio has no live features and no use for it. A
shared package with one consumer is premature. Moved with `git mv` so history
follows. Extract to a shared package only if Studio ever needs device access
again.

It landed as `live-link/` and was layered into `core/` by D-L10 below; the
directory name in this decision is historical.

### D-L04 — Profile Studio is frozen at `refactor/aug`

Bug fixes only. It is not a development target. This is what makes forking
presentation code between the two apps cheap: nobody fixes the same bug twice
in a tool nobody is changing. Retirement stays open and does not need deciding.

### D-L05 — The keycode catalog is vendored, not parsed

A build step reads QMK's `*.hjson` keycode files once and emits a checked-in
JSON catalog inside the live app, stamped with the QMK version it came from.

This is required, not cosmetic. Today `compileViaLayout` needs
`qmkKeycodeValues`, which Profile Studio builds by parsing the QMK checkout,
and `decodeViaGetKeycodeResponse` returns a bare `uint16` with no mapping back
to a name. The live app needs both directions and cannot depend on a firmware
workspace for either. Vendoring also turns QMK version drift into a diffable
file rather than silent behaviour change.

### D-L06 — Port the whole UI, rebuild only the model source

**Superseded in execution, and the correction matters.** This decision
originally said to fork Studio's presentation — layout geometry, key faces,
colour controls — and rebuild the rest. That was based on a wrong reading of
where the reusable seam sits.

Studio's webview does no file access at all. It is a pure renderer: the host
posts a `model` object, the webview renders it, and edits come back as typed
messages (`updateLayoutKeys`, `saveBehavior`, `addCombo`). The webview never
knew the model came from parsed C. So the seam is the model, not the widgets,
and the whole UI ports unchanged as long as something produces the same shape.

Trying to lift "the presentation" out of 534 functions in one template literal
is impossible; the model boundary one level up is free. The live app therefore
takes Studio's UI verbatim into `webview/` and supplies the model from the
keyboard via `core/session/device-model.js`.

The cost is real and accepted: a large template literal now lives in an app
otherwise organised into small modules. It is exempted from the layer rules by
path, still tested for the two properties that matter — it cannot import
device layers and cannot touch the filesystem — and gets decomposed tab by tab
as each is rewired.

The original text follows, kept because the reasoning it got wrong is worth
remembering.

### D-L06 (original) — Fork the presentation, rebuild the state

The live app takes the expensive-to-rebuild visual work: the Charybdis layout
geometry, key-face rendering, colour controls, layer tabs, and the CSS. It does
not take the state layer.

Profile Studio's model is C expression strings (`canonicalLayoutKeyExpression`,
`authoredInternalKeyExpression`, `isLayoutKeyCallExpression`) resolved against a
repo-parsed catalog. The live app's model is structured values read from the
device. Carrying the state layer across would import an authority model built
for the wrong source of truth.

The live app's webview is written as real modules loaded through
`webview.asWebviewUri()`, not as a template literal. No bundler required.

### D-L07 — Specs are promoted, process is deleted

Kept, as durable specs under `docs/architecture/`:

- `profile-wire-v1.md` — the HID protocol the live app speaks
- `profile-split-v1.md` — the split protocol
- `authority-state-table.md` — source, device, and split authority; safe
  activation
- `storage-and-resource-baseline.md` — EEPROM maps, storage ceilings, and the measured resource baseline
- `field-classification.md` — the field inventory the product goal cites
- `pointing-cadence-known-issue.md` — the parked R-21 investigation

Deleted: all 23 review folders, `Sol Findings/`, `prompts/`, and the review
process conventions in `AGENTS.md`. Roughly 15,000 lines of process history,
none of which describes how the thing works.

### D-L08 — The live-profile owner is on by default

The gate inverts: `NOAH_LIVE_PROFILE_OWNER=no` builds an owner-free image. The
live app targets ordinary firmware rather than a special engineering artifact.

One coupling surfaced while implementing this and is worth stating, because it
changes what "ordinary firmware" means. The owner requires a provisioned
`NOAH_PHYSICAL_HALF`, since durable profile origin identity is side-specific.
The generic half-less convenience build therefore cannot carry the owner. It
now reports that at configure time and builds without it, rather than failing.

So the firmware you flash — the side-specific left/right pair — has the owner
by default, and the single generic image does not. If that split is wrong, the
alternative is to make the generic build an error, which would break the plain
`qmk compile` documented in the root README.

`tools/build-firmware-pair.sh`, and the `Build Firmware Pair (flashable)` task
that runs it, exist because of this. A build path that sets only
`FORCE_MASTER`/`FORCE_SLAVE` silently omits the owner, which is easy to miss:
the firmware works, it just has no committed profile to read.

The opt-out is kept deliberately. It is the only remaining lever for comparing
ordinary against live behaviour on identical source, which matters for R-21
(below), and it is the fallback if the regression proves intolerable in daily
use.

Static RAM for the current owner build is 55,796 B against the 57,344 B
regression tripwire, so this needs no memory policy change.

### D-L11 — Current state comes from the keyboard, including compiled defaults

The app has zero reliance on the repository's C files. It parses none of them,
reads no file at runtime, and `tests/layering.test.js` fails the build if a
`core/` module so much as names one.

A keyboard with nothing committed is still running something: the defaults its
firmware was built with. Profile Wire value `0x05` serves those over the same
page layout as the committed payload, so the app can show what the board
actually does instead of an empty editor.

That is not a source dependency. The bytes come from the device over HID. If
the flashed firmware was built from different authored data than the repo
currently holds, the app shows what is flashed, because that is what the
keyboard reports. The compiled defaults are labelled as such in the UI and
never presented as a committed generation; `generation 0` is reported as "no
committed profile" rather than as a generation.

Two pieces of data are shipped with the app rather than read from the device,
because no device command exposes them:

- the vendored QMK keycode catalog, since keycodes arrive as bare `uint16`
  with no names;
- the Charybdis layout matrix, since standard VIA has no query for which
  physical position maps to which row and column.

Both are keyboard-definition data of the kind VIA and Vial ship per board. No
configuration flows from them; they only decode what the device sends.

Readback must be tested through the extension's posted model and the rendered
controls, not just the byte decoders. The initial readback implementation omitted
the profile from the extension publication and forwarded wire-domain objects
without adapting them to the UI. Passing codec tests did not prove display.
`device-profile-view.js` now converts validated device domains into presentation
fields. The separate behaviours view lists every returned row; RGB includes
stage state and resolved group membership. Zero timing values remain visible,
with the unreported firmware-default duration stated explicitly.

Semantic target links to opaque VIA keycodes are enabled only for the known v1
native action ABI advertised by the keyboard. An unfamiliar ABI keeps semantic
rows inspectable without guessing native key identities. Source names and
unreported policy values are never reconstructed from the repository. Recorded
device bytes used by regression tests are test-only fixtures, not runtime data.

Layer preview membership follows the firmware rule: both transparent and
no-action keycodes are unmapped, regardless of their QMK alias spelling. The
preview honours all-keys mode and disabled layer stages as well. Tests exercise
the delivered key renderer, including unsaved edits whose numeric readback is
stale. Standard modified keycodes decode structurally; complete numeric IDs
remain for unnamed custom keys. Catalog generation honours QMK fragment resets
and deletions, and every decoded uint16 must encode back to its original value.

### D-L12 — RGB rule identity and preview appearance are separate

Pass-through is a layer paint operation, not white, black, or a copy of a
compiled default colour. The app reads current QMK RGB Matrix settings over
standard VIA channel 3, GET `0x08`, values 1–4 (relative brightness, effect,
speed, hue/saturation). These settings are outside custom-profile generation
identity and remain separate in the session and presentation models.

The read requires consecutive matching samples, up to four samples (16 GETs).
This detects ordinary changes during readback; VIA offers no atomic snapshot.
Unsupported, malformed, or unstable reads clear the previous base colour and
leave custom-profile readback intact. Disconnect clears both.

The board is explicitly a **selected-layer preview**. It assumes base plus
the selected layer, paints their colours in layer order, then their applicable
LED groups in reported order. All-keys/mapped-only membership, pass-through,
owning-layer group inheritance and disabled layer stages follow the firmware
rules. RGB Matrix off suppresses the full preview. Solid colour mode can be
drawn from the read settings; unimplemented effects use labelled placeholders.
Transparent-key identity is independent of inherited RGB fill.

This is not rendered LED telemetry. VIA brightness is scaled to an unreported
compiled ceiling; display intensity is approximate. Effect flags, idle/suspend,
animation phase, active/locked layers and transient feedback are not supplied
by these reads. Refresh is explicit through **Read from keyboard**. Exact live
appearance remains a future device-frame readout, not host reconstruction of
unreported state. No firmware or profile-format change is required for this
settings-based preview.

### D-L13 — Combo readback is device data, separate from persisted profiles

Profile Wire GET value `0x06` exposes the native combo table the connected
half runs, with effective per-combo timing and hold/tap/order rules, global
enabled state, and layer-reference mapping. The bounded read supports up to
32 rows of four inputs. A metadata digest and a repeated metadata read detect
ordinary mid-read changes. Exact framing is in `architecture/profile-wire-v1.md`.
This is an optional probe: old firmware returns VIA unhandled, and the app
shows an update message without losing its other readback.

This read does not introduce the reserved combo profile domain `0x30`, a
generation, an EEPROM representation or a mutation route. Definitions are
native numeric keycodes under the connected firmware's ABI. The app uses its
vendored keycode vocabulary and already-validated semantic labels, never source
files. Combo names are stable generated row labels. Firmware callback outputs
and additional custom trigger/release hooks are explicitly identified as opaque.

The initial readout listed every returned row in a Combos tab; D-L14 returns editing to the layout. Layout badges show where the selected
layer over Layer 0 supplies all inputs, respecting reported reference layers
and transparent keys; they do not claim to observe the active layer stack or
evaluate arbitrary trigger predicates. No per-combo transient engine state is
presented as configuration. Global disable suppresses preview badges.

Readback was proven on the connected keyboard. D-L14 adds the combo domain and generation-bound persistence.

### D-L09 — The live app owns a canonical profile format, not `.c`

Backup, restore, sharing, and version control all go through a canonical
profile format. `.c` import and export stay entirely in Profile Studio.

**This amends the product contract.** `PROFILE_STUDIO_PRODUCT_GOAL.md` lists
the C files as "an explicit import source" and "a canonical export target" of
the control software. On this branch, that bridge lives in the authoring tool
instead, and the live app's durable representation is the profile itself.
Honouring it as written would drag C parsing, the repo dependency, and the
`.c` serialiser back into the app the split exists to keep clean.

## Known Issue Carried Onto This Branch

**R-21 — pointing cadence regression, unexplained.** Reported mouse rate falls
from ~450 Hz to ~300 Hz with the live-profile owner enabled, roughly 1.1 ms of
added work per main-loop iteration. A full static investigation eliminated the
pointing path, the poll throttle, split pointing, the split reconciler (by
measurement), EEPROM read cost, macro reseeding, the split mirror step, and the
layer key-LED map rebuild. Nothing found accounts for a millisecond.

With D-L08 this ships by default, so it is now felt in daily use rather than
only in an engineering build. Details, the full candidate inventory, and the
remaining measurement paths are in
[`architecture/pointing-cadence-known-issue.md`](architecture/pointing-cadence-known-issue.md).

### D-L10 — The live app is layered, and the layering is enforced by review

`core/` is split into `transport`, `schema`, `protocol`, `session`, and `data`,
with imports pointing one way and `tests/` mirroring it. `media/` never imports
`core/`; it renders snapshots the host posts, so the core stays runnable in a
plain Node process and the UI stays replaceable.

The structure exists because the thing being replaced was a 14,539-line file
that grew one convenience at a time. The rules, and where new work belongs, are
in [`tools/charybdis-live/AGENTS.md`](../tools/charybdis-live/AGENTS.md).

D-L15 adds `core/model/` for the canonical portable document and layer-reference
rewrites. Generation-bound behaviour drafts exist; the whole-profile draft
coordinator remains pending.

## Delivery

Slices, each ending in something testable on the real keyboard. Protocol or UI
infrastructure without a usable connected workflow is progress, but it is not a
completed slice.

1. **Restructure and scaffolding.** Branch, revert the studio, move
   `live-link`, create the live extension, vendor the keycode catalog, promote
   the specs, delete the process, invert the firmware gate. Proven by
   connecting and displaying device identity, capabilities, schema, and status
   through the Profile Wire reads that already work.
2. **Device readback.** *Landed.* Profile Wire value `0x04` serves the
   committed payload: page 0 metadata, pages 1..N raw bytes. Coherence is the
   host's, by re-reading metadata after the chunks, since generation only
   increases. The payload is verified against both the reported CRC and digest
   before decoding, and a domain that fails to decode is reported without
   discarding the rest of the profile. RGB and key behaviours now reach the UI
   as the keyboard holds them.

   `READ_SURFACE` remains capability and status reporting and is still not
   this.
3. **Generation-bound drafts and conflict-safe apply**, then read-after-write
   verification and visible two-half convergence.
4. **Backup, restore, reset, and recovery journeys** over the canonical profile
   format.
5. **Domain expansion** per the field classification.
6. **Repackaging** so normal configuration needs no firmware workspace.
7. **Acceptance**: compatibility, migration, performance, resource, and
   real-hardware matrices before promotion. R-21 must be resolved here.

## Remaining Load-Bearing Contracts

- **The canonical profile format is defined.** D-L15 and
  [portable-profile-v1.md](architecture/portable-profile-v1.md) specify the
  complete supported backup. Future schema migrations and the remaining
  hardware acceptance must preserve that artifact.
- **One logical generation across two stores.** Standard VIA owns dynamic
  layout and macros; the custom store owns RGB, behaviours, and policy. One
  manifest must bind them, and a partial cross-store write must be refused
  rather than reported as a complete commit. The contract needs freezing before
  readback returns a generation identity.
- **External VIA writes.** Another VIA client can change layout underneath the
  app. Those changes are adopted into a new generation or surfaced as a
  conflict; they must not silently escape profile identity.

### D-L14 — Save device edits as a complete, generation-bound profile

RGB edits now patch the verified device payload and retain every untouched
profile domain. A save checks the original generation/digest/origin before
staging and again after acquiring the candidate lease. Conflicts abort before
chunks. The existing split commit barrier persists and activates the candidate
on both halves; the app then reads the entire committed payload and requires
byte equality. Combo edits additionally bind the native readout digest and
verify that the running combo table matches the saved domain.

Combos use optional canonical domain `0x30` v1 (specified in Profile Wire).
Absent means compiled fallback; an explicit empty table means no combos. Both
halves validate actions, references, duplicate native inputs and the shared hold
threshold before persistence. QMK introspection, combo origin tracking and GET
`0x06` all use the same effective native table. Publication is blocked by the
existing strict idle boundary. At publication the combo invalidator copies at
most 32 rows / 896 bytes once into owner-held native records; ordinary typing
and readback perform zero profile-reader calls. This bounded cold copy is a
specific exception to the metadata-only invalidation used by the other domains.
A failed copy exposes no partial table and makes native readback unavailable.
The validator state policy moves from 352 to 356 bytes for its optional native
translation callback; the owner and provider state policies remain unchanged.

QMK's hold/tap wait is global. All rows must carry the same threshold, and the
UI edits it as one shared setting. The local compatibility header supplies the
QMK hook configuration; no upstream source is changed. Custom trigger/release
hooks, callback outputs and disabled combo timing are not editable through this
format. Their readout remains available.

Combo editing stays beside the physical layout, including input selection on
the board. A collapsed all-combos list below the layer overview covers rows
unreachable on the selected layer; there is no separate combo navigation tab.
RGB stage toggles, colours, locality, fade policy, reusable LED membership and
assignment creation/removal use the same save path. Auto-mouse's follow-real-
destination mode deliberately ignores the end colour; its control is disabled
and explained in that mode. The static preview does not animate timeout fades.

Next: macro read/write, full backup/recovery and the remaining
policy domains. This is not acceptance of the complete product; the known
pointing-cadence and hardware acceptance work still apply.

Behaviour editing now uses that same generation-bound save path. The app patches
only domain `0x20`, preserving other domains and untouched rows. Editor labels
resolve through the shipped vocabulary and stable semantic action kinds;
existing target/action identities survive equivalent aliases. Sparse tap
branches, all four hold modes, repeat rates, zero/default timing and the anchor
flag round-trip through the existing v1 codec. Firmware-advertised row, step
and action-reference limits are checked before upload. New rows, replacements
and deletion are available from the layout and the Behaviours view. No firmware
format or runtime change is needed. Changed-setting persistence across reboot
and role changes remains part of the hardware acceptance matrix.

The behaviour editor keeps an ephemeral draft per device and row, pinned to
the source/generation/digest/origin first edited. A failed save or model update
does not silently discard or rebase it. The host checks the UI's expected base
before encoding and retains the existing device checks around lease acquisition.
Stale drafts are visible and save-disabled until explicitly discarded. This
is bounded row editing, not the still-pending whole-profile draft coordinator.

Connected-device validation added an unused, action-free `KC_F24` row at
generation 11 and deleted it at generation 12. Both saves converged on both
halves; the restored 1,293-byte payload matched the original generation 10
payload exactly. Browser checks separately cover row switching, failed-save
draft retention, stale-draft blocking and explicit discard. Physical execution
of changed actions and reboot/role-swap acceptance are still outstanding.

### D-L15 — A portable profile is the complete effective configuration

Export/import owns the whole keyboard snapshot. Flashed and committed domains
are materialized into the same document from device reads. The file contains
all matrix positions, all 64 VIA macros, all 16 user macro instruction streams,
RGB, behaviours, effective combos, global settings and layer names. Missing
and explicitly empty domains have different runtime meanings; complete files
must carry all four domains and cannot fall back to destination authored data.
The contract is in [portable-profile-v1.md](architecture/portable-profile-v1.md).

The standard image reserves eight layers. Base stays at index zero; the app
moves overlays and rewrites references together. The action ABI describes the
engine vocabulary, independently of authored behaviour rows. Empty-profile and
populated-profile builds must advertise the same ABI.

A five-layer deployment needs a one-time snapshot bridge before changing its
storage geometry. `tools/build-firmware-pair.sh --snapshot-bridge` keeps the
five-layer addresses and deployed profile identity while exposing full readback.
Export there, then install the regular eight-layer pair and import. Import
expands the unused layers transparently and translates the three-position shift
in user trigger IDs. No firmware is flashed automatically by the app.

Import validates and reviews before writing, saves a local recovery document,
and checks the reviewed state again after acquiring the candidate lease. The
custom profile commits first; VIA macro/layout writes follow with macro
invalidation during transfer. Success requires exact whole-profile readback and
both storage owners' split convergence. This remains a recoverable sequence
across two durable owners, not one atomic transaction. Interrupted work retains
its recovery file and reports incomplete restoration.

Next: finish hardware acceptance of bridge/export/update/import, power loss and
USB role changes. D-L16 wires the macro editor; global-policy controls and the
inherited pointing-cadence regression remain work toward product acceptance.

D-L15 verification: the app has 278 passing tests, including exact macro-write
invalidation/retry, incomplete-restore recovery, legacy migration, layer-name
retention and bridge-first guidance. The host suite passes, and the firmware
validator accepts an app-generated complete populated payload when compiled
with zero authored behaviours and combos. Both eight-layer and bridge pairs
build. Both reviewed stack manifests and the memory gate pass; fresh accounting
is in [memory-budgets.md](architecture/memory-budgets.md). A read-only check still
found the connected board on five-layer firmware before handoff. The user has
since reported that the new workflow appears to work and requested the commit.
The individual migration, restore and persistence steps have not been recorded
as a completed hardware matrix. A subsequent read-only probe found the HID
interface but could not open it, so it did not establish a new layer count or
generation. Next is recording bridge/export/update/import acceptance, then
reboot, power-loss and USB-role checks.

### D-L16 — Wire Studio's macro UI to the existing complete-profile writer

The slot browser, payload editor, browser-event recorder, step builder and
preview remain the existing Studio interface. The device model now supplies
all 64 VIA and 16 user slots, including empty slots, from coherent complete
readback. Both banks use the same editor; their existing key identities stay
stable. The layout picker resolves those identities through the advertised
action vocabulary, including slots beyond the vendored catalog's named macro
range; the existing layout hover cards and overview show their device payloads.
No source parser or new firmware command is involved.

The host translates the existing text/command syntax into each bank's native
bytes, validates supported keys, balanced holds, delays and storage capacity,
and patches only the selected slot in the reviewed snapshot. Doubled braces
represent literal braces in text. Apply uses the existing complete-profile
restore path with recovery-file creation, stale-base checks, both-half
convergence and exact readback. It therefore inherits the recoverable, non-atomic
cross-store contract; a narrower macro-only writer is not introduced here.

Drafts retain their original complete-profile fingerprint. Failed saves keep
their contents; external changes block stale writes. A verified save advances
other drafts from that exact base because the submitted edit changed only the
selected macro. A per-slot Discard action restores the last readback. Editing
requires the eight-layer complete-profile image; the five-layer bridge remains
available for readback and export.

Verification includes schema and model round trips, bank capacity and stale
draft rejection, the real extension Apply-message/recovery/acknowledgement
route, and browser checks of Cmd+N insertion, recording, clearing user macros,
immediate discard, failed-save retention and preserving other drafts after a
successful save. Browser writes use a simulated keyboard; physical macro
execution remains part of hardware acceptance. Next: wire the existing Defaults
controls to global settings and finish the recorded hardware matrix.

The app has 293 passing tests for this checkpoint, and the full host suite
passes. No firmware source or protocol contract changed in this wiring pass.


### D-L17 — Device-backed Defaults and reported brightness limits

The existing Defaults section cards now edit timing, combo enablement, normal
and sniping DPI, mode overrides, auto-sniping, auto-mouse activation and fade
timing, base HSV/speed/enablement/idle timeout, and feedback flash timing. The
model supplies actual settings from the complete keyboard snapshot, including
zeros, rather than build-time ladder parameters or authored defaults. Behaviour
timing placeholders use the same readback. Auto-mouse timeout and fade delay
share a section so the required ordering can be validated together.

Each section patches only its scalar fields in the existing settings domain.
Layer names, both macro banks and all other domains remain byte-for-byte intact.
The existing complete-profile restore path owns recovery, stale-base checks,
both-half convergence and exact readback. There is no new settings write command
and the recoverable, non-atomic two-owner contract is unchanged.

Section drafts retain their original full-profile fingerprint and survive view
changes, failed writes and re-reads. Discard is per section. A verified section
save advances other settings and macro drafts from that exact base; a verified
macro save likewise advances settings drafts. External changes still block
stale saves. This bounded coordination is not the unified whole-profile draft
and Apply model.

Brightness exposed a device constraint missing from readback: QMK clamps the
saved value to its compiled maximum. GET value 0x08 page 1 now reports that limit
from firmware. The app never infers it from source files or by writing temporary
lighting values. Older firmware returns the existing unsupported-page status;
the app leaves brightness read-only while keeping the other controls usable.
New firmware enables a bounded brightness control. Complete-profile restores
also reject brightness above a reported destination limit before staging.
The wire addition is documented in portable-profile-v1.md; no profile document
or settings payload version changes.

The current RGB effect and LED flags, boot-layer mask, native keymap options and
combo reference-layer map remain preserved and import/export-capable. Their
dedicated controls are still pending; they must not be advertised as editable
in Defaults. Build-time LED cadence and pointer ladder definitions are not
portable settings and are not fabricated as device controls.

Verification covers section round trips, unchanged-domain preservation, packed
lighting channels, bounds and cross-field validation, stale drafts, recovery
handoff, the extension acknowledgement route, and optional limits decoding.
Browser checks with a simulated keyboard confirmed loaded fields and failed-save
draft retention; they also caught the inherited C-identifier layer validator,
which now accepts the keyboard's reported layer names. Remaining browser
interactions were blocked by automatic approval review's usage limit. Actual
hardware operation, persistence and interruption acceptance remain pending.
Next: finish browser/hardware acceptance and the remaining native settings UI,
then unified draft/review/Apply and guided recovery.

D-L17 verification: `npm --prefix tools/charybdis-live run check` passes all
306 app tests; `sh tests/host/run_all_host_tests.sh` and
`sh tests/host/run_feature_gate_compile_tests.sh` pass. The required
`qmk compile -kb bastardkb/charybdis/4x6 -km noah` passes, and
`sh tools/build-firmware-pair.sh` builds both owner-enabled halves, including
the optional limit page. The generated pair is build 9 in the branch's builds
folder. No keyboard was flashed or configuration written during this pass.

### D-L18 — Native settings use device-reported capabilities

Defaults now adds startup-layer toggles, per-layer combo matching, keyboard-wide
key options, named lighting effects and LED-class selection. Advanced sections
start collapsed and retain their open state after a save. Startup layers require
at least one selection and are validated after the whole section is patched,
so changing from one startup layer to another works in one save. Packed RGB
bytes, combo nibbles and unknown key-option bits survive unrelated edits.

Optional GET 0x08 page 2 and bounded pages 3 onward report QMK's actual enabled
effect inventory, supported semantic key options, their native bit masks and
the LED classes present on this keyboard. The app decodes this metadata over
HID and never reads firmware sources. Page 1 brightness metadata remains
compatible. Older firmware's canonical unsupported-page reply leaves dependent
controls read-only; malformed metadata is an error. Complete-profile restore
checks advertised effect availability before staging, alongside brightness.
The profile format and the recoverable, non-atomic two-owner save contract are
unchanged. Native effect IDs still require compatible firmware; the metadata
does not introduce cross-build effect-name migration.

QMK ignores mode and HSV changes while RGB is disabled. The compat adapter now
temporarily enables lighting without saving, applies settings synchronously at
the existing safe activation boundary, then restores and persists the requested
on/off state. No renderer runs between these steps. Host tests model QMK's
disabled setters and cover all four initial/final on/off combinations, actual
QMK option masks, enabled-effect inventory, paging and C-to-app decoding.

Browser checks with a simulated keyboard cover supported and disabled key
options, startup validation, retained section expansion, an unrelated timing
draft surviving a verified startup-layer save, combo matching, and saved effect,
LED selection and HSV while lighting is off. A two-window check caught the
shared refresh handler clearing Defaults drafts; refresh now retains them and
blocks stale saves after another editor changes the profile. A delivered-script
regression test covers this reset boundary. Physical LED behaviour, reboot
persistence and interruption acceptance still require testing on the keyboard.
Next: unified whole-profile draft/review/Apply, guided recovery and the remaining
hardware acceptance and pointing-cadence work.

D-L18 verification: `npm --prefix tools/charybdis-live run check` passes 316
app tests. `sh tests/host/run_qmk_portable_editor_tests.sh`,
`sh tests/host/run_feature_gate_compile_tests.sh` and the complete
`sh tests/host/run_all_host_tests.sh` pass. The required generic
`qmk compile -kb bastardkb/charybdis/4x6 -km noah` and
`sh tools/build-firmware-pair.sh` pass; normal flashable outputs are build 10.
An instrumented left build with `NOAH_STACK_BUDGET_ENABLE=yes` passes
`python3 tools/check_firmware_stack_budget.py` with both the baseline and
live-profile-owner manifests. `sh tests/host/run_firmware_memory_budget_checks.sh`
passes for both the instrumented and final normal left image. The normal left
image remains at 55,684 bytes of static `.data + .bss`, below the 57,344-byte
regression tripwire, with a 206,456-byte SRAM0–3 boot core-memory span. These are
per-half linked measurements, not runtime high-water evidence. No firmware was
flashed or physical keyboard configuration written; sibling folders received
generated QMK build files and the numbered pair only.


### D-L19 — Shared whole-profile draft and review

A complete eight-layer device snapshot seeds one window-local profile draft.
Layout, behaviours, combos, RGB, both macro banks and Defaults use their existing
validated editors to update that draft. Import and layer naming/reordering also
replace the draft, preserving the same undo history. Keep does not write HID.
Review lists semantic before/after values by domain, and Apply requires the exact
reviewed draft revision and matching connected device. The header continues to
report the actual device generation; draft revisions are never device generations.

Unfinished forms survive keeping another section and re-reading the device.
Undo/redo and review require those forms to be kept or discarded first. An
external fingerprint change preserves the target and blocks a stale Apply.
Review against keyboard explicitly compares that target against a fresh read;
it does not silently merge external edits. Interrupted restore retains the target
and requires another read/review before retrying. An incomplete read is labelled
as recovery with no full semantic comparison, never as a complete backup.

Apply uses the existing complete-profile restore with a recovery copy before
writing and verified readback afterward. This does not change the wire or storage
contracts and does not make VIA/custom-owner writes atomic. Export continues to
capture the saved board, not unapplied window-local changes. Older firmware keeps
its supported direct-save paths. Closing the editor still loses local drafts.

Verification includes composed edits across all six editor areas, no-write staging,
revision/device binding, undo/redo, stale reads, interrupted apply retention, text-only
review rendering and browser interaction with the real extension and a simulated
keyboard. Browser checks cover unfinished Defaults/RGB preservation, refresh,
macro staging and combined review. Physical acceptance remains outstanding.

Next: guided recovery and persisted draft recovery, the complete logical-generation
contract, and physical reboot/interruption/blank-firmware acceptance.

### D-L20 — Differential Apply and the logical transaction boundary

The observed Apply delay comes primarily from redundant VIA transfers. The VIA
macro bank is 7,191 bytes and a standard 32-byte Raw HID report carries 28 data
bytes, so one complete macro read costs 257 request/response exchanges. The old
restore path captured the complete profile three times, read the VIA regions a
fourth time for verification, and rewrote the whole macro bank even when it was
unchanged. A small edit could therefore exceed 1,100 VIA exchanges before
candidate status polling and split work.

Apply now verifies and reuses the complete snapshot already loaded in the editor
as its recovery base, uses custom/VIA/settings identities for the preflight and
post-lease compare-and-swap checks, writes only changed 28-byte layout and macro
blocks, and reads those blocks back exactly. Stable final custom and VIA
identities still have to prove both-half convergence. An unchanged macro bank
sends no macro writes; a five-byte edit in one block sends one data write
bracketed by the two macro-validity writes. Refresh and Export continue to
perform an independent complete keyboard read.

Hardware timing on 2026-09-14 measured the remaining path before recovery-base
reuse. A layer-name-only Apply took 13.05 seconds and 1,402 Raw HID requests,
despite writing zero VIA bytes. The complete safety capture cost 2.96 seconds;
candidate upload and validation reached the commit barrier at 4.97 seconds; the
two-half custom-profile commit then took 8.06 seconds and accounted for 717
candidate-status reads. An exact restore repeated the same 13.05-second shape,
and an independent final read proved the original fingerprint at generation 10.
Recovery-base reuse reduced the same measured Apply to 10.14 seconds and 875
requests. A detailed state trace then isolated 7.2–7.4 seconds in
`PREPARING_PEER`, 0.23 seconds in local `COMMITTING`, and about 0.78 seconds in
peer convergence and activation.

The split delay was an unintended interaction between the scan-owned receiver
mailbox and generic failure backoff. Every mutating split RPC first returns
`BUSY` to acknowledge mailbox admission, then publishes its result from matrix
scan context. The sender treated that expected first response like a transport
failure and waited 50 ms for every 14-byte chunk. Expected admission now gets
one bounded 5 ms acknowledgement retry; a peer that remains busy still enters
the existing exponential 100–1000 ms backoff, and transport failures retain the
original 50–1000 ms path. This changes scheduling only, keeps one operation per
scan, and keeps the existing failure semantics.

Physical timing after flashing both halves confirmed the change. The same
layer-name-only Apply completed in 5.51 seconds and its exact restore in 4.92
seconds. `PREPARING_PEER` fell from 7.2–7.4 seconds to 1.9–2.1 seconds; local
commit remained about 0.2 seconds and convergence/activation about 0.7 seconds.
An independent final read matched the exact pre-flash content fingerprint
`3951067804:168945532:1805986991` at generation 3. This covers an uninterrupted
save and restore; power-loss acceptance at the shortened acknowledgement timing
still belongs with the logical-transaction interruption matrix.

This optimization did not by itself turn the two-owner sequence into an atomic
commit. The accepted firmware design was
[`architecture/logical-profile-transaction-v1.md`](architecture/logical-profile-transaction-v1.md):
stage target VIA bytes on the non-USB half, bind that identity into prepared
custom records on both halves, use the USB-side custom marker as the logical
decision, then roll the target VIA copy forward before activation. It avoids
doubling QMK's EEPROM RAM cache and defines old-or-new behavior for each power
loss boundary. D-L21 implements that contract; physical interruption acceptance
remains.


Hardware acceptance on 2026-09-12: the connected eight-layer keyboard reported
37 behaviours, seven combos and 12 populated macros. Two complete reads matched;
a same-half power cycle preserved the complete snapshot at generation 12.
Moving USB to the left exposed no Raw HID interface, consistent with the normal
pair's FORCE_SLAVE/usb_disconnect policy. USB role migration is not applicable
to that pair and remains untested with role-switching firmware.

Returning USB to the right initially produced a storage-recovery error. A
follow-up status read was ready with no conflicts, and the full snapshot still
matched the original. The cause of that temporary condition is not confirmed.
Using the actual ProfileDraftSession and complete-profile Apply over HID, a
single last-layer display-name change committed at generation 13; independent
readback matched the target and both halves converged. Restoring the original
snapshot committed at generation 14; independent readback exactly matched the
original configuration and both halves converged without errors. Recovery copies
and machine-readable evidence are held locally under backups/. This exercised
the live service path, not the actual VS Code UI. No firmware was flashed.
The connected firmware does not implement the optional settings-limit/options
queries, so capability-dependent controls remain untested on hardware.

Next hardware checks: reboot after this save/restore, representative runtime
behaviour changes, and later explicitly approved interruption/blank-firmware
recovery tests. Investigate transient reconnect readiness before presenting it
as a persistent recovery requirement.

Post-save/restore reboot on 2026-09-12: custom-profile generation 14 and its
original digest remained converged. The first complete capture failed again.
A diagnostic at 16:49:44 UTC observed VIA storage flags 7 (dirty, recovery
required, digest valid), generation 9 on both sides, local digest 2839131957,
peer digest 1961381780, and no acknowledgement. At 16:50:04 UTC it reported
flags 4, matching digest 1961381780 and acknowledgement 9, with no errors or
conflicts. The subsequent complete profile exactly matched the original backup.
No host configuration writes occurred during these checks. This proves eventual
recovery to the expected configuration; it does not prove clean startup or
independent durable retention on each half. Clarify which halves lost power,
then investigate the dirty/recovery transition before marking reboot acceptance
fully passed. Do not suppress the readiness error without establishing its cause.

Power-setup correction and separate test, 2026-09-12: the user clarified that
only one half lost power during the preceding recovery observation; the other
remained powered. It must not be treated as a simultaneous cold boot. The user
then disconnected power from both halves and reconnected USB only to the right.
The first diagnostic at 16:53:16 UTC reported clean VIA flags 4, generation 9,
matching digests 1961381780, acknowledgement 9, and no errors/conflicts. The
complete read at 16:53:30 UTC matched the original snapshot exactly; custom
profile generation 14 remained converged. This full-power-cycle persistence
check passed at observation, without host configuration writes. The earlier
one-half power-cycle recovery transition remains an open issue; the passing
cold-boot check does not explain or dismiss it. Next: representative runtime
edit tests and investigation of the one-half reconnect path.

### D-L21 — Apply publishes one atomic logical generation

Complete Apply now treats the custom Profile Wire record and standard VIA
layout/macro store as one logical generation. The host acquires the candidate
lease, binds the candidate to the next VIA generation and canonical digest, and
sends only changed VIA ranges to the non-USB half. Firmware verifies that staged
copy before it makes durable custom intent. Both custom slots reach a prepared
marker before the USB-side marker becomes the decision record. The peer custom
record commits next, its VIA copy is accepted, and ordinary split reconciliation
rolls that exact VIA identity back to the USB half. Runtime activation waits for
both custom and VIA convergence.

Storage format 2 keeps the existing 32-byte header and 4,064-byte payload. A
distinct `NQ` header packs domain/origin/flags, stores the VIA binding, and still
retains the compiled-default and action-ABI digests needed after firmware
updates. Its custom payload digest is derived again during validation. Format-1
`NP` records remain readable and migrate on the next complete Apply.

Boot begins with VIA reconciliation fenced. A committed logical record supplies
the required VIA generation and digest; firmware either adopts a matching local
staged bank or asks the peer to accept and supply it. The effective profile does
not activate until that recovery converges. A prepared marker without the USB
decision is ignored by boot selection, so the previous committed generation
remains authority.

The app requires the new atomic capability for complete Apply. It keeps the
recovery file, stale-base comparison, differential range selection and final
exact readback. Deterministic predecision failures request both VIA-stage and
custom-candidate aborts. Refresh and Export remain independent full reads.

Host coverage exercises both header formats, incompatible compiled/action
identities, durable prepare and reboot selection, abort and decision ordering,
role recovery, bound-profile propagation, staged VIA fencing/acceptance and
postdecision boot recovery. Remaining acceptance is physical power interruption
at each durable boundary, the one-half reconnect case, external VIA-writer
adoption, guided recovery, and blank-firmware restore.

D-L21 verification: `sh tests/host/run_all_host_tests.sh` passes, including 340
live-app tests. The side-specific `sh tools/build-firmware-pair.sh` build passes
with the owner enabled on both halves. The linked left-half owner is exactly
4,096 bytes, within its unchanged 4,096-byte engineering policy. The normal
left image passes `sh tests/host/run_firmware_memory_budget_checks.sh` at 55,796
bytes of SRAM0–3 `.data + .bss`, 1,548 bytes below the 57,344-byte regression
tripwire, with a 206,344-byte linker/core-memory span at boot. The fresh
instrumented owner build passes the reviewed-path stack manifest; its largest
named main-process path is 1,824/1,920 bytes and its largest named split-slave
path is 328/768 bytes. These are per-half linked results and reviewed-path
estimates, not runtime high-water measurements. The build wrote generated QMK
artifacts and numbered firmware pairs in sibling output directories; no sibling
source was edited.
