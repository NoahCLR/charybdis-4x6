# Architecture Decisions

This is the durable decision log for the live-profile project. Agents may add
detail, but must not silently reverse an accepted decision.

## Accepted

### D-001 — Live Means Data And Policy Within Compiled Ceilings

Status: accepted

The system live-edits every behavior already representable by the compiled
profile schema and advertised capacities. Arbitrary executable C, new hardware
support, USB topology, and capacity increases still require flashing.

Reason: a bounded interpreter and profile store can be validated and recovered;
general C hot loading cannot preserve the firmware safety model.

### D-002 — The Three C Files Remain Compiled Defaults

Status: accepted

keymap.c, config.h, and rgb_config.c remain the human-readable authored default
profile. A valid persistent device profile is the active deployed state.
Profile Studio exposes and reconciles both instead of pretending they are
always identical.

### D-003 — Use The Existing VIA Raw HID Endpoint

Status: accepted

Standard VIA commands remain authoritative for dynamic keymap and VIA macro
storage. Custom live-profile operations use a versioned keyboard-specific
channel on the existing 32-byte VIA Raw HID endpoint.

Reason: this avoids another USB interface and builds on existing host, EEPROM,
and split integration.

### D-004 — Use A Canonical Wire Schema

Status: accepted

The persisted and transported format uses fixed-width integers, explicit byte
order, explicit lengths, stable ids, a schema version, and checksums. It never
stores pointers or raw compiler structs.

Desktop and firmware implementations share golden byte fixtures and
round-trip tests.

### D-005 — Activate Candidates Transactionally

Status: accepted

A candidate is staged, completely validated, and then activated as one logical
generation. A rejected or interrupted candidate leaves the previous valid
generation active.

### D-006 — Behavior Changes Require A Safe Boundary

Status: accepted

Changes that can affect key output wait until the runtime quiescence contract is
satisfied. RGB-only preview may use a documented RGB frame boundary when it
does not alter behavior-affecting fields.

### D-007 — Both Halves Persist And Reconcile

Status: accepted

The USB half accepts host operations. The existing durable split
reconciliation model is extended or generalized so both halves converge,
survive role swaps, and recover after reconnect.

### D-008 — Milestone A Is RGB Plus Key Behaviors

Status: accepted

The first user-significant completion point is not layout-only live editing.
It is the complete Milestone A definition in README.md: live, persistent,
split-safe, source-aware RGB and key behaviors.

## Open Decisions For Stage 00

### D-009 — Persistent Storage Layout

Status: open

Decide:

- custom VIA config region versus a separately reserved EEPROM region;
- single record log versus two validatable slots;
- exact macro-region tradeoff;
- maximum encoded profile size;
- power-loss commit marker and checksum layout.

The decision must use measured current EEPROM and target RAM budgets.

### D-010 — Fixed Capacity Ceilings

Status: open

Measure and select at least:

- maximum logical layers
- maximum key-behavior rows
- maximum combo rows and keys per combo
- maximum RGB groups and LEDs per group
- maximum persisted hardcoded macro bytes
- maximum full candidate size

The firmware reports these values through capability negotiation.

### D-011 — Split Integration Shape

Status: open

Choose whether the live profile becomes one new canonical region in the
existing VIA split snapshot protocol or uses a sibling profile-specific
reconciliation package sharing its primitives.

The choice must keep region lists, mutation effects, digest coverage, and
receiver handlers mechanically aligned.

### D-012 — Profile Studio HID Adapter

Status: open

Prototype and choose between:

- a maintained N-API HID dependency loaded by the VS Code extension host;
- a small packaged native helper with a stable JSON or binary IPC boundary.

The adapter must be mockable, cancellable, serial per device, and explicit
about VIA/Profile Studio contention.

### D-013 — Preview And Apply Semantics

Status: open

Decide which RGB edits may be volatile previews, how preview rollback works,
and whether ordinary Apply performs device-first or source-first work. Partial
outcomes must remain visible either way.

### D-014 — Generation Authority And Drift Resolution

Status: open

Define how source digest, compiled-default digest, active-device generation,
USB-half generation, and peer-half generation are compared. Define the exact
rules for push, pull, reconnect, role swap, and same-generation digest
disagreement.
