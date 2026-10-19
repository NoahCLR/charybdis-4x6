# Device-Resident Profile Target

This document defines the ownership model for live editing with Charybdis Ark
(the independent app repository in the
[local workspace map](../DEVELOPMENT.md#local-repositories-and-worktrees)). It supersedes the earlier project assumption that the three C
authoring files must remain the only source of truth during a live editing
session.

This is the technical profile contract beneath the broader product goal,
which Charybdis Ark owns in its `docs/PRODUCT_GOAL.md`.
The product target is first-grade keyboard control software, not only a working
live-edit protocol.

## Goal

The keyboard owns one readable, versioned, committed logical profile containing
the editable values represented by:

- `config.h` profile policy;
- `keymap.c` layers, behaviors, combos, and macros;
- `rgb_config.c` colors, feedback policy, and LED groups.

Ark connects to the keyboard, reads that logical profile, edits a
local draft based on its exact generation, commits a replacement safely to both
halves, and reads it back for verification. Reflashing is required only when
executable firmware capabilities change.

The keyboard does not store literal C source. Comments, formatting, preprocessor
expressions, and executable code are compiler inputs and cannot be reconstructed
losslessly from firmware data. It stores the complete structured values needed
to reproduce the supported behavior.

## Authority Model

On a connected keyboard, the active committed device generation is the live
authority. The three C files remain:

- compiled factory defaults and the recovery fallback;
- the human-reviewable and version-controlled representation, edited directly
  in the firmware repository.

Ark never reads or writes them. Its durable representation is the
portable profile; `.c` import and export are outside the app (D-L09, an app
decision in Ark's direction). Connecting a keyboard never
silently overwrites the keyboard or a draft.

Every editor draft records the device generation and digest it was based on. A
commit uses compare-and-swap semantics: if the device generation changed after
the draft was opened, the app refuses the write and asks the user to refresh or
resolve the conflict.

**The working copy** (D-F32). Between saves the keyboard may run unsaved live
edits: a working copy in RAM on each half, named by a session (per boot) and
a revision, over the saved generation it began from. It has no authority at
boot: a replug runs the saved generation. Live edits carry the session and
revision they were made against, compare-and-swap at that grain; Save makes
the working copy a new generation through the same commit path; Revert and
an Apply end it. The saved profile (GET `0x04`), the compiled defaults
(`0x05`) and the active one, working copy included (`0x21`), read separately
([Profile Wire, Live Edits](profile-wire-v1.md#live-edits)).

## Profile domains own layout and macros

The canonical schema-4 profile includes keymap domain `0x60` v1 (16 × 60
little-endian keycodes) and macro domain `0x70` v1 (128 streams in the existing
bank encoding, up to 34,903 bytes). Authored C remains the compiled default for
both; the authored macros are also generated already in the bank encoding
(`authored_macro_bank.h`, rebuilt with the firmware's encoder by
`tools/profile_introspect.py --write` and checked by real-profile validation),
so writing the compiled defaults copies bytes and compiles no macro. Domain
omission falls back to those authored defaults, just as for the other optional
domains, and so does a domain bit whose range cannot be found. Complete exports
must materialize all domains.

The active effective-profile generation supplies both key lookup and macro
playback. The keymap override is shared by QMK, combo origin, handled-key
transparency and RGB layer maps. Readers borrow immutable storage instead of
copying the matrix or bank into a new RAM cache. Macro publication uses the
existing no-running-macro safe boundary, and cache metadata is invalidated
before the next playback.

VIA is not supported (D-F31): the firmware builds without it, answers Raw HID
itself, and keeps no second copy of the layout or macros. The profile is the
only store.

The format-6 slot header is 36 bytes: domain mask at byte 0, uint32 payload
length at 1, origin/override flags at 5, generation at 6, payload CRC32 at 10,
compiled digest at 14, action ABI at 18, eleven zero reserved bytes at 22–32
(22–29 held the bound VIA generation and digest until D-F31), CRC16 at 33 and the final marker at 35. The CRC
covers bytes 0–32 prefixed by the store format. This preserves full 32-bit
lengths while admitting all seven domain-mask bits. Boot discovery reads at
most one bounded storage chunk per step; publication still writes the marker
last. These offsets describe the logical store view: on flash the marker is
read from the three separate cells and the payload starts at the 1 KiB
physical record boundary (D-F29). Retired format-5 markers remain recognized
at their old byte 31. Format 6 was never released with its earlier 32-byte
header (identity byte, CRC16 at 29, marker at 31), so the number did not
change. Such a slot reads here with bytes 31–34 erased; its original keyed
CRC identifies the retired layout. A 36-byte slot written by a test build
from before D-F31 carries a VIA generation or digest in bytes 22–29 and is
also refused. The keyboard runs its compiled defaults, and Profile Wire state
flag 9 identifies either refused test layout when nothing current is committed.

## Current Implementation Gap

Ark now reads the complete supported configuration from the keyboard,
edits all current domains, exports/imports a materialized snapshot of the whole layer bank,
and applies one profile generation without consulting repository sources.
Both halves are durably prepared before the decision marker; activation
waits until the other half has been seen holding the commit.

Live edits change the running profile on both halves without a save, and
Save, Revert and the edit mode are the keyboard's own operations (D-F32); the
app's use of them follows.

Remaining product work is physical interruption acceptance across each durable
boundary, guided recovery, the known one-half reconnect transition, broad
hardware acceptance, and standalone distribution. Until the interruption
matrix is recorded, Apply continues to create a recovery file.

## Required Device-First Operations

Ark must expose these operations with unambiguous direction (reset
to compiled defaults is not built yet):

- **Open from keyboard** — read the complete active logical profile and its
  identity;
- **Apply to keyboard** — validate and transactionally commit the current draft;
- **Refresh from keyboard** — discard or reconcile a stale local draft;
- **Save backup** — store a named, portable copy with schema and compatibility
  identity without changing the keyboard;
- **Restore backup** — validate a backup against connected capabilities and
  apply it as a new device generation;
- **Reset to compiled defaults** — activate the firmware's compiled profile by
  an explicit durable operation.

Only Profile Wire writes the profile; VIA clients are not answered (D-F31).
The app still detects a profile changed by another session and offers
adoption or conflict resolution before applying a stale whole-profile draft.

Labels such as `Apply`, `Reload`, `Source`, and `Device` must always state which
direction data moves. Digest equality alone is not a substitute for payload
readback.

## Non-Functional Requirements

Device ownership does not justify permanent scan-loop cost. In steady state:

- no profile payload is decoded on every matrix scan;
- no unchanged metadata frame is re-encoded before its deadline;
- RGB and behavior consumers use generation-owned materialized runtime views;
- inactive RGB stages do not resolve configuration rows;
- persistence and validation work remains bounded and runs only while startup,
  transfer, recovery, or activation work is pending;
- pointing-device cadence is measured on hardware and protected by a regression
  threshold.

Small materialized runtime caches are valid engineering choices. Each keyboard
half has 270,336 bytes of physical SRAM; static regression policies and runtime
high-water evidence must guide the tradeoff without presenting policy margin as
physical capacity.

## Implementation Sequence

The delivery slices are in Ark's product goal and the product's status in
Ark's direction; the firmware's status is in the
[firmware direction](../LIVE_EDIT_APP_DIRECTION.md#current-firmware-status).
The one-store commit and recovery ordering are implemented (D-F31). The
active phase is
acceptance: reboot, reconnect, applicable USB/role configurations,
interruption, two-half convergence, and polling/resource regressions before
production promotion.

## Completion Criterion

The device-first goal is complete only when a user can connect a keyboard whose
repository profile is unavailable or stale, read its complete supported
configuration into Ark, edit it, commit it to both halves, reboot, reconnect,
and recover the same editable values without consulting the C files.

## Current-format admission

Firmware accepts only Profile Wire 2.0, schema 4.0 with its current domain
versions, and store format 6 (D-F10, D-F14, D-F28). Every save is one
generation of the one store (D-F31). Legacy storage is refused at
boot: the keyboard runs its compiled defaults and its status says a slot holds
an earlier format. A backup in an earlier format is refused, not translated.
See [Profile Wire](profile-wire-v1.md) and
[the one-store transaction](logical-profile-transaction-v1.md) for admission and
recovery rules.

The profile is not bounded at 64 KiB anywhere it travels. Every length and
offset of a profile, and of each domain inside it, is 32 bits in the blob's
domain envelopes, the slot header, the store's I/O, the candidate wire and its
status, the split copy between halves, and the capability page (D-F28). What
does stay 16 bits is inside a domain whose own format bounds it: RGB and key
behaviours keep their 16-bit internal offsets, so each of those domains is at
most 65,535 bytes whatever the profile's size. The payload is capped at
130,048 bytes, one 128 KiB slot less the kilobyte its header and commit
marker take on flash (D-F29).

## Where the profile is stored

Each half keeps its own two slots, A and B, in a fixed region of its own flash
outside QMK's emulated EEPROM (D-F29), and reads the committed one in place:
no RAM copy of a slot exists. QMK's logical EEPROM holds only its own
eeconfig, with room for a user datablock (D-F31); its RAM mirror is 4 KiB. The copy between halves is
unchanged: each half writes its own inactive slot.

Every save keeps the same guarantee: power lost at any point leaves the
previous profile active. A save writes only the inactive slot, takes it out
of boot selection before writing anything to it (its header sector is
erased), writes payload and header, reads both back, writes a prepared marker
and, last, the commit marker. Boot selects the newest slot with a committed
marker, a valid header CRC and a payload matching its CRC32; a slot
interrupted anywhere before its commit marker is never selected. No flash
page is programmed twice between erases. Erases wait for a quiet keyboard (no
key held, no matrix change for 50 ms) and run one per scan, never in the scan
the save's writer runs in, which programs at most one page; a save waits while
the keyboard is busy. Each half reports its longest pauses over Profile Wire
(custom get `0x0D`). An EEPROM reset forgets the saved profiles before QMK
re-validates its EEPROM, so an interrupted boot forgets them again. See the
[storage baseline](storage-and-resource-baseline.md#profile-slot-flash-region-d-f29).

The maximum fixture includes every domain and retains a 12,288-byte reserve.
