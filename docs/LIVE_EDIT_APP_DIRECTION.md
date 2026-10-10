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
paths are in the [local workspace map](DEVELOPMENT.md#local-repositories-and-worktrees)).
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
direction. Firmware accepts only current formats and every save binds VIA
(D-F10); old backups require client translation before restore. Compiled defaults
contain every enabled domain and use domain-seeking reads (D-F12).

Macro text is typed through a selected host layout (D-F16), with Unicode
entry for macOS, Windows and Linux as the fallback, protected automatically
from new physical presses with a per-macro override (D-F17); the encoding, ownership
and setup contract is in [runtime flow](architecture/runtime-flow.md).
Physical host acceptance of each layout and entry method is pending.

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

- **Unified gesture ownership.** Native LT/MT/OSM remain a compatibility
  boundary; adding a row can still change classification semantics. Runtime-owned
  LT/MT/OSM rows now follow QMK's default overlapping rule (D-F03). The
  intended direction is explicit defaults and one owner, without silently
  changing the current layout; it is not designed or implemented.

- **Physical gesture acceptance (D-F01).** The buffered-repeat defect is
  reproduced and fixed through the real QMK combo/tapping path. Acceptance on
  both halves, pointing-mode routing and host Button 7 bindings remains open.
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
- **The 32-slot upgrade has not run on hardware** (D-F09). Flashing it should
  make each half refuse its eight-slot stored profile by action ABI digest
  and run the compiled defaults; confirm that, that an eight-slot backup
  imports through the translation, and that slot 31 holds, locks and colours
  on both halves.
- **The bigger profiles have not run on hardware** (D-F14). Flashing them
  resets each half's storage (the flash base moves), and the stored profile
  is refused by action ABI digest. Before relying on them, measure on both
  halves: allocator and stack high-water with the 140 KiB EEPROM mirror; how
  long the maximum profile's validation (7,429 owner scans), boot (9,912)
  and the peer's prepare take in real time against the 60-second no-progress
  window; how long a 140 KiB wear-leveling consolidation pauses; and how long
  macro 127 takes to start on a full 10,327-byte macro bank. Also check
  typing, pointing and lighting with a maximum profile, a sixteen-key chord
  across both halves, and every participation control.

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
history that did not describe how the thing works. Active plans live in the
work-queue vault (`charybdis-notes`, see README), not in this repository. A
completed plan is folded into the spec it produced and deleted.

### D-L08 — The live-profile owner is on by default

`NOAH_LIVE_PROFILE_OWNER=no` builds an owner-free image; the app targets
ordinary firmware, not an engineering artifact. The owner needs a provisioned
`NOAH_PHYSICAL_HALF`, since durable profile origin identity is side-specific,
so the generic half-less build reports that at configure time and builds
without the owner rather than failing (failing would break the plain
`qmk compile` in the firmware guide). The firmware you flash is the side-specific pair
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
mid-read changes. The default window, the hold threshold and which combos
follow the default came with readout version 2; readout version 3 (D-F14)
adds each combo's enable and allowed layers, a counted reference page for the
sixteen-layer bank, and two wide pages per row for up to sixteen inputs. Old
firmware returns VIA unhandled and the app shows an update message without
losing its other readback. Combo names are stable
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

Historical decision, superseded for accepted formats by D-F10: storage format 2 keeps the 32-byte header and 4,064-byte payload; a distinct
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
  `NOAH_PROFILE_PD_SETTINGS_VERSION_ACCEPTED` in `profile_versions.h`, since
  replaced by the domain registry's one version per domain (D-F11, D-F13).

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

### D-L40 — Firmware builds use only the slot-based pointing engine

The per-preset Volume, Brightness, Zoom, Arrow and Pinch C handlers and the
old-geometry firmware bridge builds are retired. Every firmware build uses the
schema-2 slot engine (eight slots then; 32 since D-F09). The ordinary side-specific pair owns a live profile;
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

Names live on the keyboard in the settings domain (version 5 then, version 6
since D-F14), beside the macro names, and
the keymap authors the defaults inline (`CUSTOM_KEYS`). The app offers custom
keys only on the known digest (Profile Wire feature bit 16), in their own
screen and picker section. Renumbering was a deliberate migration: firmware
refuses an older stored profile by digest and resets its VIA layout bank
(sync metadata schema 3), falling back to compiled defaults that reproduce
the profile; the app translates an older backup key by key on import. See
[PD-mode domain](architecture/pd-mode-domain-v1.md) for the blocks and
[portable profile](architecture/portable-profile-v1.md) for the settings
domain and the translation.

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
and asynchronous scheduling are not built; either needs hardware measurement first.

The split link stays at QMK's default 230,400 baud. 460,800 was built as a paired
option and removed after hardware comparison on 2026-09-28: with the same code and
coalescing, profile copies logged roughly 20–30 split transport failures per Apply
at 460,800 and none across three Applies at 230,400, and the other half's lighting
flickered, because QMK's lighting sync carries no checksum. Setting
`NOAH_SPLIT_BAUD` now fails the build. A faster link needs checksummed syncs
first, and new measurements. The split frame CRC is now in the default build; 460,800
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
LT/MT/OSM remain native. Bypassing native tapping must not change when the
key's own first hold starts: an authored row without a first hold holds `MO()`
for LT, or the modifiers for MT/OSM, only once held past its tap-hold term, as
native QMK decides. A tap never turns the layer or modifiers on; the first tap
is unchanged. An authored LT row is therefore not a momentary layer: its layer
is not on from the press, and an interrupting key does not cancel its tap. The existing fork hook needs no new QMK changes. Feature bit 18 lets clients distinguish this from bit 17's
LT-only exemption; older strict clients must recognize it before connecting.

The real QMK pipeline tests held output thresholds for all four added families,
delivery into intrinsic layer ownership, and preservation of native tapping.
The release matrix also pins the empty release-interval rule consumed by Ark's
timing analysis. Physical acceptance remains open under D-F01.

Runtime-owned MT one-shot consumption follows the owner's release TAP decision,
including a tap whose output waits for multi-tap disambiguation. The selected
press-layer action remains fixed. Native MTs use QMK's resolved tap count;
holds and modifier-only tap output leave the one-shot armed. Output dispatch
does not consume again. See the [one-shot interaction rule](INTERACTION_MODEL.md)
and [release authority](KEY_RUNTIME.md#4-release-routing). This uses the existing
layer-ownership bridge and changes no physical timestamps or admission timing.

## D-F03 — Keys wait for an undecided tap/hold key

Keys pressed while a runtime-owned tap/hold key (authored LT/MT/OSM row) is down
and undecided wait for its decision, then replay in order: its tap first on a
release, its layer or modifiers first on a hold. This is QMK's default tapping
rule, so rolls keep their order and a key tapped under a held layer resolves on
it. A tap/hold key's pending taps are settled by any other key's press. Chosen
over emitting the next key at once (swapped order in rolls) and over settling a
tap on the next press (breaks quick chords under a held layer); the cost is a
wait of at most the tap-hold term for keys pressed during an undecided key.

Extended 2026-10-10 to authored keys whose hold is a layer held until release
(`PRESS_AND_HOLD_UNTIL_RELEASE(MO(n))`, such as the thumb keys whose tap
locks a layer): they are undecided in the same sense, so what is pressed under
their layer preview lands on that layer. A pending tap that changes layers is
settled by the next press of a key without a behaviour before that press is
resolved, so a key typed right after a lock tap lands on the locked layer.
Other handled keys keep independent series alive, as before. Chosen over
turning the layer on at the press when every outcome agrees (a per-key special
case, dropped).

Records are held after combos and native tapping, before every QMK feature,
through the fork's `process_record_admit_user()` hook, and replayed through
`process_record()` like QMK's own tapping queue
([contract](INTERACTION_MODEL.md#keys-pressed-while-a-taphold-key-is-undecided)).
It joins unreleased bit 18. Optional QMK modes (permissive hold, hold on other
key press) are not reproduced.

The admission queue has a bounded overload policy: when a record needs to wait
and the queue is full, replay exactly its oldest record, then append the new
one. This preserves FIFO and causal press/release delivery without dropping
records or increasing capacity. The oldest record may run before the tap/hold
decision, on the layer and modifiers active then; all remaining records keep
the normal waiting rule. Physical timestamps and record metadata remain intact.
Releases whose presses were already delivered still pass through. This is an
overload exception to the tap-first/hold-first ordering above, local to record
admission; gesture ownership and native QMK tapping stay with their existing
engines.

## D-F04 — Firmware pins its BK commit; `main` is a released, agreeing stack

The UF2 is this userspace and the BK fork compiled together, so firmware names
the exact BK commit it builds with in a committed `qmk-pin.json`, a published
commit on the fork's `noah-userspace-contracts-dev`. It is a plain file, not a
submodule, and code enforces it rather than habit: the pair build refuses any
other BK, CI and the release build check BK out at the pin, and `land` refuses
a pull request whose pin is unpublished. A non-required job tests the BK dev head as an
early warning. Re-pinning (`tools/pin-qmk.sh`) is a deliberate firmware change
that reports what moved in BK ([the BK pin](DEVELOPMENT.md#the-bk-pin)).

Firmware and its client agree by contract, not by commit: firmware states the
capability pages its keyboard answers, its BK pin and its fixture hashes
(`tests/host/run_contract_probe.sh`), and the client judges them with its own
runtime code. `dev` is the development line and tests against its own pins.
`main` moves only by a release, which promotes the BK released line, firmware
and the client together after testing that exact stack and their agreement, so
every `main` has a tag, both halves, the BK pin and the agreement table
([release verification](architecture/ark-compatibility.md#release-verification)).

## D-F05 — The flashed pair and the released pair come from one compiler

The firmware Noah flashes and the firmware users download are built by the same
compiler: `tools/build-firmware-pair.sh` always compiles in the image
`tools/build-image` names, which CI also builds in, running itself there with
Docker when it is started outside it, without network access, and never falling
back to the host's compiler. The build is reproducible: QMK's version stamps are fixed
(`SKIP_VERSION`, so the `QK_VERSION` keycode prints placeholders) and source
paths are mapped, so one userspace commit and BK pin give the same bytes on any
machine. Each pair has a note with its inputs, compiler and SHA-256, so a
published pair can be matched to a local one. Before this, local pairs used
Homebrew GCC 8.5.0 and releases GCC 14.2.1; the memory effect is recorded in
[memory budgets](architecture/memory-budgets.md#release-compiler-gcc-1421--2026-10-01).

The image is our own, `ghcr.io/noahclr/charybdis-build`, built from QMK's
official `qmk_cli` image (QMK CLI 1.2.0 and QMK's `arm-none-eabi-gcc` 15.2.0,
toolchain release `v15.2.0-1`, on Debian 13; index `sha256:b7d7fa8f…`) by
[`tools/build-image.dockerfile`](../tools/build-image.dockerfile). It changes
one thing. QMK builds its toolchain separately for each host, and the official
image's `linux/arm64` variant links different ARM target code (newlib, libgcc,
startup files) from its `linux/amd64` variant, so a Mac building natively and
CI built different pairs from the same inputs. Our image gives both platforms
the amd64 variant's target files and keeps each platform's native compiler
programs, which generate the same code: a Mac builds at native speed and CI's
pair equals it byte for byte. Nothing upstream can move or delete the image, and
it changes only when we choose. Earlier pairs, including every release before
it, were built in `ghcr.io/noahclr/qmk_base_container:debian13-qmk1.1.8`, a
modified base image with Debian's GCC 14.2.1. That package and its archived fork
remain the record of those builds.

`tools/build-image` names the image by its multi-arch index digest
(`name:tag@sha256:…`; the tag is only for reading), so a tag pushed again
cannot change the compiler. Each machine builds in its own platform's variant.
`sh tools/make-build-image.sh <tag>` builds both platforms from the Dockerfile,
refuses a tag that already exists, checks that their target files are
byte-identical, pushes only then, and prints the reference to put in
`tools/build-image` and every workflow's `image:` (a host test keeps them equal,
pinned and in our package). To move to a newer official image, read its index
digest with `docker buildx imagetools inspect ghcr.io/qmk/qmk_cli:latest` (the
top-level `Digest`), put it in the Dockerfile's `QMK_CLI`, and run the script
with a new tag. Then build a pair on the Mac and compare it with CI's pair for
the same firmware commit and BK pin, which every release does: different bytes
mean the platforms diverged. Compare it too with a pair from the previous
image: different bytes there mean the compiler changed, so record the memory
effect in [memory budgets](architecture/memory-budgets.md).

## D-F06 — CI runs for releases; the release gate may run Ark's agreement check

Development is verified locally (the work-queue vault's `verify`), so CI does not
run on `dev` or on pull requests into it. A release's `dev` → `main` pull request
runs everything `main` requires: `Promotion from dev`, `Host suite (GCC)`,
`Pair build` and `Agreement with Ark main` (`.github/workflows/ci.yml`). Nightly
runs check `dev` and BK drift and never block. `Pair build` builds both halves in
the release image at the pin (D-F05); the release publishes exactly those files
with `firmware-contract.json`, so nothing is rebuilt at the tag.

Firmware code, tests and tools still never read the app. The one exception is
that release gate: `Agreement with Ark main` runs the app's own agreement check
against this firmware, against Ark `main` (Ark `dev` when the release marker says
the release is joint), so `main` cannot move to a contract the released app does
not speak, not even through a merge on the GitHub page. The independence test
exempts that one workflow and nothing else.

## D-F07 — A directional mode can send once per movement

A directional mode either sends once per threshold step, as before, or once per
movement, so an imprecise movement does not send a burst of the same shortcut.
A movement ends after a 150 ms pause (the engine's existing idle boundary) or
the mode ending. Within one, only motion back against the direction that sent,
a whole threshold of it, sends again, so a back-and-forth sends once per leg
while a turn, or a single stray report in a long move, sends nothing more;
leftover motion is dropped. It is byte 87 of the unchanged
96-byte directional record, zero (once per step) in every existing profile, so
the domain version stays `1` and older firmware and apps reject a nonzero value
as reserved, as with axis `3` (D-L24). See
[PD-mode domain v1](architecture/pd-mode-domain-v1.md).

## D-F08 — A scrolling mode can scroll one axis only

A scrolling mode scrolls both axes, as before, horizontally only or vertically
only. The engine still chooses and holds an axis per gesture exactly as for
both axes; a gesture held on the excluded axis sends nothing, and its steps are
consumed and still decay the other axis. So a sideways swipe in a vertical-only
mode is dropped whole rather than having its slight vertical drift scroll, and
excluded motion is never remapped onto the allowed axis. It is byte 3 of the
scrolling record, which was zero, so every existing profile scrolls both axes;
the domain version stays `1`, and older firmware and apps reject a nonzero
value, as with D-F07. See [PD-mode domain v1](architecture/pd-mode-domain-v1.md).

## D-F09 — Thirty-two pointing slots, stored sparsely

The keyboard has 32 pointing slots, `0..31`, filling the reserved keycode
blocks: `PD_SLOT_n` holds at `0x7e80 + n`, `PD_SLOT_n_LOCK` toggles at
`0x7ea0 + n`, and action kinds 4 and 5 take operands `0..31`. Slots 0–6 keep
their identities and presets; 7–31 start disabled. Storing 32 fixed records
would take 3,080 of the profile's 5,088 bytes whether or not a slot is used,
so PD domain `0x50` version 2 stores only the slots that say something: a
header `[2, 32, 96, n, 0, 0, 0, 0]`, then `n` unchanged 96-byte records in
ascending slot ID, a record present exactly when its slot is configured or
disabled with a name. An omitted slot is disabled and unnamed; storing one
that way is noncanonical and refused, so every profile has one encoding. RGB
keeps one colour row per slot (domain `0x10` version 3, 32 rows, 120 bytes
more than version 2) because a disabled slot keeps its colour. The firmware
accepts only RGB v3 and PD v2 in schema 2.0, with unchanged storage geometry;
the effective cache still holds all 32 slots (3,072 bytes), filled at
publication, so pointing never reads storage.

`pd_mode_mask_t` is 32 bits. The split runtime sync carries mode ids, not
masks, and is unchanged. The action ABI digest, which now digests each mode
flag at 32 bits, is `0xf79c6151`; firmware therefore refuses a stored eight-slot
profile by digest and falls back to its compiled defaults, and the app imports
an eight-slot backup (`0x1d3fcacc`) by the key-by-key identity translation plus
PD v1 → v2 and RGB v2 → v3 ([portable profile](architecture/portable-profile-v1.md)).
The static RAM tripwire gains a 4 KiB feature increment
([memory budgets](architecture/memory-budgets.md)). See
[PD-mode domain](architecture/pd-mode-domain-v1.md) and
[RGB domain](architecture/rgb-domain-v1.md).

## D-F10 — Firmware accepts only what it writes today

The running firmware has one accepted profile/storage contract. At this cut it
was schema 2.0, RGB v3, key behaviors v1, combos v2, settings v5, sparse PD v2
and logical store format 3 (`NR`); D-F14 moved it to schema 3.0, RGB v4,
settings v6 and format 4 (`NS`), refusing the earlier ones the same way.
Settings v2–4, combo v1, schema 1, custom-only `NP` format 1 and logical `NQ`
format 2 are refused. There is no firmware migration path, legacy GET 9 source
page, feature bit 13 or legacy PD envelope validator.
Old backup translation belongs to the client before Apply.

Every save is a logical generation, including edits that change only custom
domains. Profile Wire BEGIN requires the current format and nonzero VIA
generation and digest. The owner always stages, prepares, accepts and converges the bound VIA
identity before publishing. Prepared peer pushes require a correlated bind;
background stale-peer repair fetches or sends the committed VIA identity too.
A repeated BEGIN retains its generation/digest-correlated bind after BUSY or
a lost reply. Convergence-only scans admit binding metadata so simultaneous
hosts can arbitrate while payload mutation remains fenced.

Client follow-up: require binding in `candidateMetadataForBlob`, refresh tests
and imported fixtures/specs, and repin the landed firmware revision. Firmware's
required checks stay independent of the client checkout. This cut does not
perform the later domain-registry deepening or claim hardware acceptance.

## D-F11 — One current shape and traversal per profile domain

The profile-domain registry defines canonical IDs, mask bits, current versions
and ordering once. Blob, candidate, validator, compiled-envelope, store and
provider modules consume that definition. One envelope walker serves blob
readback/decoding, whole-profile validation and synchronous/stepped storage
checks without owning their I/O scheduling. Domain modules retain semantic
record policy; combos and sparse pointing share bounded record iteration
between validation and publication. See [domain ownership](architecture/profile-wire-v1.md#firmware-domain-ownership).

Wire bytes and storage geometry are unchanged by D-F11. D-F12 subsequently
adds compiled settings/combos and compiled-domain seeking. Client integration recipes
that compile the firmware codecs must link `profile_domain_registry.c`, and
standalone pointing codec recipes must link `profile_reader.c`; refresh the
imported governing spec and repin after landing. The earlier D-F10 binding
follow-up still applies.

## D-F12 — One complete compiled profile, with reads that seek by domain

Opening compiled defaults retains each enabled domain's payload offset and
length in registry order. Reads emit only intersecting envelopes and payloads,
including reads across their seams; no domain depends on being last. The
complete standard profile contains RGB 3, key behaviours 1, combos 2, settings
5 and sparse PD 2. The same stream defines its checksums and its readback.

The settings module owns authored names and immutable factory scalars. A
compat adapter supplies pinned QMK factory constants and injects the settings
apply hook. Effective settings cache publication does not call up into compat.
Current native DPI, lighting, default layers and keymap options remain QMK
owners: boot preserves their EEPROM values and settings readback overlays their
current values. A profile without settings warms its factory fallback cache
without applying factory values to those owners.

RGB has one authored domain writer. Its immutable encoded cache warms once on
a cold path and supplies the same decoded view used for stored RGB; accessors
have no second authored interpretation, and frames never replay a writer.
The cache bound follows current RGB geometry. Compiled combos and settings
warm the existing effective caches before the owner opens output admission.

Domain versions, action ABI, accepted store format and storage geometry are
unchanged. Adding the two domains changes the compiled-default digest, so
stored profiles tied to the preceding digest fall back to compiled defaults;
firmware does no migration. A client restores a complete current profile bound
to the connected firmware. Client probes must link the new compiled RGB and
settings modules, refresh the complete compiled-profile fixture and affected
spec imports, and pin a published firmware revision after landing.

## D-F13 — A registry row names its domain module

Each registry row names its domain's module beside its ID and version; its
index is its position and its mask bit follows from that, so neither can drift
from the row order. The validator, compiled defaults and the compiled writer
declarations bind each row's operations by that name in switches generated
from the rows: a row without its module's validator or compiled encoder does
not compile, and every call stays a direct edge for the reviewed stack
contexts (no function-pointer tables). Neither the validator's dispatch nor
compiled defaults names a domain otherwise.

Every domain's compiled encoder lives in its own module (`key_behavior_compiled_v1.c`,
`profile_combo_compiled_v1.c`, `profile_pd_compiled_v1.c`, beside the RGB and
settings ones); a build without the domain writes nothing and the compiled
profile omits it. A domain's compiled length is what its encoder emits.

A validated profile stores decoded views only for RGB and key behaviours, the
domains read through record accessors. Publication finds the combo, settings
and pointing payloads by walking the snapshot's envelope
(`noah_profile_blob_v1_find_domain`), so no profile copy carries payload
ranges; the provider checks the two remaining views. Each domain has exactly one
accepted version, so no codec takes a version and the version-acceptance
macros are gone.

Wire bytes, digests, store geometry and validation error details are unchanged.
Client probes compiling compiled defaults link the three new encoder modules;
the blob codec needs `profile_reader.c` and `profile_checksum.c`; the combo
codec reads a payload range
instead of a view; the compiled fixture drops the unused
`profile.action_abi_row_visits` key. See
[domain ownership](architecture/profile-wire-v1.md#firmware-domain-ownership).

## D-F14 — Bigger profiles: sixteen layers, 128 of everything, 64 KiB slots

The profile grows in one version transition, so a user upgrades once:

| Table | Before | Now |
| --- | ---: | ---: |
| Layers | 8 | 16 (IDs 0–15) |
| Shared behaviour rows / populated steps | 64 / 128 | 128 / 640 |
| Combos / inputs per combo | 32 / 4 | 128 / 2–16 |
| Custom keys | 64 | 128 |
| VIA macros | 64 | 128 |
| Name | ASCII, 20 or 23 bytes | counted UTF-8, 32 bytes, everywhere |
| Custom-profile slot | 5,120 bytes | 65,536 bytes (65,504 payload) |
| VIA region (keymap and macros) | 8 KiB | 12 KiB |

Each half's logical EEPROM is 140 KiB: VIA owns `0x0000–0x2FFF`, slot A
`0x3000–0x12FFF`, slot B `0x13000–0x22FFF`. Wear-leveling backing is 280 KiB.
QMK mirrors logical EEPROM in SRAM0–3, so this adds 124,928 bytes of fixed RAM
per half; the static-data regression policy moves with it, from fresh linked
accounting ([memory budgets](architecture/memory-budgets.md)). It is a policy
for the new representation, not a hardware limit, and allocator, stack and
timing evidence on the keyboard remains open. Pinned QMK's wear-leveling log
addresses 19 bits, so 140 KiB needs no QMK change; consolidation now rewrites
140 KiB of flash, a pause the hardware acceptance times.

The VIA keymap grows from 960 to 1,920 bytes; the 12 KiB region keeps the
macro bank at 10,327 bytes (it was 7,191), shared by all 128 macros, with
the per-macro playback budget unchanged.

Formats: profile schema 3.0 with RGB v4, key behaviours v2, combos v3,
settings v6 and pointing v3; logical store format 4 (`NS`); VIA sync metadata
schema 4. Wire masks of layers are 32 bits with every bit above the advertised
bank refused; counts and member lists are counted, so a smaller combo sends
fewer bytes. Readback pages, capability fields and storage addresses are wide
where the new sizes need it (capability layout 2, feature bit 19). Older
firmware refuses the new formats and the new firmware refuses the old ones;
geometry moves the flash base, so old storage is reset, never reinterpreted.
Backup translation (eight layers to sixteen, 64-slot banks to 128, old custom
keys by stable slot) belongs to the client, as before (D-F10).

Custom keys leave `0x7e40–0x7e7f`, which pointing holds box in, for an aligned
block `0x7f00–0x7f7f`; the old block is retired and inert. Pointing holds,
locks and layer locks keep their values. VIA macros fill QMK's own
`QK_MACRO` range, `0x7700–0x777f`. The action ABI digest changes, so stored
profiles from earlier firmware are refused by digest.

Behaviours and combos gain participation controls at four scopes (master,
layer, definition, placement), decided once per press against the press's
source layer; see [participation policy](architecture/participation-policy.md).
A bypassed `LT()` or `MT()` placement must return to native tap-hold timing,
which needs QMK to ask per record rather than per keycode, so BK gains a weak
`is_tap_record_user()` hook beside `is_tap_keycode_user()`. BK also preserves
an optional opaque byte of press context through its queues and synthesized
tapping releases, and provides a combo member-record gate before state updates.
The context keeps repeated buffered presses independent; the gate makes an
excluded duplicate inert to an eligible chord's member state.
Defaults allow everything, so a profile that never uses them is unchanged.

`tests/fixtures/maximum_profile_v3.fixture` holds every table at its maximum
at once: 37,667 bytes, leaving 27,837 bytes of the slot. It validates, commits,
boots and publishes whole on one half and prepares and commits on the peer;
settings validation reads 20 bytes per step, like the other domains, so the
whole profile validates in 7,429 owner scans
([storage baseline](architecture/storage-and-resource-baseline.md#schema-3-ceilings-and-the-maximum-profile)).

The authored layout keeps its eight layers; layers 8–15 are transparent,
unnamed and inactive. One supported tap depth (five) bounds behaviour steps,
step capacity and the extra tap-branch colours; formats are counted so a
later firmware can raise it.

Client follow-up: Ark adopts every format above, derives its limits from the
capability pages instead of constants, adds the controls, and translates older
backups; pin the landed firmware.

## D-F15 — Unicode macro entry preserves live ownership

Text uses canonical UTF-8 and capability-gated Host settings (OS override
and independent Unicode enablement). Auto uses QMK USB detection, with unknown
remaining inert for non-ASCII playback; host detection never confirms input
configuration. The
settings-v6 zero defaults and current profiles stay valid. Unicode emission is
scan-driven and uses the fork's report-only modifier override, so the current
live modifier owners are revealed afterward. ASCII text is typed with
ordinary keys in every mode; only non-ASCII scalars use host Unicode entry, so
enabling Unicode never makes ASCII macros depend on the host input source. Host methods, limits, cancellation
and exact byte encoding are governed by [runtime flow](architecture/runtime-flow.md).
No native helper or automatic host-input-source detection is implied.

## D-F16 — Macro text is typed through the host's layout

The keyboard sends key positions and the computer's layout makes them
characters, so a macro types text through a host layout chosen in settings
scalar 27 (feature bit 22): bits 16–23 name a layout from the
[host layout catalogue](architecture/host-layouts-v1.md), and bit 24 marks a
Mac that classified the keyboard as ISO. Each character, accented letters
included, is typed with the keys that layout uses, dead keys too. Unicode entry
is only the fallback for characters the layout cannot type, with its digits
typed through the layout; a character neither can type is refused before
output. On macOS, hex entry needs Unicode Hex Input as the input source, so it
is a layout of its own and no Option layout takes hex entry.

The catalogue is data, not code: one versioned fixture generated from macOS's
and Linux's own layout definitions and QMK's Windows data, from which the C
tables are generated. Clients read the same fixture, so labels and typing
cannot disagree. Layout 0 (US) is the default and types exactly what earlier
firmware typed. The keyboard cannot see the computer's layout; the user picks
it. Typing rules are in [runtime flow](architecture/runtime-flow.md).

Client follow-up: Ark vendors the fixture, offers the layout in Settings →
Host behind bit 22, checks macro text against the chosen layout, and labels
keys as that layout prints them; pin the landed firmware.

## D-F17 — Per-macro protection ignores new physical presses

VIA macros may override automatic input protection through an optional prefix,
advertised by feature bit 23. Automatic protects a macro needing Unicode entry
under the layout latched at playback start; native accents remain interruptible.
Explicit On and Off travel with the macro bank, independently of its text/name.
Ignored matrix presses remain ignored until released, including after playback
ends. Earlier releases and synthetic macro output are admitted. Cancellation,
failure and reset still clean up owned keys. Protection is a physical-key
admission policy, not a host input lock or delayed typing queue. The canonical
encoding and scope are governed by [runtime flow](architecture/runtime-flow.md).

Client follow-up: expose Automatic/On/Off per macro, explain ignored typing,
preserve policies in edits and backups, show stored and layout-derived changes
in Review, and gate overrides on bit 23.
