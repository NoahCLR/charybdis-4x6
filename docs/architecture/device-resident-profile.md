# Device-Resident Profile Target

This document defines the target ownership model for Charybdis Profile Studio
and live editing. It supersedes the earlier project assumption that the three C
authoring files must remain the only source of truth during a live editing
session.

This is the technical profile contract beneath the broader
[`Charybdis Profile Studio Product Goal`](../tooling/PROFILE_STUDIO_PRODUCT_GOAL.md).
The product target is first-grade keyboard control software, not only a working
live-edit protocol.

## Goal

The keyboard owns one readable, versioned, committed logical profile containing
the editable values represented by:

- `config.h` profile policy;
- `keymap.c` layers, behaviors, combos, and macros;
- `rgb_config.c` colors, feedback policy, and LED groups.

Profile Studio connects to the keyboard, reads that logical profile, edits a
local draft based on its exact generation, commits a replacement safely to both
halves, and reads it back for verification. Reflashing is required only when
executable firmware capabilities change.

The keyboard does not store literal C source. Comments, formatting, preprocessor
expressions, and executable code are compiler inputs and cannot be reconstructed
losslessly from firmware data. It stores the complete structured values needed
to reproduce the supported behavior.

## Authority Model

When Profile Studio is connected, the active committed device generation is the
live authority. The three C files remain:

- compiled factory defaults and the recovery fallback;
- the human-reviewable and version-controlled representation;
- an import source for intentionally replacing device state;
- an export target for saving a device profile back to the repository.

Source import and source export are explicit operations. Merely opening Studio
or connecting a keyboard must not silently overwrite either side.

Every editor draft records the device generation and digest it was based on. A
commit uses compare-and-swap semantics: if the device generation changed after
the draft was opened, Studio refuses the write and asks the user to refresh or
resolve the conflict.

## One Logical Profile, Initially More Than One Storage Adapter

The user should see one profile even while firmware continues to use established
QMK storage owners internally:

- standard VIA dynamic storage for layout keys and VIA macros;
- the versioned custom profile store for RGB, custom behaviors, and future
  profile-policy domains.

This is an intentional migration boundary, not the final user model. Studio must
read both stores into one generation-aware document and coordinate writes and
verification across them. Replacing VIA storage with a duplicate custom keymap
format is not required for the device-first milestone and would reduce VIA
interoperability.

The final product requires one authoritative manifest and coordinator binding
the participating store digests to one logical generation. External VIA writes
must be adopted as a new generation or exposed as conflicts. If atomic
cross-store commits cannot be made trustworthy, the limitation must be visible
and recoverable; the product must not claim a complete commit. Connected-half
VIA readback is not equivalent to proof that both halves hold the same complete
logical profile.

## Current Implementation Gap

The current engineering milestone is source-driven:

- Studio parses the three C files;
- `Apply live` uploads RGB and custom key behaviors through Profile Wire;
- layout keys are diffed, written, and read back through standard VIA;
- the firmware reports capabilities, status, generations, and digests.

It cannot yet download the committed custom profile payload. Profile Wire's
current `READ_SURFACE` feature means capability and status pages only; it does
not mean complete profile readback. Studio therefore cannot currently open the
keyboard as its configuration source.

The existing canonical schemas, validation, dual-slot store, safe activation,
generation tracking, split prepare/commit barrier, and convergence checks remain
useful foundations. The missing architecture is host-readable committed data and
a device-first editor lifecycle.

## Required Device-First Operations

Profile Studio must eventually expose these operations with unambiguous
direction:

- **Open from keyboard** — read the complete active logical profile and its
  identity;
- **Apply to keyboard** — validate and transactionally commit the current draft;
- **Refresh from keyboard** — discard or reconcile a stale local draft;
- **Save backup** — store a named, portable copy with schema and compatibility
  identity without changing the keyboard;
- **Restore backup** — validate a backup against connected capabilities and
  apply it as a new device generation;
- **Import from source** — intentionally create a draft from the three C files;
- **Export to source** — write a canonical, reviewable C representation without
  changing the keyboard;
- **Reset to compiled defaults** — activate the firmware's compiled profile by
  an explicit durable operation.

Writes made through another VIA client are treated as external changes. Studio
must detect them and offer adoption or conflict resolution before applying a
stale whole-profile draft.

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

1. Measure and remove the current steady-state scan and RGB costs.
2. Freeze the logical-generation manifest, cross-store commit/recovery rules,
   and external VIA edit semantics.
3. Add bounded, chunked readback of the exact committed custom profile payload.
4. Add a device snapshot model that combines Profile Wire and standard VIA data.
5. Make connected Studio sessions open from that snapshot and guard writes with
   generation/digest conflict detection.
6. Add named desktop backup/restore plus explicit source import and canonical
   source export.
7. Extend the device schema to supported defaults, macros, combos, and logical
   layer structure within advertised capacities.
8. Prove reboot, reconnect, both USB orientations, role swap, interruption,
   two-half convergence, and polling/resource regressions before production
   promotion.

## Completion Criterion

The device-first goal is complete only when a user can connect a keyboard whose
repository profile is unavailable or stale, read its complete supported
configuration into Studio, edit it, commit it to both halves, reboot, reconnect,
and recover the same editable values without consulting the C files.
