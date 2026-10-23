# The firmware's architecture

This repository is a userspace runtime for the Charybdis 4x6, compiled into
QMK through the BK fork, not only a keymap. The authored profile in the keymap
folder says what the keyboard does; the runtime under `users/noah/` turns QMK's
events into decisions and projects them back into QMK's reports, layers and
LEDs. The profile a half runs is the device-resident one (D-F12, D-F30): the
compiled defaults until a client saves another, kept in that half's profile
slots and converged across both halves. Words are the
[glossary](../../GLOSSARY.md)'s; contracts are the [specs](../spec/README.md);
decisions are the [ADRs](../adr/).

The rule every module follows: **each runtime fact has one owner.** Everything
else reads it, projects it, mirrors it or adapts QMK to it.

## The parts

| Part | What it is |
| --- | --- |
| `keyboards/bastardkb/charybdis/4x6/keymaps/noah/` | The authored profile as data: `keymap.c` (layers, combos, `key_behaviors[]`, `MACROS()`, custom keys), `config.h` (timing, layer ids, pointer tuning), `rgb_config.c`, `pd_config.c`, and the generated `authored_macro_bank.h`. `keymap.c` and `rgb_config.c` include `noah_keymap.h`; `pd_config.c` includes the pointing schema and defaults headers instead |
| `users/noah/noah_keymap.h` | The authoring surface: ids (`noah_keymap_ids.h`), behaviour macros, `keymap_materialize.h` (turns the authored tables into the symbols the runtime reads) and the lighting helpers |
| `users/noah/hooks.c`, `noah_runtime.h` | Weak QMK `*_user` hooks, each calling its `noah_*` helper; a keymap that overrides one chains back into it |
| `users/noah/runtime_init.c` | Init, scan and housekeeping order: which module runs when, on which half |
| `users/noah/lib/` | The runtime packages below |
| `users/noah/source_manifest.mk`, `rules.mk` | The source list the firmware and the host compile gates share, and the build options (D-F35) |
| BK (`qmk-pin.json`) | QMK with the hooks this runtime needs, pinned to one commit (D-F04) |

## Who owns what

| Runtime fact | Owner | Read or projected by |
| --- | --- | --- |
| The profile each half saves, runs and copies; the working copy | `lib/profile/` ([profile flow](profile-flow.md)) | every runtime, through the `profile/runtime/effective_*_runtime.c` views |
| What an action is and how it is dispatched; literal report usages | `lib/action/` | key runtime, macros, pointing outputs |
| A key's effective behaviour and its participation | `lib/key/behavior/` | the key runtime's planners, validation |
| Presses, tap series, leases, pending releases | `lib/key/runtime/reducer/` ([key press](key-press.md)) | planners, projection, feedback, trace |
| Release, scan and tap-series decisions | `lib/key/runtime/planning/` | `transition.c`, which projects them |
| Applied held actions and repeats | `lib/key/ownership/` | projection, housekeeping |
| Applied layers and modifiers | `lib/state/ownership/`, `lib/state/modifiers/` | QMK's layer state and reports |
| Macro playback | `lib/macro/` ([macro playback](macro-playback.md)) | action lifecycle, input guard |
| Pointing mode state and handlers | `lib/pointing/runtime/`, `lib/pointing/modes/` ([pointer report](pointer-report.md)) | key runtime bridge, lighting, split sync |
| What QMK and the fork promise | `lib/compat/` ([its README](../../users/noah/lib/compat/README.md)) | everything that touches QMK |
| The lit frame | `lib/rgb/` ([lighting render](rgb-render.md)) | nothing: it only draws |
| Runtime state on the slave half | `lib/split/` | lighting and pointing on the slave half: it only mirrors |
| Shared storage, diagnostics, trace | `lib/state/shared/`, `lib/state/diagnostics/` | owners, through narrow accessors |

## Layering

- **Authored data stays data.** The keymap folder holds tables written the
  way a person authors them; runtime modules never include `noah_keymap.h`,
  and keymap files never include `noah_runtime.h`
  (`run_feature_gate_compile_tests.sh`).
- **QMK is reached through `compat/`.** A fork-specific hook, a QMK internal
  or a QMK data shape is adapted once there, with a contract check in
  `run_qmk_contract_checks.sh`; `run_compat_boundary_tests.sh` refuses the
  symbols it adapts anywhere else. Adapters normalise; they decide nothing.
- **Reducer, planner, projection.** Physical events enter the reducer first;
  planners decide; projection applies effects to the ledgers; only a ledger's
  aggregate transitions reach QMK's report (D-F55).
- **The profile is read through views.** A runtime asks its effective view,
  never the store or the compiled tables directly, so an activation swaps
  every reader at one safe boundary (D-F40).
- **Callbacks copy, the scan works.** Candidate frames and split RPCs are
  copied into a mailbox; a live edit does only bounded RAM work on the working
  copy in its callback. Validation, flash and split sends run in bounded steps
  from the matrix scan, never in a callback (D-F54).
- **Lighting and split sync own nothing.** They draw or mirror a fact another
  module owns; the slave half draws what the master half computed (D-F58).

## Where new work goes

| Change | Where |
| --- | --- |
| A key's behaviour, a combo, a macro's default, a colour | the keymap folder's data; run real-profile validation |
| What a behaviour means, or a new keycode the runtime owns | `lib/key/behavior/`, or a matcher in `lib/action/action_kind.c` |
| Release, hold or multi-tap timing | `lib/key/runtime/planning/`, with reducer support if state changes |
| Layer or modifier ownership | `lib/state/ownership/`, `lib/state/modifiers/` |
| A pointing engine or mode | the [pointing domain](../spec/pd-mode-domain-v1.md), `lib/pointing/modes/`, `lib/pointing/runtime/` |
| A lighting stage | `lib/rgb/stages/` ([`rgb/README.md`](../../users/noah/lib/rgb/README.md)) |
| A profile domain, Profile Wire request or split exchange | `lib/profile/` and its spec, with the QMK side in `lib/compat/` |
| A QMK or fork assumption | `lib/compat/`, with its contract check |
| A hook | `users/noah/hooks.c` and `runtime_init.c` |
| A source file | `users/noah/source_manifest.mk`, in the same change |
| RAM, flash or stack | the [resource policy](../spec/resource-policy.md) and its gates |

## Flows that cross modules

- [A key press](key-press.md): from the QMK hook through the reducer, planners
  and projection to the report, and the scan that settles holds and multi-taps.
- [The profile](profile-flow.md): boot, a client's Apply, Save and live edits,
  and how the halves end converged.
- [The lighting render](rgb-render.md): where each stage's truth comes from and
  how the slave half draws the same.
- [Macro playback](macro-playback.md): from the key that starts a macro to the
  keys it types.
- [A pointer report](pointer-report.md): from the sensor through the pointing
  runtime and its modes to the host, and how the pointer layer is steered.

Module internals sit beside their code: [`key/runtime/`](../../users/noah/lib/key/runtime/README.md),
[`compat/`](../../users/noah/lib/compat/README.md),
[`profile/`](../../users/noah/lib/profile/README.md),
[`macro/`](../../users/noah/lib/macro/README.md),
[`pointing/`](../../users/noah/lib/pointing/README.md),
[`rgb/`](../../users/noah/lib/rgb/README.md).
