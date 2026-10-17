# Firmware documentation

Start with the [firmware guide](GUIDE.md) for what the keyboard does and the
[root README](../README.md) for release installation. These docs describe the
firmware; [Charybdis Ark](https://github.com/NoahCLR/charybdis-ark) owns its
editor guide, interface and product status.

## Using and configuring the keyboard

| Read | For |
| --- | --- |
| [Firmware guide](GUIDE.md) | Capabilities, behavior vocabulary, source authoring and building a flashable pair |
| [Interaction model](INTERACTION_MODEL.md) | Tap, hold, multi-tap, timing and buffered physical events |
| [Pointing modes](POINTER_MODES.md) | Slot engines, output rules, scrolling and factory presets |
| [RGB configuration](RGB_CONFIG.md) | Colors, localities, LED groups and stage order |
| [RGB feedback](rgbflow.md) | What pending branches, holds and commit pulses mean |
| [My keymap](KEYMAP.md) | Noah's authored defaults and shortcut choices |
| [Generated keymap overview](KEYMAP-OVERVIEW.md) | Layout diagrams and tables derived from checked-in authoring inputs |
| [My setup](MY_SETUP.md) | Personal host applications and bindings |

The authored defaults and generated overview are **not a snapshot of a
connected keyboard**. Saved device settings may differ. Read the keyboard with
Ark to see its active configuration.

## Developing the firmware

| Read | For |
| --- | --- |
| [Development](DEVELOPMENT.md) | Worktrees, the QMK pin, checks, Docker pairs, CI and releases |
| [Architecture index](architecture/README.md) | Ownership model and durable contract map |
| [Source map](architecture/source-map.md) | Entry points, packages, tooling and their documentation |
| [Change guide](architecture/change-guide.md) | Where to make a change and which checks cover it |
| [Runtime flow](architecture/runtime-flow.md) | Key, scan, pointing, RGB, macro and split paths |
| [Key runtime](KEY_RUNTIME.md) | Reducer, planning, projection and ownership boundaries |
| [RGB implementation](RGB_IMPLEMENTATION.md) | Generation-consistent rendering and feedback projection |
| [Hook overrides](HOOK_OVERRIDES.md) | QMK integration and mandatory helper chaining |
| [Adding a pointing mode](ADDING_PD_MODE.md) | Profile configuration versus a new firmware capability |
| [VS Code workflow](../.vscode/README.md) | Editor tasks and compilation database |
| [Measurements](../measurements/README.md) | Procedures and immutable hardware captures |

## Contracts and their authority

[Device-resident profiles](architecture/device-resident-profile.md) define the
ownership goal. [Firmware direction](LIVE_EDIT_APP_DIRECTION.md) records decisions,
current implementation status and open acceptance work. A requirement or a
historical decision is not evidence of completed hardware acceptance.

The architecture specs own wire bytes, validation, storage and runtime
invariants. Their `*-v1.md` filenames are stable document paths, **not a claim
that every format inside is version 1**. Current versions come from
[`profile_domain_registry.h`](../users/noah/lib/profile/schema/profile_domain_registry.h)
and [`profile_versions.h`](../users/noah/lib/profile/schema/profile_versions.h);
[Profile Wire](architecture/profile-wire-v1.md) documents the advertised contract.
Read [memory budgets](architecture/memory-budgets.md) before interpreting RAM or
stack figures. Dated linked measurements apply only to their recorded builds.

## Documentation tooling

[Profile introspection](tooling/PROFILE_INTROSPECT.md) explains how to regenerate
and check `KEYMAP-OVERVIEW.md` and its SVGs. Do not edit generated output by hand.
It covers selected authored inputs, not the complete device profile or the
`pd_config.c` slot records. The [legacy VIA-to-C bridge](tooling/VIA_TO_QMK.md)
is deprecated and is not a current editing workflow.

Keep each claim in its owning guide or contract and link to it from indexes.
Update implementation details alongside source changes; keep active plans in
the work queue and raw hardware evidence under `measurements/`.
