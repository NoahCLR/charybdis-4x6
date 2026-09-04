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

### D-L06 — Fork the presentation, rebuild the state

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

The opt-out is kept deliberately. It is the only remaining lever for comparing
ordinary against live behaviour on identical source, which matters for R-21
(below), and it is the fallback if the regression proves intolerable in daily
use.

Static RAM for the owner build is 51,820 B against the 57,344 B regression
tripwire, so this needs no memory policy change.

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

## Delivery

Slices, each ending in something testable on the real keyboard. Protocol or UI
infrastructure without a usable connected workflow is progress, but it is not a
completed slice.

1. **Restructure and scaffolding.** Branch, revert the studio, move
   `live-link`, create the live extension, vendor the keycode catalog, promote
   the specs, delete the process, invert the firmware gate. Proven by
   connecting and displaying device identity, capabilities, schema, and status
   through the Profile Wire reads that already work.
2. **Device readback.** Bounded, chunked, generation-correlated read of the
   exact committed payload. This is the critical path: device-first open,
   generation-bound drafts, stale-write refusal, read-after-write verification,
   backup, restore, and recovery are all downstream of it. Today's
   `READ_SURFACE` bit is capability and status reporting only and must not be
   presented as profile readback.
3. **Generation-bound drafts and conflict-safe apply**, then read-after-write
   verification and visible two-half convergence.
4. **Backup, restore, reset, and recovery journeys** over the canonical profile
   format.
5. **Domain expansion** per the field classification.
6. **Repackaging** so normal configuration needs no firmware workspace.
7. **Acceptance**: compatibility, migration, performance, resource, and
   real-hardware matrices before promotion. R-21 must be resolved here.

## Undesigned And Load-Bearing

- **The canonical profile format.** D-L09 made it the durable artifact users
  keep. Slice 2 will force its shape whether or not it has been decided
  deliberately.
- **One logical generation across two stores.** Standard VIA owns dynamic
  layout and macros; the custom store owns RGB, behaviours, and policy. One
  manifest must bind them, and a partial cross-store write must be refused
  rather than reported as a complete commit. The contract needs freezing before
  readback returns a generation identity.
- **External VIA writes.** Another VIA client can change layout underneath the
  app. Those changes are adopted into a new generation or surfaced as a
  conflict; they must not silently escape profile identity.
