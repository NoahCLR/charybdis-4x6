# Charybdis Profile Studio Product Goal

## Product Promise

Charybdis Profile Studio is intended to become first-grade control software for
this keyboard. A user connects the keyboard, sees the configuration that is
actually running, changes every supported behavior visually, and safely saves
the result without editing C or reflashing firmware.

This is more than a live-edit transport. The finished product must make the
keyboard understandable, customizable, recoverable, and trustworthy during
normal use. The current repository-backed VS Code extension is the development
shell for that product, not its final authority model.

Profile Studio is user-space HID control software; it does not require a custom
kernel driver. Normal configuration uses the keyboard's VIA and Profile Wire
HID interfaces. Firmware flashing remains a separate operation for changing
executable capabilities.

## Core User Experience

A successful normal session is:

1. Connect a Charybdis without opening its firmware repository.
2. Identify the keyboard, firmware, schema, capacities, and both physical
   halves.
3. Read the complete supported configuration currently committed on the
   keyboard.
4. Edit a local draft with clear validation, semantic differences, undo, and
   redo.
5. Preview changes that are safe to preview without persistence.
6. Apply the draft as one logical generation, without leaving layout, behavior,
   RGB, macro, or policy domains partly updated.
7. Verify the exact committed result by reading it back and proving both halves
   agree.
8. Reconnect after reboot and recover the same editable configuration from the
   keyboard alone.

The user must also be able to back up, restore, duplicate, import, export, and
reset a profile without ambiguity about which direction data moves.

## One Logical Device Profile

The keyboard owns one active, readable logical profile. It contains every
supported data-driven value represented by the three current authoring files:

- layout, layers, combos, macros, and key behaviors from `keymap.c`;
- timing, pointing, auto-mouse, and other live policy from `config.h`;
- colors, render policy, feedback stages, and LED groups from `rgb_config.c`.

The device stores canonical structured values, not literal C source. Comments,
formatting, preprocessor expressions, hardware definitions, and executable
handlers are not reconstructable profile data.

One logical profile does not require one physically contiguous EEPROM block.
The implementation may retain standard VIA storage for layout and VIA macros
and the custom profile store for other domains to avoid duplication and retain
VIA interoperability. If it does, one authoritative manifest and coordinator
must bind every participating domain to a complete logical generation and
digest. The product must never claim an atomic profile commit when only one
underlying store was updated.

Writes made by another VIA client are external configuration changes. Studio
must detect them and either adopt them into a new logical generation or present
an explicit conflict. They must not silently escape profile identity.

Both RP2040 halves are replicas of the same committed profile. USB role does
not determine durable ownership, and working split keys alone are not evidence
that profile state has converged.

## Source And Firmware Boundaries

The three C files remain valuable as:

- compiled factory defaults and recovery input;
- a reviewable, version-controlled representation;
- an explicit import source;
- a canonical export target.

Opening Studio never requires those files and never silently changes them.
Import from source, export to source, apply to keyboard, refresh from keyboard,
restore backup, and reset are separate directional operations.

Every current and future setting must be classified as one of:

1. live-editable profile data;
2. live-editable after adding a bounded runtime application owner;
3. structurally editable within a firmware-advertised capacity;
4. a compiled capability or safety ceiling that requires flashing;
5. executable or hardware-specific source that cannot be represented as data.

The UI must explain a non-live boundary instead of silently omitting the field
or offering an operation the connected firmware cannot perform. The maintained
field inventory is
[`field-classification.md`](../../review/2026-08-24-review-01/field-classification.md).

## Product Capabilities

The first-grade product includes:

- device discovery, identification, compatibility, and capacity reporting;
- complete profile readback and generation-bound drafts;
- layout, behavior, RGB, macro, combo, layer, and live-policy editors;
- semantic validation and understandable errors before mutation;
- preview, undo/redo, device apply, read-after-write verification, and reset;
- visible USB-half and peer-half convergence;
- named desktop backups and presets, with import/export suitable for sharing
  and version control;
- recovery from disconnects, partial transfers, stale drafts, corrupt storage,
  incompatible schemas, and interrupted commits;
- firmware compatibility guidance and a clearly separate firmware-update path;
- diagnostics useful to a normal user, with deeper details available for
  development and support;
- a reusable application core that does not depend on parsing an open
  repository, even while the VS Code extension remains a supported shell.

Multiple persistent named profiles on the keyboard are not required for the
first release. One durable active device profile plus named desktop backups is
the preferred initial tradeoff unless measured storage and UX evidence justify
more on-device copies.

## Quality Bar

Configuration software must not degrade the keyboard it configures. Production
promotion requires measured acceptance thresholds for:

- pointing report cadence and latency;
- matrix-scan cost;
- RGB frame timing;
- idle scheduler and protocol cost;
- transfer, validation, commit, and convergence duration;
- per-half linked memory, allocator high-water, and stack high-water;
- EEPROM capacity, endurance, interruption, and recovery behavior.

No profile payload is decoded on every scan. Unchanged protocol metadata is not
continuously rebuilt. Runtime consumers use bounded materialized views, and
persistence or reconciliation work runs only while it is needed.

The product also requires deterministic schemas and migrations, actionable
errors, accessible controls, safe defaults, no silent data loss, and automated
plus real-keyboard verification of all critical journeys.

## Delivery Strategy

Development proceeds in user-testable vertical slices:

1. restore and protect normal pointing and keyboard performance;
2. open the complete current profile from the keyboard;
3. edit and reapply one complete generation with conflict protection;
4. add backup, restore, reset, and failure recovery;
5. expand supported domains according to the field classification;
6. separate the reusable device/configuration core from repository parsing;
7. package the experience so normal configuration does not require a firmware
   workspace;
8. close compatibility, migration, performance, and hardware matrices before
   ordinary-firmware promotion.

Each slice must end in something testable on the real keyboard. Protocol,
storage, or UI infrastructure without a usable connected workflow is progress,
but it is not a completed product milestone.

## Completion Criterion

The product goal is complete when a user with only the keyboard and the
installed control software can inspect, understand, edit, validate, preview,
persist, back up, restore, reset, and recover every supported configuration
surface; both halves retain the same verified state across reboot and role
changes; unsupported executable changes are clearly identified as firmware
work; and the live-profile system causes no unacceptable input, pointing, RGB,
memory, or endurance regression.
