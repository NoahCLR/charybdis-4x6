# Live Profile Editing Project

Status: architecture and implementation plan

Primary milestone: live RGB and live key-behavior editing

This folder is the implementation control center for making Charybdis Profile
Studio apply authored-profile changes to a connected keyboard without a
firmware rebuild and reflash.

It uses [the initial architecture review prompt](../../prompts/initial-architecture-review.md) and opens a new review thread
because live profile editing is a materially different architecture topic from
the [full-code audit](../2026-08-16-review-02/).

## Outcome

The finished system keeps the three authored C files as readable, reviewable
defaults while allowing the connected keyboard to run a versioned live profile:

- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`

Profile Studio becomes the bridge between source and device. It can compare,
preview, apply, persist, pull, push, and reset live profile state.

The project does not attempt to hot-load arbitrary C code. Firmware algorithms,
new hardware support, USB descriptors, and changes beyond compiled capacity
ceilings still require a firmware build and flash.

## Milestone A: Live RGB And Key Behaviors

Milestone A is complete only when all of the following are true:

1. Profile Studio discovers the intended keyboard and verifies a compatible
   live-profile protocol and schema.
2. The user can edit every current Profile Studio RGB surface and see it change
   on the keyboard without flashing:
   - layer colors and render modes
   - pointing-mode colors and locality
   - auto-mouse fade configuration
   - combo feedback color and locality
   - key-behavior feedback colors and policy
   - reusable and stage-specific LED groups
   - runtime enable or disable state for the authored RGB feedback stages
3. The user can add, edit, and remove supported key-behavior rows without
   flashing, including actions, tap branches, hold modes, timing overrides,
   repeat rate, and auto-mouse anchoring.
4. Firmware validates a candidate before activation and rejects it without
   partially changing the active profile.
5. Behavior-changing commits wait for a documented safe activation boundary so
   no held key, modifier, macro, combo, layer, or pointer-mode lease is orphaned.
6. Accepted changes reach both halves, survive reboot, and recover after a
   disconnect, role swap, or interrupted transfer.
7. Profile Studio shows whether draft, source, USB-half, and peer-half state
   agree.
8. The user can push source defaults to the keyboard, pull supported device
   state into source, and reset the keyboard to compiled defaults.
9. Host tests, target resource gates, firmware compile, and the Milestone A
   hardware matrix pass.

Live but volatile preview is useful, but it does not satisfy persistence,
split-consistency, source-reconciliation, or recovery requirements by itself.

## Architecture In One Page

The intended flow is:

    three C files
        -> compiled default profile
        -> effective profile provider
        -> RGB and key runtime consumers

    Profile Studio draft
        -> desktop validation and canonical encoder
        -> VIA Raw HID custom profile protocol
        -> staged device candidate
        -> firmware validation
        -> safe atomic activation
        -> persistent profile store
        -> split reconciliation

Core rules:

- Use one versioned canonical wire schema. Never transmit or persist raw C
  struct layouts, pointers, enum widths, or compiler padding.
- Compiled data remains the recovery default. A valid persisted override becomes
  the active deployed state.
- Runtime consumers read through provider or snapshot APIs, not directly from
  keymap-owned const arrays.
- A profile commit is validated and generation-stamped as one logical
  transaction even when transported in 32-byte chunks.
- RGB-only previews may activate at an RGB frame boundary. Any change that can
  affect key output uses the stricter keyboard-quiescent boundary.
- The USB half owns host communication. Both halves must converge durably
  through the split reconciliation layer.
- Protocol capabilities and fixed ceilings are discoverable. The UI must never
  offer a change the connected firmware cannot represent.

## Stages

| Stage | Purpose | Depends on | Result |
| --- | --- | --- | --- |
| [00](stages/00-contract-and-baseline.md) | Contract, baseline, capacities, and proof plan | none | Approved Profile Wire v1 design and measured budgets |
| [01](stages/01-live-link-transport.md) | Transport prerequisites and Live Link | 00 plus Review 19 transport prerequisites | Profile Studio can identify and communicate safely with the board |
| [02](stages/02-profile-schema-store-and-commit.md) | Schema, persistent store, validation, and atomic commit | 01 | A generic profile candidate can be transferred, validated, activated, recovered, and split-synced |
| [03](stages/03-live-rgb.md) | Live RGB vertical slice | 02 | All Milestone A RGB surfaces use the effective profile |
| [04](stages/04-live-key-behaviors.md) | Live key-behavior vertical slice | 02 | Supported behavior rows use the effective profile |
| [05](stages/05-milestone-a-integration.md) | Milestone A integration and hardware closure | 03 and 04 | Live RGB plus behaviors is a trustworthy end-to-end product |
| [06](stages/06-live-defaults-layout-and-macros.md) | Existing VIA surfaces and runtime defaults | 05 | Layout, macros, and practical config defaults share the same source/device UX |
| [07](stages/07-live-combos-and-layer-structure.md) | Dynamic combos and layer structure | 06 | Remaining high-coupling authored data is live within fixed capacities |
| [08](stages/08-production-closure.md) | Production closure and migration | 07 | Documentation, recovery, upgrades, budgets, and hardware evidence close the project |

Detailed briefs live under stages/. A stage may be split into multiple small
implementation passes, but its exit criteria cannot be weakened or silently
deferred.

## Folder Map

| File | Purpose |
| --- | --- |
| [userspace-architecture-review.md](userspace-architecture-review.md) | Findings, architecture decisions, tradeoffs, and recommended implementation sequence |
| [progress.md](progress.md) | Current status, completed history, verification, blockers, and next steps |
| [agent-workflow.md](agent-workflow.md) | Required entry, execution, testing, and handoff workflow for every agent |
| [decisions.md](decisions.md) | Accepted and open architecture decisions |
| [risks.md](risks.md) | Cross-stage risks, mitigations, owners, and closure evidence |
| [stages/](stages/) | Bounded stage briefs with scope, deliverables, tests, and exit criteria |

## Agent Entry Point

Every implementation agent starts in this order:

1. Read the repository AGENTS.md.
2. Run git status --short.
3. Read this file.
4. Read progress.md, decisions.md, risks.md, and agent-workflow.md.
5. Read the current stage brief completely.
6. Inspect the current tree instead of assuming the plan still matches it.
7. Update progress.md and the stage brief in the same pass as implementation.

Do not begin a later stage because an earlier stage looks mostly complete.
Record the exact exit evidence or keep the dependency open.
