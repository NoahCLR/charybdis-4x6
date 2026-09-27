# Live Edit App — Direction

The current status of the live app and the decisions behind it. The end goal is
the [product goal](PRODUCT_GOAL.md); the technical authority is
[`architecture/device-resident-profile.md`](architecture/device-resident-profile.md).
This document records how we get there and what we decided along the way.
Verification logs, build numbers and the branch's setup history are in git, not
here.

## The Direction In One Sentence

The keyboard becomes the source of truth, and the live app becomes a client of
the keyboard rather than a client of the repository.

## Current Product Status

The live app, [`tools/charybdis-live/`](../tools/charybdis-live/), reads
everything it edits from the keyboard without a firmware workspace, keeps every
change in one reviewed draft, and applies it to both halves as one atomic
logical generation. The user reports that the workflow works on their keyboard;
that is useful manual feedback, not completion of the hardware acceptance
matrix.

| Product surface | Current state |
| --- | --- |
| Layout and eight layers | Read/write; names and overlay order travel with complete profiles; a reorder renumbers layer keys by default ("Keys follow their layers") |
| Key behaviours, combos and RGB | Read/write editors over the shared draft; matching Keys reach sections share open state across tabs, open independently, and use the page scrollbar |
| Macros | 64 named VIA macro slots with builder, recorder and preview; shared-memory and per-macro limits shown and enforced (D-L25, D-L26) |
| Mouse | Pointer and sniping DPI, auto-sniping and auto-mouse: global-policy sections the core files under the Mouse area, so the rail, the review and import counts all place them there. The auto-mouse fade delay is a share of the timeout, edited on its lighting stage (D-L17) |
| Pointing modes | Eight device-owned slots and eight RGB rows; see [PD-mode domain v1](architecture/pd-mode-domain-v1.md) |
| Global policy | Every other portable setting, including startup layers, combo matching and device-reported lighting and key options; unsupported firmware features stay read-only |
| Backup and restore | Complete snapshots, import review against the keyboard, recovery file and verified restore |
| Drafts and Apply | One draft with item-by-item review, discard by edit group, Show, undo/redo and draft history; the review checks reachable actions, confirms active warnings and traps, and blocks profiles the destination cannot save (D-L36); Apply shows its steps and says where a failure happened (D-L19, D-L23, D-L29, D-L30) |
| Recovery | Atomic logical Apply, differential transfer, reboot recovery fencing, firmware roll-forward after the decision, resume after a lost or power-cycled peer link, bounded cancel owned by the keyboard (D-L20–D-L22, D-L27, D-L39) |

The rail's health strip shows connection, both-half convergence, draft state
and recovery state. Convergence needs the firmware's peer-known and
peer-converged flags, not just matching generations. With several compatible
keyboards connected, a selector picks one; a dirty draft stays bound to its
keyboard, and a reconnect requires an explicit review before Apply.

Remaining before calling the product complete:

- physical interruption acceptance at every durable boundary;
- adoption or conflict reporting for writes by external VIA clients;
- guided recovery, and an explicit reset to compiled defaults;
- broad hardware acceptance, including blank-firmware restore and the
  [PD-mode hardware matrix](architecture/pd-mode-domain-v1.md#hardware-acceptance).
  USB role migration is untested: on the normal pair the left half exposes no
  Raw HID interface (`FORCE_SLAVE`/`usb_disconnect`), so it needs role-switching
  firmware;
- standalone packaging (D-L02);
- the open issues below.

## Open Issues

- **One-half power-cycle recovery transition.** On 2026-09-12, after one half
  lost power while the other stayed powered, the first complete read failed
  with VIA storage flags 7 (dirty, recovery required) before settling to clean
  flags about 20 seconds later with the exact original profile. A full
  two-half power cycle was clean. The dirty transition is unexplained; do not
  suppress the readiness error without establishing its cause.
- **Why a peer stops acknowledging a push** (D-L22) and **why a peer flash
  write failed mid-copy** (D-L27) are both unknown. Both are now bounded and
  reported; the status fields D-L27 added should identify the cause next time.
- **`LT()` row tap/hold timing** (D-L34): the runtime times a press from when
  QMK delivers it, so an authored `LT()` row's tap/hold term likely starts only
  after QMK's own tapping term (the dual-role setting, D-L38). Not yet measured.
- **The picker's list of unbuilt QMK features is written by hand** (D-L05).
  `webview/view/picker-sections.mjs` hides keycodes for features this build
  does not include, matched against its rules files and QMK's defaults. It
  drifts when a feature is enabled or disabled. The keyboard should report its
  built features so the picker reads them from the device.

## The Tools

- **Charybdis Live** (`tools/charybdis-live/`) is the app and the only one
  developed (D-L35). Nothing in it reads the firmware repository.
- **Profile Studio** (`tools/charybdis-profile-studio/`) authors `keymap.c`,
  `config.h` and `rgb_config.c`. It is frozen (D-L04).
- The first live app (v1, which lived at the same path) ported Studio's interface
  (D-L06). It was frozen by D-L35 and then removed; the profiles it wrote remain
  a firmware compatibility check in
  `tests/fixtures/stored_profile_live_v1.fixture`.

## Remaining Load-Bearing Contracts

- **The canonical profile format.** D-L15 and
  [portable-profile-v1.md](architecture/portable-profile-v1.md) specify the
  complete supported backup. Future schema migrations and hardware acceptance
  must preserve that artifact.
- **One logical generation across two stores.** Standard VIA owns dynamic
  layout and macros; the custom store owns everything else. D-L21 binds them;
  a partial cross-store write must never be reported as a complete commit.
- **External VIA writes.** Another VIA client can change the layout underneath
  the app. Those changes must be adopted into a new generation or surfaced as a
  conflict; they must not silently escape profile identity. Not yet built.

## Decisions

Numbers are stable and cited from code and docs. D-L01 (branch from
`refactor/live_edit`, revert only Studio's shell) and D-L03 (move `live-link/`
into the live app, since layered into `core/` by D-L10) were branch setup and
are complete. D-L16 wired Studio's macro UI into v1 and went with it (D-L35).

### D-L02 — The live app is a VS Code extension for now

It ships as its own extension, sharing nothing with Profile Studio at runtime.
Its `core/` has no `vscode` imports, so repackaging as a standalone desktop app
is a shell and adapter swap rather than a rewrite. This defers the product
goal's "no firmware workspace" requirement; the trigger to repackage is the
first time a non-developer needs to run it.

### D-L04 — Profile Studio is frozen at `refactor/aug`

Bug fixes only; it is not a development target. That is what made forking
presentation code between the apps cheap: nobody fixes the same bug twice in a
tool nobody is changing. Retirement stays open and does not need deciding.

### D-L05 — The keycode catalog is vendored, not parsed

A build step (`npm run keycodes`) reads QMK's `*.hjson` keycode files once and
emits a checked-in JSON catalog inside the live app, stamped with the QMK
version it came from. Keycodes arrive from VIA as bare `uint16`, and the app
needs both directions without a firmware workspace. Vendoring also turns QMK
version drift into a diffable file rather than a silent behaviour change.

The same step also reads the layout extras' shifted US symbols (`S(KC_1)` is
`KC_EXLM`) and the keyboard's own keycodes from `enum charybdis_keycodes` in
`keyboards/bastardkb/charybdis/charybdis.h`. Those take their QK_KB slots under
the header's short name (`DRGSCRL`), with the long name and `QK_KB_n` kept as
aliases. The `QK_KB` range is fixed by QMK and the keyboard code, not by the
userspace action ABI, so these keys need no matching vocabulary. Their drag
scroll is the keyboard code's own, at a fixed `CHARYBDIS_DRAGSCROLL_DPI`, which
the Pointing modes settings do not reach. Drag scroll is the `DRAGSCROLL`
pointing mode, so the picker does not offer `DRGSCRL` or `DRG_TOG`; a key
already holding one still reads back as "Built-in drag scroll".

A name the catalog gives a key must be what the key does wherever it is
placed. A key or a combo output runs QMK's full key handling, so any keycode
works there. A behaviour sends QMK and keyboard functions (keycodes past the
layer keycodes and below the user range: the DPI and sniping keys, RGB Matrix,
Magic, `QK_BOOT`) as a synthetic QMK record, so they run as on a key, and says
so with Profile Wire feature bit 15. A key whose own keycode is one keeps a
plain key's fallback hold: with only a double-tap branch on `SNIPING`, holding
it still holds sniping, as with no behaviour at all. Older firmware sent them through
`tap_code16()`, which keeps only the low byte (`DPI_MOD` sent nothing), so
against a keyboard without bit 15 the app refuses them in a behaviour's target,
tap or hold (`behaviorEmitProblem` in `core/schema/actions.js`).

The picker offers only what does something on this keyboard. It leaves out
retired user macros, the Charybdis drag scroll and unnamed `QK_KB` slots, and
keycodes for QMK features the build does not include (MIDI, audio, RGBLight,
Caps Word, Repeat…); see Open Issues. A key that already holds one still reads
back by name.
VIA macro slots appear as `VIA_MACRO_0`–`VIA_MACRO_63` throughout the app.
Their QMK numeric identities still round-trip, but the picker does not offer
duplicate `QK_MACRO_n` names from the first 32 slots.

### D-L06 — v1 ported Studio's whole UI and rebuilt only the model source

*Superseded by D-L35.* Studio's webview does no file access: the host posts a
`model`, the webview renders it, and edits come back as typed messages. The
reusable seam was therefore the model, not the widgets, so v1 took Studio's UI
verbatim and supplied the model from the keyboard. The original plan, forking
the presentation and rebuilding the state, misread where that seam sits;
lifting "the presentation" out of 534 functions in one template literal was
impossible. v2 then replaced the ported UI with its own.

### D-L07 — Specs are promoted, process is deleted

Durable specs live under `docs/architecture/`: the Profile Wire and split
protocols, the authority state table, the storage and resource baseline, the
field classification and the domain contracts. The review folders, findings
registers, prompts and review-process conventions were deleted: process
history that did not describe how the thing works. A completed plan is folded
into the spec it produced and deleted.

### D-L08 — The live-profile owner is on by default

`NOAH_LIVE_PROFILE_OWNER=no` builds an owner-free image; the app targets
ordinary firmware, not an engineering artifact. The owner needs a provisioned
`NOAH_PHYSICAL_HALF`, since durable profile origin identity is side-specific,
so the generic half-less build reports that at configure time and builds
without the owner rather than failing (failing would break the plain
`qmk compile` in the README). The firmware you flash is the side-specific pair
from `tools/build-firmware-pair.sh`; a build that sets only
`FORCE_MASTER`/`FORCE_SLAVE` silently omits the owner. The opt-out stays as the
lever for comparing ordinary against live behaviour on identical source.

### D-L09 — The live app owns a canonical profile format, not `.c`

Backup, restore, sharing and version control go through the portable profile.
`.c` import and export stay in Profile Studio. This amends the product goal,
which listed the C files as an import source and export target of the control
software; honouring that would drag C parsing and the repository dependency
back into the app.

### D-L10 — The live app is layered, and the layering is enforced

`core/` is split into `transport`, `schema`, `protocol`, `model`, `session` and
`data`, with imports pointing one way and `tests/` mirroring it. The webview
never imports `core/`; it renders the model the host posts, so the core runs in
plain Node and the UI stays replaceable. The rules and where new work belongs
are in [`tools/charybdis-live/AGENTS.md`](../tools/charybdis-live/AGENTS.md).
The structure exists because the thing it replaced was a 14,539-line file that
grew one convenience at a time.

### D-L11 — Current state comes from the keyboard, including compiled defaults

The app parses no C file and reads no repository file at runtime;
`tests/layering.test.js` fails if a `core/` module names one. A keyboard with
nothing committed still runs its compiled defaults, which Profile Wire value
`0x05` serves over the same page layout as the committed payload. If the
flashed firmware was built from different data than the repository holds, the
app shows what is flashed. Compiled defaults are labelled as such; generation 0
reads as "no committed profile".

Two pieces of keyboard-definition data ship with the app because no device
command exposes them: the vendored keycode catalog and the Charybdis layout
matrix. Neither carries configuration; they only decode what the device sends.

Readback is tested through the posted model and rendered controls, not just
the byte decoders: passing codec tests once coexisted with a profile that never
reached the UI. Semantic links to native keycodes are enabled only for a known
action ABI advertised by the keyboard. Layer preview membership follows the
firmware: transparent and no-action keycodes are unmapped whatever their alias.
Every decoded `uint16` must encode back to its original value. Zero timing
values stay visible, with the unreported firmware default stated. Recorded
device bytes are test-only fixtures, never runtime data.

The Configure screens reveal the keyboard only after the complete portable
read has opened an editable draft. Layout, committed domains, VIA base RGB and
the complete profile arrive in separate reads and are published separately for
diagnostics, but their partial previews must stay behind one loading state.
Every screen menu is disabled during that read. Afterwards, Configure requires
an editable draft; Profile & backups requires complete-profile support; Device requires a connected
keyboard that reported its capabilities.

### D-L12 — RGB rule identity and preview appearance are separate

Pass-through is a layer paint operation, not a colour. The app reads QMK RGB
Matrix settings over standard VIA channel 3 (brightness, effect, speed,
hue/saturation), outside custom-profile generation identity, requiring
consecutive matching samples since VIA has no atomic snapshot. An unstable read
clears the base colour and leaves custom-profile readback intact.

The board is a **selected-layer preview**: base plus the selected layer, their
colours in layer order, then their LED groups in reported order, following the
firmware's membership, pass-through and inheritance rules. It is not LED
telemetry: brightness is scaled to a ceiling, and effects, animation phase,
active layers and transient feedback are not reported. Exact live appearance
would need a future device-frame readout, not host reconstruction.

### D-L13 — Combo readback is device data

Profile Wire GET `0x06` exposes the native combo table the connected half runs,
with effective per-combo timing and rules, global enable and layer-reference
mapping: up to 32 rows of four inputs, with a metadata digest to detect
mid-read changes. Readout version 2 also reports the default window, the hold
threshold and which combos follow the default. Old firmware returns VIA unhandled and the app shows an
update message without losing its other readback. Combo names are stable
generated labels; firmware callback outputs and custom trigger/release hooks are
shown as opaque. Board badges show where the selected layer over layer 0
supplies all inputs; they do not evaluate the active layer stack or arbitrary
trigger predicates.

### D-L14 — Device edits are saved as a complete, generation-bound profile

A save patches the verified device payload, keeps every untouched domain, and
checks the original generation, digest and origin before staging and again
after acquiring the candidate lease; the app then requires byte-equal
readback. Combos are optional canonical domain `0x30` v1: absent means compiled
fallback, an explicit empty table means no combos. Both halves validate
actions, references, duplicate inputs and the shared hold threshold before
persistence, and the app checks the firmware-advertised row, step and
action-reference limits before upload. At publication the combo invalidator copies at most 32 rows /
896 bytes once into owner-held native records, so typing and readback do no
profile reads. Combo timing follows QMK: every combo has its own window or
follows one default window (`COMBO_TERM`), and the hold/tap wait
(`COMBO_HOLD_TERM`) is one value for all combos. Domain `0x30` version 2
stores the default window and the hold threshold once, so both exist without
combos and are edited in Settings · Combos. A new combo starts on the default,
shown filled in; typing a window makes it the combo's own, and Use default
makes it follow again. Review compares a following combo as following, so a
changed default is one Combo timing change. Version 1 — every window explicit,
the threshold repeated on each row — is still read; a draft holding it takes
the keyboard's default on its first combo edit. Custom trigger/release hooks,
callback outputs and disabled combo timing are not editable through this
format.

The per-domain save paths this introduced were replaced by the shared draft
(D-L19) and atomic Apply (D-L21); the generation binding and readback rules
stand.

### D-L15 — A portable profile is the complete effective configuration

Export and import own the whole keyboard snapshot. Flashed and committed
domains are materialized into the same document from device reads: every
matrix position, the VIA macros and their names (D-L25), RGB, behaviours,
effective combos, pointing slots, global settings and layer names. Missing and
explicitly empty domains mean different things; a complete file carries every
domain and never falls back to destination authored data. The contract is in
[portable-profile-v1.md](architecture/portable-profile-v1.md).

The standard image reserves eight layers; base stays at index zero and the app
moves overlays and rewrites references together. The action ABI describes the
engine vocabulary independently of authored rows, so empty and populated
builds advertise the same ABI. The old five-layer snapshot bridge is retired
(D-L40). Old portable snapshots remain importable only when they carry the
source evidence required by the current schema. No firmware is flashed by the
app.

Before a file becomes the draft or is restored, its card compares it with what
the keyboard holds, since that is what applying it would write, counting the
differences by what they configure: Keys, Lighting, Macros and Pointing modes,
each with added, changed and removed totals. It names no single difference;
that is the draft's review. A file identical to the keyboard says so and cannot
be used. A file carries no layer order, so it is compared slot by slot. See
`importDifferences` in `core/session/panel-session.js` and `categorySummary`
in `webview/view/review.mjs`.

### D-L17 — The keyboard reports its brightness limit

QMK clamps saved brightness to a compiled maximum, which readback did not
expose. GET `0x08` page 1 now reports it. The app never infers it from source
or by writing temporary values; older firmware's unsupported-page reply leaves
brightness read-only. Restores reject brightness above the destination's
reported limit before staging. Build-time LED cadence and pointer ladder
definitions are not portable settings and are not fabricated as controls.
The auto-mouse fade delay (setting 16, the milliseconds the colour holds
before it fades) must stay shorter than the timeout, and both halves refuse a
profile where it is not. The app edits it on the Lighting auto-mouse stage as
a whole percentage of the timeout, at most 99%, and still stores milliseconds.
Saving a new timeout rescales the stored delay to its exact previous ratio in
the same staged edit, so no edit can break the ordering and the review lists
both. The ratio lives in the app, not the firmware: VIA does not edit these
settings, and a storage change would cost a schema version and a reflash.

### D-L18 — Native settings use device-reported capabilities

Optional GET `0x08` page 2 and pages 3 onward report QMK's enabled effect
inventory, the supported key options with their native bit masks, and the LED
classes present. The app decodes that over HID; older firmware leaves the
dependent controls read-only, and malformed metadata is an error. Restore
checks effect availability before staging. Packed RGB bytes, combo nibbles and
unknown key-option bits survive unrelated edits.

QMK ignores mode and HSV changes while RGB is disabled. The compat adapter
temporarily enables lighting without saving, applies settings at the safe
activation boundary, then restores and persists the requested on/off state, so
lighting edits save correctly while lighting is off.

### D-L19 — One whole-profile draft and review

A complete device snapshot seeds one window-local draft. Every editor, import
and layer edit updates it, with one undo history. Keep does not write HID.
Apply requires the exact reviewed draft revision and the matching connected
device; draft revisions are never device generations. Unfinished forms survive
keeping another section and re-reading the device; undo, redo and review
require them to be kept or discarded first. An external change preserves the
draft and blocks a stale Apply; **Review against keyboard** compares it
against a fresh read and never silently merges. An incomplete read is labelled
as recovery, never as a complete backup. Export captures the saved keyboard,
not unapplied changes. Closing the editor loses the draft.

### D-L20 — Differential Apply

The VIA macro bank is 7,191 bytes and a 32-byte Raw HID report carries 28 data
bytes, so one complete macro read is 257 exchanges; the old path read the
profile four times and rewrote the whole bank. Apply now verifies and reuses
the snapshot already loaded as its recovery base, uses custom, VIA and settings
identities for the compare-and-swap checks, writes only changed 28-byte blocks
and reads those back exactly. Refresh and Export remain independent complete
reads.

"Changed" means changed from the bank the keyboard holds, not from one rebuilt
from the document: a valid bank may keep stale bytes after its 64th macro,
which a document cannot carry. A capture keeps the exact layout and macro
bytes it read, and an Apply result the target it proved; a cached snapshot
without them is read again. See
[Portable Profile V1](architecture/portable-profile-v1.md#portable-document).

Most of the remaining delay was split scheduling: every mutating split RPC
first returns `BUSY` to acknowledge mailbox admission, and the sender treated
that like a failure and waited 50 ms per 14-byte chunk. Expected admission now
gets one bounded 5 ms retry; a peer that stays busy still enters the
100–1000 ms backoff and transport failures keep the 50–1000 ms path. On
hardware this took a layer-name-only Apply from 13.05 s to 5.51 s.

### D-L21 — Apply publishes one atomic logical generation

Complete Apply treats the custom profile record and the standard VIA
layout/macro store as one logical generation, per
[logical-profile-transaction-v1.md](architecture/logical-profile-transaction-v1.md).
The host binds the candidate to the next VIA generation and digest and sends
only changed VIA ranges to the non-USB half, which verifies the staged copy
before durable custom intent. Both custom slots reach a prepared marker before
the USB-side marker becomes the decision record. The peer commits and accepts
its VIA copy, then the host writes the changed ranges to the USB half as soon
as the decision and peer accept are visible; the peer stays the complete
recovery copy until the USB-side VIA identity is verified. Runtime activation
waits for both custom and VIA convergence. A prepared marker without the USB
decision is ignored at boot, so the previous generation remains authority;
boot starts with VIA reconciliation fenced and recovers a decided target from
the local stage or the peer.

Storage format 2 keeps the 32-byte header and 4,064-byte payload; a distinct
`NQ` header stores the VIA binding and still carries the compiled-default and
action-ABI digests. Format-1 `NP` records stay readable and migrate on the next
Apply. Schema 2 (PD slots) uses format 3 `NR`; see
[PD-mode domain v1](architecture/pd-mode-domain-v1.md). The app requires the
atomic capability for complete Apply and keeps the recovery file, stale-base
check and exact final readback. A failure the app sees before the decision
requests the custom-candidate abort, which the keyboard carries out for both
stores (D-L39); once the decision is made the keyboard refuses it, even if the
host never saw the decision, and finishes the generation. On hardware an unchanged Apply takes about
6.6 s, spent in keyboard-side validation and durable publication.

### D-L22 — A cancelled save ends in bounded time, and says why it ended

A peer that stopped acknowledging a push and then answered the cancel's split
`ABORT` with `BUSY` once left an Apply stuck in `PREPARING_PEER` until
unplugged, because that `ABORT` was retried without limit. Firmware now bounds
it (15 s); past it the USB half releases its own side, reports *peer cleanup
pending* as status flag bit 8, retries the `ABORT` in idle slots and starts no
new save or split transfer until the peer acknowledges. See
[the authority state table](architecture/authority-state-table.md#cancelled-prepare-peer)
and [Profile Wire V1](architecture/profile-wire-v1.md).

Apply reports `RESTORE_NOT_SAVED` (nothing was saved, the keyboard kept its
profile) when the failure came before the commit was sent or the keyboard
confirmed the cancel by returning to idle, which it refuses to do once a marker
exists. An unconfirmed cancel after the
commit was sent stays `RESTORE_INCOMPLETE` with its recovery file. While cleanup
is pending, the rail says *Restart the keyboard* and the message says to unplug
the USB cable, not the cable between the halves.

### D-L23 — Apply shows its steps, and a failure says where, why and what was saved

`core/session/apply-progress.js` names the ten steps of an Apply and tracks
them forward only: check the keyboard, save a recovery copy, send the profile,
keyboard checks it, stage keys and macros on the other half, copy the profile
to the other half, save it on this half, finish the other half, write keys and
macros on this half, check both halves. `restoreProfile()` reports at each real
boundary with byte counts. A failure keeps its step, a reason from the
keyboard's own error (or the other half's last answer), and whether anything
was saved. The commit bar keeps a failed Apply on screen until dismissed.
After a successful Apply, the app reads layout, committed domains, combos and
VIA base lighting again. The editor and rail stay visible but busy; the commit
bar names the current read and its page progress until editing resumes. This
readback is separate from the ten transaction steps and does not claim another
two-half verification.

Candidate status page 1 (see
[Profile Wire V1](architecture/profile-wire-v1.md#candidate-operation-status))
reports the peer phase, transferred bytes and the peer's last split status. The
host counts its movement as progress while the keyboard is `PREPARING_PEER`, so
a slow copy no longer runs into the stall window. Firmware without page 1
answers `UNKNOWN_PAGE` and keeps the old behaviour.

### D-L24 — Directional modes can read eight directions

A directional mode can read the four straight directions plus the four
diagonals, each with its own shortcut, classified into 45-degree wedges; a
diagonal with no shortcut sends the nearer straight direction, both neighbours
or nothing, as the mode chooses. It is axis policy `3` in the unchanged 96-byte
PD record, using bytes a directional record otherwise leaves zero (see
[PD-mode domain v1](architecture/pd-mode-domain-v1.md)). Older firmware and
apps reject axis `3`, so the domain version stays `1`. The C/JS differential
corpus uses the v2 app's codec.

### D-L25 — VIA macros have names; user macros are retired

The 16 user macros are retired and the 64 VIA macros can be named. Names live
in the settings domain, in the space the user macros had, so the worst-case
profile does not grow; they are saved atomically with every Apply, copied to
the other half and carried in backups. The `MACRO_n` keycodes keep their
numbers (reserved, inert) so the action ABI digest and every later keycode are
unchanged; tapped as keycodes they would read as modified basic keys, so they
are consumed and do nothing. The user-macro runtime, `HARDCODED_MACROS` and
their introspection and memory-gate requirements are gone. Settings readback
streams from the effective settings cache instead of a second copy, saving
1,400 bytes of static RAM per half; see
[memory budgets](architecture/memory-budgets.md#retired-user-macros-and-streamed-settings-readback--2026-09-23)
and [portable profile](architecture/portable-profile-v1.md#version-3-via-macro-names-instead-of-user-macros).

### D-L26 — Macro slots share one visible memory, and every slot says what fits

All 64 slots share the keyboard's macro memory (7,191 bytes on the eight-layer
geometry; a key tap takes 3 bytes, a typed character 1). A macro plays only if
the firmware compiles it into at most 512 bytes (`MACRO_PAYLOAD_IR_MAX_BYTES`),
about 170 key taps; a longer one used to be accepted and then silently never
played. The app computes that size exactly (`macroProgramBytes`, checked
against the firmware decoder by `run_macro_program_size_tests.sh`), refuses an
edit past it, and marks a slot VIA wrote past it as too long.

Every empty slot keeps room for ten key taps (30 bytes); when free memory
cannot keep that for every empty slot, the highest-numbered empty slots show no
room and cannot be edited until space is freed. The firmware does not enforce the reserve, so the
app shows what a VIA edit left. Settings version 4 guarantees every macro name
20 printable ASCII characters; see
[portable profile](architecture/portable-profile-v1.md#version-4-every-macro-name-gets-20-characters).

### D-L27 — A stale copy on the other half can no longer hold off every later one

Apply stalled while copying to the other half, with the peer answering busy and
nothing copied until a power cycle. The faults, each now fixed and tested:

- The receiver timed its provisional lease from any frame on the link, so the
  sender's polls kept a stale lease alive. It is now timed by requests for its
  own copy only.
- A sender whose chunk met busy retried that chunk forever. It now restarts at
  `PREPARE_BEGIN`, which resumes a live lease and re-creates a dropped one.
- A busy reply carried the stale copy's offset, which could be unencodable; it
  now reports `0` for another copy.
- A copy rejected after a failed flash write did not release the peer store's
  `PEER` storage admission, so every later copy was refused. A rejected copy
  now always returns it.
- The trigger behind every stall: the store's shape check kept its own list of
  settings versions and refused every stored copy carrying v4 after both
  validators had passed it. There is now one list,
  `NOAH_PROFILE_PD_SETTINGS_VERSION_ACCEPTED` in `profile_versions.h`.

A failed store on the other half is now retried from the start up to two
times, safe because nothing is durable before the commit is authorized; then
the Apply ends with `PEER_TRANSFER_FAILED` and the app says the other half
could not store the profile. Busy replies say why and carry the receiver's
current store state and admission; page 1 reports the last reason and the busy
streak. See [profile split](architecture/profile-split-v1.md) and
[Profile Wire](architecture/profile-wire-v1.md).

### D-L28 — Dominant axis and eight directions are one directional engine

Eight-direction modes chose the wedge from whatever motion was still banked, so
a slightly off-diagonal move alternated diagonal and straight taps; dominant
axis chose its axis per report with no smoothing. All directional modes now run
one engine with four or eight directions: a smoothed heading, measured against
each axis's own threshold, picks a direction and holds it until the heading is
clearly elsewhere, and only progress along the held direction counts. The axis
policy only says which directions exist; motion toward a missing one goes to
the nearest existing one, which for a single axis is exactly counting that
axis. Counting in threshold X × threshold Y units keeps the single-axis
presets (Volume, Brightness) on their threshold counts. The dominant-axis
presets use the current smoothed-heading behavior.

"When a direction is empty" applies to every directional mode (byte 86 for
axes 0–3). "Send both neighbours" means the two compass neighbours, 45° either
side, the one the movement leans toward first; modes without diagonals have no
such neighbours, so there it acts as "its neighbours take over". See
[PD-mode domain v1](architecture/pd-mode-domain-v1.md).

### D-L29 — The review lists items, discarded in the groups their edits made

The review lists one item per thing that differs from the keyboard: a key on a
layer, a layer's name, a behaviour, a combo, a macro with its name, a settings
section, a pointing slot, or one lighting record (LED groups and the rows that
paint them are one record, since rows name groups by id). An item says whether
it was added, changed or removed and lists only the fields that differ, in the
editors' words. Fields compare what is stored, not what is shown, so a changed
default or a renamed layer is one item where it was made. Colour reads as in
the editors (D-L31). The draft's change count counts items. **Show** opens the
item where it is edited; a removed item has no Show.

Combo numbers are packed positions. When the only net combo difference is one
removed row, review and history show that removal as one item and explain that
later combos move up one number. Discard reinserts the row at its former
position. If another combo also changed, review keeps the positional differences
visible rather than attributing them all to the deletion.

An item is also the unit **Discard** puts back, as one more undoable step. The
items one staged edit changed belong together, so a key swap or a moved
behaviour is one block with one Discard, titled by the edit that made it; a
later edit touching two groups joins them. Every item is listed under its own
area in rail order; a group spanning areas shows its part in each, and each
part's Discard takes back the whole group. Rebases, discards and steps that
fell out of the bounded history link nothing. A discard from a current review
keeps it current; once nothing described is left, the draft is the keyboard's
profile again, including bytes no item describes. A recovery review is
discarded whole or not at all. See `core/model/profile-review.js`,
`core/model/profile-revert.js`, `ProfileDraftSession.changes()` and
`webview/view/review.mjs`.

**Layers are compared by identity, not by slot.** Beside every history entry
the draft keeps which keyboard layer each slot now holds, set by Rename & Reorder
from the order it saves and never inferred from names or contents, which two
empty layers or a swap that also swaps the names would fool. The review
compares the draft with the keyboard's profile rearranged into that order, with
every layer reference following. So a reorder is one **Layer priority** item,
one row per layer that moved; everything else is compared layer with layer.
With "Keys follow their layers" off, keys that kept their numbers now reach a
different layer and are listed as the changes they are. Discarding the order
moves the layers back and keeps every other change; undo and redo carry the
order with their entry; a rebase keeps it; an import, a discard of the whole
draft and an Apply start again from the keyboard's order. Group links are kept
by layer, not by slot, so a later reorder does not tie two layers' keys
together. See `core/model/layer-order.js`.

### D-L30 — The draft shows itself where it is edited

Every editor marks what the draft changed with the draft's amber dot: a key, a
layer chip, a tab, a behaviour or combo row, a slot, a colour row, a settings
section, and the header of a folded group holding a change. The marks come from
the same review items (`draftMarks` in `webview/view/review.mjs`), so the two
never disagree.

**Discard all** is one more draft step: undo brings every change back, and the
keyboard is not read again, except for a draft out of step with it, bound to
another keyboard, or a recovery. Undo and Redo name the step they take back or
bring back. **Draft history** opens every step newest first, each with when it
was made and what it changed from the step before (not from the keyboard, as
the review compares); going to a step is several undos or redos at once. The
host sends the steps only while the sheet is open. The commit bar has one way
on, **Review and apply**, and says what the draft holds ("4 added · 10 changed
· 1 removed").

### D-L31 — One mark per thing that has a colour

Each thing with a colour of its own has exactly one mark, drawn by
`webview/ui/marks.mjs` wherever the thing is named:

| Thing | Mark |
| --- | --- |
| A behaviour tier (tap, hold, long hold) | the dot the keyboard flashes when it resolves |
| A tap count (2× and up) | its branch badge, in its tap-branch colour |
| A layer | its layer colour |
| A pointing mode | the light its slot paints |
| A combo | its badge, in the combo feedback colour |
| A lighting stage | its on/off dot |

A thing is marked where it is the subject and where it is only referred to: a
setting names what it governs (`governs` in `core/model/settings-editor.js`),
so its mark appears in Settings, in the editors' fields and in the review
alike. A stage that is off draws its marks off. A colour is the keyboard's own,
so a dark one reads dark.

### D-L32 — Things sit in fixed places

A repeated element sits in the same place every time it appears, so a screen
reads as a grid rather than as text that wraps wherever it lands.
The Keys workbench tabs count what the selected layer stores. The Key tab counts
mapped keys, excluding transparent and disabled positions; its selected key's
layout index belongs in the key details. Every Keys tab is at least as tall as
the Behaviours tab; a taller editor carries its height across tab changes, so
changing tabs does not move the page's scroll position.
The Behaviours, Combos, Macros and Pointing modes reach lists lead with **On
this view**, calculated for the selected layer plus exactly the lower layers
currently previewed on. **On this layer** means a key stored on the selected
layer, and the other route groups describe ways it can reach an item under
different activations. The view can therefore overlap those groups; it is the
current activation, not another storage location. A combo in the view requires
all inputs in that activation and uses the configured Combo Layer Matching
reference when one is set. Tab counts still count the selected layer's stored
routes.

The reach sections have one open state by section identity across those four
tabs. Opening one does not close another. A section unique to one tab keeps its
own state; sections absent from another tab retain their state until shown
again. The Behaviours rail grows with its rows, so the Keys content pane is its
only vertical scroller.

- The review is one grid: a status gutter, the title with its area underneath
  when outside its section, the fields as sign · label · on the keyboard · in
  your draft, then Show and Discard at the edge. A field is marked as a diff
  marks a line: + added, − removed, nothing for changed. A side with nothing
  shows a dash in its own column.
- A destructive action is the last thing in its row.
- The draft's dot follows a row's label; on a tile it has one fixed spot.
- A removed thing keeps its mark and Show only where it is still on screen (a
  cleared pointing slot, an emptied macro slot).
- A table's owner reads by the name its dropdown offers, with its mark, never
  an enum.
- Where marked and unmarked labels share a column, every label gets the same
  mark slot (`marked(…, {slot: true})` in `webview/ui/marks.mjs`).

### D-L33 — One home for each rule the app needs twice

Knowledge kept in several places drifted: stored values had different words in
the review and the screens, the pointing-mode keycode registry was written six
times, a dirty draft was decoded 10–16 times per publish, and the host
sequenced Apply itself. Each rule now has one home:

- Words: `core/model/vocabulary.js`, sent as `model.vocabulary`; stages are
  found by id, never by label.
- Key names: `core/model/key-names.js`. The catalogue names what QMK ships;
  the profile names its macros, pointing-mode keys and bare user slots. The
  screens' `qmkKeyLabels` and the review are both built from it, each from
  its own snapshot, so a key reads the same in the combo table, the picker and
  the review. The webview never names a keycode: a form that shows keys, such
  as the combo builder, carries the host's label with each one. The stored name
  (`KC_KP_1`) is added only when a review row's two sides would read alike.
  A layer is always named by its name, in the picker's verbs: `MO(3)` reads
  "Hold Navigation", `LT(3,KC_SLASH)` "/ / Navigation", and a layer key's cap
  shows the layer's name. Only a raw keycode, where one is shown, keeps the
  number.
- Actions, native keycodes, decode limits, layer references:
  `core/schema/actions.js`; the pointing registry and key layout are data.
- Decoding: once per draft revision, carried as `decoded` and taken through
  `decodedOf`; history entries are frozen.
- Settings bits and base lighting: `fieldMask` and `baseLighting` in
  `core/model/settings-editor.js`.
- Panel sequencing: `core/session/panel-controls.js`, tested with a fake
  service; `extension.js` supplies dialogs, files and progress.
- Interface: `canEdit(area)` for permission, `view/reach-groups.mjs` and
  `ui/groups.mjs` for grouped lists, `slotLight` in `ui/marks.mjs`, one form
  per macro slot and one `state.combo` for the builder.

### D-L34 — Layer keycodes are owned in the firmware, and the app follows

QMK ran `TG()`/`TO()` behind the userspace's layer ownership, so a toggled
layer turned off when an `MO()` of it was released and `TO()` left stale locks.
The fix is in the firmware: `TG(n)` is `LOCK_LAYER(n)`, `TO(n)` is "lock only
n" (`layer_ownership_goto`), `TT(n)` is `MO(n)` with a built-in
`LOCK_LAYER(n)` on its `TAPPING_TOGGLE`-th tap, and `OSL(n)` holds like `MO(n)`
while its tap arms a one-shot owner the next qualifying press uses up. All act
the same on a plain key, in a behaviour and as a combo output. Profile Wire
feature bit 14 advertises this; the app accepts them in behaviours only when a
keyboard reports it: `OSL()` as a tap, `TT()` as a "Press and hold until
release" branch. A plain `LT()` hold (tap count 0) goes through layer
ownership; its tap still reaches QMK. `LM(n, mods)` holds layer n and its
modifiers through modifier ownership, as a key or combo, not a behaviour step.

The keyboard holds a candidate it is asked to save to where its actions are
placed, by the rules `keymap.c` validation uses; a committed record still loads,
and the halves still sync it. The app refuses the same placements first, in the
behaviour and combo editors and before every upload, naming the misplaced
action. `DF()` and `PDF()` stay refused: layer 0 is the base in the
firmware's lookup, the RGB base effect and the app, and `TO()` already covers
the need. Decided behaviour: `TT()`'s last tap locks on release; `OSL()`
follows QMK (a lone long press arms it, a second tap within `TAPPING_TERM`
cancels it); `TO()` keeps held layers on; `ONESHOT_TIMEOUT` and `ONESHOT_TAP_TOGGLE` apply to `OSM()` only;
`TT()` counts taps within `CUSTOM_MULTI_TAP_TERM` like every multi-tap key; a
`TG()`/`TO()` of the pointer layer from a behaviour is not seen by QMK's
auto-mouse. A 44-step hardware check passed on both halves on 2026-09-24; the
`LT()` row timing question stays open (see Open Issues). D-L38 makes the dual-role setting drive QMK's own tapping term.

### D-L35 — Charybdis Live v2 is the app

*Numbered D-L21 when written, alongside the atomic-Apply decision.* v2 was
built beside v1 as `tools/charybdis-live-v2/`, and took over the plain name
`tools/charybdis-live/` once v1 was removed. It keeps v1's core, its layering and the one rule
(nothing reads the firmware repository), and replaces v1's ported Studio
interface with its own: browser ES modules with pure, tested `view/` modules,
every posted edit built by `webview/view/edits.mjs` and staged against a real
draft in `tests/edits.test.mjs`, and a board that shows what the firmware does.
Edits exist only as a reviewed draft; a keyboard the app cannot open a draft for
is read-only, and the host refuses edits rather than writing them directly. v1
was frozen by this decision and has since been removed.

### D-L36 — The review checks reachable actions and save blockers

A layer can lock with nothing left to release it: `TG(n)` on Base when layer n
covers that key and has no `TG(n)`, `TO(0)` or `LOCK_LAYER(n)` of its own, a
chain of locks where the second hides the first one's way out, `TT()`'s fifth
tap, a behaviour branch or combo that locks. The firmware has no timeout and no
"clear all" key, so only unplugging recovers it. The keyboard cannot refuse
such a profile, and should not: the rules are the person's to break. The app
says so before Apply instead.

`core/model/layer-reach.js` walks every state a profile can reach from a
keyboard at rest, a set of locked layers with any layers held on top, resolving
every key as the firmware does and following each key's own layer action, its
behaviour branches, the combos that match the highest layer on, and the
trackball waking the pointer layer (unless the sniping layer keeps it off). A
**trap** is a set of locks it can reach from which Base cannot be reached once
every key is let go. It also reports, as warnings and notices, a layer with
keys that nothing reaches, a layer key onto a layer with no keys of its own,
transparent and `KC_NO` keys on Base in separate notices, a pointer layer that
cannot work, and layer keys the keyboard leaves to QMK (`DF()`, `PDF()`, a layer
past the bank). A combo whose reference differs from the highest active layer
uses the reference's raw keys; otherwise its inputs come from the resolved
active stack, including keys inherited through transparency. A combo is warned
about when no reachable stack can supply all its inputs. Holding is not limited
by fingers and a one-shot counts as a hold. The first four trapped lock states
have paths; any remainder is counted explicitly.

`core/model/profile-checks.js` adds whole-profile findings for bindings that
reach empty pointing slots (keys, behaviours and combo outputs), macros too long
for the keyboard's playback engine, and destination brightness or lighting
effects the keyboard would refuse. The latter are **blockers**: review shows
them, disables Apply, and the host refuses Apply before writing a recovery copy.
Other invalid data is still refused by its editor, profile decoder or save
preflight rather than offered for confirmation.

The draft reports its findings beside the keyboard's, both in the draft's
layer order, so each is **new**, **on the keyboard** or **fixed** by the draft.
Per-key findings include the key and code in their identity; grouped empty-key
notices include their positions, so replacing one issue with another on the
same layer does not call the new one existing.
The review lists them first, under Checks, each with the steps into it, the way
out and Show. Active warnings have orange backdrops; traps and blockers are red;
fixed findings stay quiet. Every warning or trap the draft keeps asks for
confirmation, whether the draft made it or the keyboard already had it: Apply
turns into "Apply anyway / Go back", and the host refuses to start an Apply of
such a draft unless the message confirms it. Notices and fixed findings do not
ask. Confirmation is tied to the reviewed draft revision, so a changed draft
must be reviewed and confirmed again. A new warning or trap points to a draft
edit group only when its source is unambiguous: one group in the draft, or an
unowned layer key changed at that exact position. The source group is marked
in the review, and the check links to it. A check already on the keyboard has
no draft source; checks with several plausible edits do not guess.

Two firmware facts were settled on the way. QMK's auto-mouse kept a lock of its
own on `TG()`/`TO()` of the pointer layer that `TO(0)` never released; the
runtime now takes those flips back and holds auto-mouse on for as long as the
pointer layer is locked (`docs/POINTER_MODES.md`). And a layout key the keyboard
does not own is refused where it is placed, since the keyboard's save check
covers behaviours and combos only; one already on the keyboard is reported, not
refused, so it never blocks an unrelated Apply.

### D-L37 — Any layer can be the base

The base is a slot, not a layer: the bottom one, always on, what every
transparent key falls through to, and what `TO(0)` returns to (layer 0 is the
base in the firmware's lookup, in the RGB base effect and in layer ownership,
so `DF()`/`PDF()` stay refused, D-L34). Rename & Reorder puts another layer
there with **Make base**, which swaps it with the current base, as one
reorder; the base row itself is never dragged, so a drag never changes the base
by accident.

With keys following their layers, the new base and the old one trade roles
rather than follow: a layer key that reached the base still reaches the bottom
slot, so `TO(0)` still goes home, and one that reached the new base now reaches
the old base, wherever it went. The key that held Numbers from Base holds Base
from Numbers, and `TO(Game)` becomes the key that switches back. Following them
instead, as the first version did, turned `MO(n)`, `LT(n, …)`, `TT(n)` and
`TG(n)` into `MO(0)` and the like, which do nothing because the base is always
on. The swap holds whether or not keys follow their layers: with the toggle off,
a key that kept its number would reach whatever took the old base's slot once
it moved on, so the toggle only decides for references to the other layers.
Every other key, behaviour and combo reference follows its layer, as do
names, colours, LED group rows, and the pointer, sniping and combo-reference
settings, which name a layer by what it holds. The startup layers are slots and
stay on the bottom one. On a base swap, transparent physical keys entering the
base become `KC_NO`, and `KC_NO` physical keys leaving the base become
transparent; unused matrix positions keep their stored values. A person
can still deliberately place a transparent key on the base afterwards. If the
old base had no layer colour, it receives the saved base HSV as its own
all-keys layer colour, including when the selected base effect is animated;
an existing layer colour stays as it was. The swap cannot make the new base's
empty keys do something or reach the old base when the
only keys that did sit on the old base itself; the review's checks (D-L36) name
both, count base `KC_NO` and transparent keys separately, and count every layer key that holds or toggles the base, rather than
the reorder inventing a way back. The checks compare the
draft with the keyboard as it is, matched layer with layer through the draft's
order, so what a reorder makes or mends is reported as new or fixed. The
review's Layer priority item names the new base first. See
`reorderLayers` in `core/model/portable-profile.js`.

### D-L38 — The dual-role setting is QMK's tapping term

The "Dual-role tap / hold" setting (settings id 0) only reached the runtime:
`LT()` behaviour rows left on the default, the `OSL()` double tap and the
runtime's hold bookkeeping. QMK's tapping engine, which decides tap or hold for
every `LT()`, `MT()`, `TT()`, `OSL()` and `OSM()` key before the runtime sees
it, kept the compiled `TAPPING_TERM`, so editing the setting changed almost
nothing a person could feel. With a portable profile the firmware now enables
`TAPPING_TERM_PER_KEY` and `QUICK_TAP_TERM_PER_KEY`
(`users/noah/lib/compat/qmk_live_tapping_config.h`) and answers both hooks with
the setting (`qmk_portable_profile.c`); the compiled `TAPPING_TERM` is its
default and applies until settings are live. Quick tap follows the tapping term
as it does in QMK by default. The one term is global: the app offers no
per-key tapping term, and a behaviour row's own tap / hold timing still governs
that row in the runtime.

### D-L39 — Only the keyboard ends a staging, together with its candidate

The app used to cancel a failed Apply by sending the VIA-stage abort and then
the custom-candidate abort. After a commit whose decision the app had not seen
(a status read failed after the marker became durable), the VIA channel
accepted that abort while the custom transaction refused its own, and the
owner's later VIA ACCEPT was refused: the other half's staged copy, then the
only complete target, was discarded. Conversely, a candidate that expired
before COMMIT released its lease without ending its VIA staging, which kept
reconciliation held and refused the next Apply until a restart.

The logical VIA staging now belongs to the owner's candidate. The host may
begin, fill and verify only the staging bound to the live candidate's
transaction and VIA identity, and only until COMMIT; the host's VIA abort is
refused. Every cancel before the decision (host abort, lease expiry,
supersession, a failed copy) ends both stores, through one idempotent VIA
cancel that also succeeds when nothing was staged. It does not wait for an
absent peer: the candidate is released at once, the old USB-side bank was
never touched, and the peer's ABORT completes when the link returns. Staging
frames the keyboard admits count as the candidate's progress, so a long
staging keeps its 15-second lease while status polls alone do not. After the
decision nothing cancels, whatever the host saw. See
[Logical Profile Transaction V1](architecture/logical-profile-transaction-v1.md#cancellation-and-lease-ownership).

### D-L40 — Firmware builds use only the eight-slot pointing engine

The per-preset Volume, Brightness, Zoom, Arrow and Pinch C handlers and the
old-geometry firmware bridge builds are retired. Every firmware build uses the
schema-2 eight-slot engine. The ordinary side-specific pair owns a live profile;
an owner-free generic or comparison image warms the same engine from validated
compiled slot records. The mode registry keeps deployed hold and lock keycode
identities, but no per-preset callbacks. `NOAH_PD_PROFILE=no` and the five-layer
snapshot-bridge option are rejected instead of quietly producing old firmware.

This removes the in-repository extraction path for an already deployed old
storage geometry. Retain old firmware and backups outside this build if they
are still needed; flashing the schema-2 pair does not migrate an old committed
profile in place. Historical profile readers and portable migration remain for
files that already carry the required source evidence. The active pointing
contract is [PD-mode domain v1](architecture/pd-mode-domain-v1.md).
