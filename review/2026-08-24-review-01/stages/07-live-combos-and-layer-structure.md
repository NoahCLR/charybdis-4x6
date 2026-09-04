# Stage 07 — Live Combos And Layer Structure

Status: blocked on Stage 06

## Objective

Make the remaining high-coupling authored structures live within fixed
advertised capacities: combos, logical layer structure, keymap-local identity
slots where practical, and any accepted hardcoded-macro domain.

## Entry Criteria

- Milestone A and Stage 06 regression matrices pass.
- Fixed capacities and layer-index authority remain valid.
- Whole-profile transactions and safe activation already handle
  cross-domain references.

## Dynamic Combos

Use QMK's combo introspection seam rather than patching combo internals:

- provide dynamic combo_count() and combo_get() views or the accepted compat
  equivalent;
- store key lists in bounded firmware-owned memory or canonical persistent
  views;
- validate key uniqueness, key count, output action, capacity, overlap
  assumptions, and behavior reachability;
- reset or defer activation around active, queued, delayed, or overlap-resolved
  combo state;
- update combo-origin, footprint, RGB, and key-runtime integration;
- preserve exact release and delayed-output semantics.

Do not mutate QMK combo arrays in place while the engine can hold their
pointers or state.

## Logical Layer Structure

Firmware compiles a fixed maximum layer capacity. Live state may define an
active logical count and source names, but:

- numeric indices on device remain canonical;
- adding or deleting a layer rewrites all affected keymap, behavior, combo,
  RGB, auto-mouse, sniping, lock, and macro action references in one candidate;
- base layer cannot be removed;
- deleted indices cannot remain referenced;
- layer state and owned locks are quiescent before structural activation;
- unused compiled slots are transparent/inactive by contract;
- Studio checks connected capacity before offering an add.

Layer names are source/UI metadata and need not be persisted unless Stage 00
explicitly chose that capability.

## Keymap-Local Identities

New executable custom keycodes are not live. If current custom identities are
data-only keys used to select behavior rows, reserve a fixed family of generic
profile key ids and map source names to them.

Any such design must:

- avoid collision with userspace keycodes, macros, layer locks, and QMK ranges;
- be stable across explicit source import/export;
- advertise capacity;
- remain data-driven;
- reject identities requiring new process_record code.

## Hardcoded Macros

Decide from the accepted project decision:

- keep hardcoded macros compiled-only;
- give them a compact custom live domain;
- or converge user-editable payloads onto VIA macro slots while preserving
  source aliases.

Do not duplicate the existing macro interpreter or cache.

## Likely Files

- profile schema, store, provider, and validation domains
- users/noah/keymap_materialize.h
- combo compat and origin modules
- keymap validation
- layer ownership and action description
- macro provider only if hardcoded payloads become live
- Profile Studio layer, combo, picker, diff, and source patch flows
- real profile and cross-domain tests

## Deliverables

- [ ] Dynamic combo provider and lifecycle safety
- [ ] Combo device operations plus explicit source import/export
- [ ] Fixed-capacity logical layer structure
- [ ] Atomic layer cross-reference rewrite and validation
- [ ] Layer device operations plus explicit source import/export
- [ ] Accepted generic identity design or documented compiled-only boundary
- [ ] Accepted hardcoded macro result
- [ ] Updated docs, screenshots, decisions, risks, stage, and progress

## Verification

Required coverage:

- QMK combo contract and origin tests
- combo overlap, delayed output, active-state activation, and RGB footprint
- key behavior and action lifecycle for combo outputs
- layer ownership, lock, momentary, LT, auto-mouse, sniping, RGB, and macro
  cross-references
- add, delete, reorder if supported, capacity, missing reference, and rollback
  layer candidates
- full profile validation and introspection
- Profile Studio fake device, source patch, picker, diff, and screenshot checks
- split convergence and role swap
- feature gates
- full host suite
- all required profile compiles
- firmware compile and fresh resource gates
- real-board combo and layer structural matrix
- all prior milestone regression matrices
- git diff --check

## Exit Criteria

- Supported combos can be added, edited, removed, persisted, split-synced,
  read from the keyboard, reset, and safely activated.
- Logical layers can be added or removed within capacity as one valid
  cross-domain transaction.
- Active QMK/runtime state never observes a partially changed combo or layer
  structure.
- Custom identity and hardcoded macro boundaries are explicit and tested.
- All previous milestone behavior remains green.

## Handoff

Stage 08 receives the full intended live surface. It focuses on compatibility,
migration, documentation, performance, hardware evidence, and closure rather
than adding another domain.
