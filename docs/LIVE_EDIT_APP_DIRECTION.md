# Live Edit — Firmware Direction

The firmware side of live editing: the decisions that constrain this firmware,
its open issues and the contracts the app relies on. The technical authority is
[`architecture/device-resident-profile.md`](architecture/device-resident-profile.md);
the wire, storage and split contracts are under [`architecture/`](architecture/README.md).
Verification logs, build numbers and the branch's setup history are in git, not
here.

## Who Owns What

Firmware owns the device: its runtime, its stored profile, the wire and split
protocols, and the specs under `docs/architecture/`. Charybdis Ark owns the app,
the product goal, the product's status and the app's decisions, in its own
repository's `docs/PRODUCT_GOAL.md` and `docs/LIVE_EDIT_APP_DIRECTION.md` (checkout
paths are in the [local workspace map](../README.md#local-repositories-and-worktrees)).
No firmware task needs that checkout.

Each decision has one home. Both direction documents keep every D-L heading, so
a citation resolves in either repository, but only the owner holds the text and
the other keeps a one-line pointer. Change a decision where its text lives. A
new firmware decision is numbered from D-F01 here; Ark continues the D-L series.

## The Direction In One Sentence

The keyboard becomes the source of truth, and Ark becomes a client of
the keyboard rather than a client of the repository.

## Current Firmware Status

The firmware serves the keyboard's complete configuration, compiled defaults
included, over Profile Wire, and publishes a complete profile to both halves as
one atomic logical generation that recovers from interruption and a lost peer
(D-L21, D-L22, D-L27, D-L39). The app's surface-by-surface status is in Ark's
direction.

Firmware work remaining before the product is complete:

- physical interruption acceptance at every durable boundary;
- adopting, or reporting as a conflict, writes by external VIA clients;
- broad hardware acceptance, including blank-firmware restore and the
  [PD-mode hardware matrix](architecture/pd-mode-domain-v1.md#hardware-acceptance).
  USB role migration is untested: on the normal pair the left half exposes no
  Raw HID interface (`FORCE_SLAVE`/`usb_disconnect`), so it needs role-switching
  firmware;
- reporting the build's enabled QMK features, so the app's keycode picker stops
  keeping its own list (an app open issue);
- the open issues below.

## Open Issues

- **Physical gesture acceptance (D-F01).** The buffered-repeat defect is
  reproduced and fixed through the real QMK combo/tapping path. Acceptance on
  both halves, pointing-mode routing and host Button 7 bindings remains open:
  [report and remaining plan](plans/gesture-timing-and-combo-arbitration.md).
- **One-half power-cycle recovery transition.** On 2026-09-12, after one half
  lost power while the other stayed powered, the first complete read failed
  with VIA storage flags 7 (dirty, recovery required) before settling to clean
  flags about 20 seconds later with the exact original profile. A full
  two-half power cycle was clean. The dirty transition is unexplained; do not
  suppress the readiness error without establishing its cause.
- **Why a peer stops acknowledging a push** (D-L22) and **why a peer flash
  write failed mid-copy** (D-L27) are both unknown. Both are now bounded and
  reported; the status fields D-L27 added should identify the cause next time.
- **The keycode-block migration has not run on hardware** (D-L42). Flashing
  it should make each half refuse its stored profile by action ABI digest,
  reset its VIA bank (sync metadata schema 3) and run the compiled defaults,
  which reproduce the captured profile with the thumbs, Click Spam and Drag
  Window on custom keys 0–3. Confirm that on both halves, that the halves
  converge, and that the pre-migration backup imports through the key-by-key
  translation, before relying on it.

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
into Ark, since layered into `core/` by D-L10) were branch setup and
are complete. D-L16 wired Studio's macro UI into v1 and went with it (D-L35).
App decisions keep their heading here and their text in Ark's direction.

### D-L02 — Ark is a VS Code extension for now

App decision; its text is in Ark's direction.

### D-L04 — The source editor is retired

App decision; its text is in Ark's direction.

### D-L05 — The keycode catalog is vendored, not parsed

App decision; its text is in Ark's direction.

### D-L06 — v1 ported Studio's whole UI and rebuilt only the model source

App decision; its text is in Ark's direction.

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
Release automation runs that same pair build and publishes both physical-half
images; a missing half fails the release instead of publishing a generic image.

### D-L09 — Ark owns a canonical profile format, not `.c`

App decision; its text is in Ark's direction.

### D-L10 — Ark is layered, and the layering is enforced

App decision; its text is in Ark's direction.

### D-L11 — Current state comes from the keyboard, including compiled defaults

App decision; its text is in Ark's direction.

### D-L12 — RGB rule identity and preview appearance are separate

App decision; its text is in Ark's direction.

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

App decision; its text is in Ark's direction.

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

App decision; its text is in Ark's direction.

### D-L20 — Differential Apply

App decision; its text is in Ark's direction.

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

App decision; its text is in Ark's direction.

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
D-L42 later renumbered every userspace keycode and gave action kind 7 to custom
keys, under a new action ABI digest; the `MACRO_n` numbers are gone.

### D-L26 — Macro slots share one visible memory, and every slot says what fits

App decision; its text is in Ark's direction.

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

App decision; its text is in Ark's direction.

### D-L30 — The draft shows itself where it is edited

App decision; its text is in Ark's direction.

### D-L31 — One mark per thing that has a colour

App decision; its text is in Ark's direction.

### D-L32 — Things sit in fixed places

App decision; its text is in Ark's direction.

### D-L33 — One home for each rule the app needs twice

App decision; its text is in Ark's direction.

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
`LT()` row timing is now governed by D-F01. D-L38 makes the dual-role setting drive QMK's own tapping term.

### D-L35 — Charybdis Live v2 is the app

App decision; its text is in Ark's direction.

### D-L36 — The review checks reachable actions and save blockers

App decision; its text is in Ark's direction.

### D-L37 — Any layer can be the base

App decision; its text is in Ark's direction.

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

### D-L41 — Pointing keycodes name slots, not factory presets

The eight configurable pointing modes use one keycode vocabulary: `PD_SLOT_n`
for hold and `PD_SLOT_n_LOCK` for toggle. The original six factory preset
names are no longer firmware keycode symbols. Slot assignment is stable; the
keycodes' numeric values moved to their fixed block with D-L42. Charybdis Ark
reads each slot's current name and behavior from the device and keeps former
preset expressions as import aliases for older portable files. See the
[PD-mode domain contract](architecture/pd-mode-domain-v1.md) for the fixed
native values.

### D-L42 — Userspace keycodes sit in fixed blocks, and custom keys are named

The userspace keycode families were numbered by enum order: pointing holds and
locks for six slots, then layer locks, then the keymap's own keys wherever the
layer count left them, and the two later pointing slots in a distant pair.
Adding a slot or a layer moved every keycode after it. Each family now owns one
aligned block, reserved beyond what is supported: custom keys `0x7e40`, pointing
holds `0x7e80`, pointing locks `0x7ea0`, layer locks `0x7ec0`, 32 each for the
last three and `0x7ee0` onward unassigned. Adding pointing modes or layers
changes storage and masks, never another keycode.

The keymap's hand-numbered keys (the thumbs, Click Spam, Drag Window) became
the first of 64 **custom keys**: `CUSTOM_KEY_n`, a named key that does only
what its behaviour row says, like a macro slot without a payload. Without a
row it does nothing. It goes on a layer or out of a combo; a behaviour step
cannot send one, because the synthetic record a step sends bypasses behaviour
lookup and would silently do nothing, so the firmware's keymap validation and
the app refuse it rather than implying chaining. Action kind 7, which named
the retired user macros, now names a custom key; the change came with a new
action ABI digest (`0x1d3fcacc`), so nothing decodes an old kind 7 as a
custom key.

Names live on the keyboard in settings version 5, beside the macro names, and
the keymap authors the defaults inline (`CUSTOM_KEYS`). The app offers custom
keys only on the known digest (Profile Wire feature bit 16), in their own
screen and picker section. Renumbering was a deliberate migration: firmware
refuses an older stored profile by digest and resets its VIA layout bank
(sync metadata schema 3), falling back to compiled defaults that reproduce
the profile; the app translates an older backup key by key on import. See
[PD-mode domain](architecture/pd-mode-domain-v1.md) for the blocks and
[portable profile](architecture/portable-profile-v1.md) for version 5 and the
translation.

### D-L43 — Split activity optimization, at QMK's default baud

Activity timestamp coalescing is on in the default build, which needs the fork's
activity hook (`noah-userspace-contracts` at `6889960271` or later). It is accepted on hardware; its
effect on the report rate is still to be measured, and
`NOAH_SPLIT_ACTIVITY_COALESCE=no` builds without it. The master keeps per-scan left-key acquisition and all existing runtime
and durable split protocols. A narrow QMK hook admits the latest timestamp snapshot
using the shortest enabled RGB idle timeout and updates successful state only
after a successful send. An independently gated, bounded recorder provides
transaction attribution without streaming during capture. See
[split activity sync](architecture/split-activity-sync.md). Runtime RPC replacement
and asynchronous scheduling remain behind the handoff's hardware measurement gates.

The split link stays at QMK's default 230,400 baud. 460,800 was built as a paired
option and removed after hardware comparison on 2026-09-28: with the same code and
coalescing, profile copies logged roughly 20–30 split transport failures per Apply
at 460,800 and none across three Applies at 230,400, and the other half's lighting
flickered, because QMK's lighting sync carries no checksum. Setting
`NOAH_SPLIT_BAUD` now fails the build. A faster link needs checksummed syncs
first, and new measurements. The split frame CRC
([plan](plans/split-sync-checksums.md)) is now in the default build; 460,800
stays removed until it is accepted on hardware and measured again.

### D-L44 — The app has its own repository and pinned firmware inputs

App decision; its text is in Ark's direction.

### D-L45 — Compatibility tests select both implementations explicitly

App decision; its text is in Ark's direction.

### D-L46 — Firmware has no reverse dependency on Ark

Firmware builds, host tests and diagnostics require no Ark checkout, and no
tracked firmware file outside prose and frozen fixtures names one;
`run_firmware_client_independence_tests.sh` enforces both. Firmware owns frozen
regression vectors in `tests/fixtures/client-regression/`. Ark owns
cross-language integration: it reads explicitly selected firmware checkouts and
compares the current implementations. The in-tree app has been removed and its
source history remains in Git. See
[the independence contract](architecture/ark-compatibility.md), including what
Ark consumes.

### D-L47 — The app is named Ark

App decision; its text is in Ark's direction.

## D-F01 — Physical gesture eligibility survives QMK buffering

Handled physical keys use press/release event timestamps; combo/tapping delivery
latency must not consume a repeated-tap window or extend a released hold.
Userspace queries existing QMK queues to preserve eligible continuations and
suppress scan-time holds after physical release. Authored `LT()` rows bypass
native tapping so one engine decides their behaviour; plain native dual-role
keys retain QMK semantics. No authored layout, combo or default timing changes.

Output still waits for arbitration. Winning combos suppress their constituent
keys, and synthetic combo outputs retain their delivery/origin contract. See
[the timing model](INTERACTION_MODEL.md#physical-gestures-and-buffered-delivery)
and [the implementation boundary](KEY_RUNTIME.md#physical-event-timing-at-the-qmk-boundary).
Profile Wire capability bit 17 distinguishes this policy without changing stored
profile bytes. Clients rejecting unknown feature bits must learn bit 17 before
connecting to this build. Physical acceptance remains in Open Issues.

## D-F02 — Tapping classification follows runtime ownership

Extend D-F01's authored LT exemption to all runtime-handled keycodes. Authored
MT/OSM rows and intrinsic TT/OSL ownership must not wait for a separate QMK
tapping classification before their owning runtime receives them. Unhandled
LT/MT/OSM remain native. This changes delivery arbitration, not the authored
action defaults or synthetic QMK action lifecycle. The existing fork hook needs
no new QMK changes. Feature bit 18 lets clients distinguish this from bit 17's
LT-only exemption; older strict clients must recognize it before connecting.

The real QMK pipeline tests held output thresholds for all four added families,
delivery into intrinsic layer ownership, and preservation of native tapping.
The release matrix also pins the empty release-interval rule consumed by Ark's
timing analysis. Physical acceptance remains open under D-F01.
