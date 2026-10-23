# Coding standards

What a review holds a firmware change to. The rules under **Judgement** need a
reader; the ones under **Checked** are enforced by the named check, so a review
only confirms the change did not weaken it. Words follow `GLOSSARY.md`;
decisions are cited as D-F<n> (`docs/adr/`); the module map and layering rules
are in [architecture](docs/architecture/README.md).

## Judgement

### Where code lives

- The keymap folder holds the authored profile as data, written the way a
  person authors it and attached to the thing it names (`MACRO(VIA_MACRO_0,
  "Spotlight", "…")`, `COMBO_WINDOW(out, keys, 100)`, `layer_names[]` by layer
  symbol). Index-parallel arrays, packed wire values, schema headers, magic
  sizes and a value repeated from `config.h` are all signs the data is shaped
  for the runtime instead of the author: derive it in `users/noah/` instead.
- `users/noah/` is generic: it reads and serves the authored profile and holds
  no personal names, timings or colours. Config files (`config.h`,
  `rgb_config.c`, `pd_config.c`) hold data only; `SEND_STRING`, dispatch and
  other logic live in runtime files.
- Each runtime fact has one owner
  ([who owns what](docs/architecture/README.md#who-owns-what)). A module that
  needs a convenient copy gets a query or a snapshot from the owner, never a
  second home for the fact.
- A QMK or fork assumption is adapted once, in `users/noah/lib/compat/`, with
  a contract check; a module elsewhere that reaches into QMK internals or a
  fork-only hook is in the wrong place (the symbols compat already adapts are
  checked, below).
- A new keycode the runtime owns is a matcher in `lib/action/action_kind.c`,
  and where an action may be placed is one rule, `noah_action_supported_at()`;
  never a special case in a caller.

### The key runtime

- Physical events enter the reducer before any side effect. Planners decide;
  projection applies; adapters (`release.c`, `deferred_release.c`, lighting,
  split sync) carry decisions and make none. A planner returns effects and
  never writes a ledger (D-F55, [key press](docs/architecture/key-press.md)).
- Collapse states; never add a parallel clock. When a displayed state and the
  engine disagree about when something happened, read the engine's own
  transition instead of re-deriving its timing (D-F57).
- Authored data reaches layers only through layer ownership: the runtime
  owns `MO`, `LT`, `TT`, `OSL`, `LM`, `TG`, `TO` and `LOCK_LAYER` (D-F46);
  `DF()` and `PDF()` stay refused, since the base layer is fixed everywhere.

### Robustness

- Everything from a client or the other half is untrusted, including a split
  RPC whose normal sender is our own firmware: prove the command's shape,
  declared length, coordinates and the destination's capacity before any
  storage or drawing side effect.
- A Raw HID or split callback never validates, writes flash, publishes or
  sends to the other half: it copies the frame into a mailbox and answers from
  a published reply, and the scan does the work in bounded steps (D-F54). A
  live edit's bounded RAM update of the working copy is the one exception.
- A config invariant the compiler can check is a `_Static_assert`, not a
  runtime guard.
- A resource claim is per half and keeps physical capacity, linked accounting,
  policy margin and runtime high-water apart
  ([resource policy](docs/spec/resource-policy.md)); a design justified by
  "not enough RAM" names all four.

### Style

- Prefer a small change where the behaviour lives over a generic runtime
  rewrite, unless the task is architecture work.
- Remove dead code, unused functions and comments made stale by the change, in
  the same change.
- A `config.h` override is a bare `#undef X` then `#define X …`; `#ifdef`
  wraps whole feature sections, not single redefinitions.
- A comment states what a reader of the code cannot see (a constraint, a
  reason, a decision as D-F<n>), not what the next line does or how it came
  to be.

### Tests

- New behaviour and every changed invariant or module boundary gets a host
  test in the same change, through the module's interface.
- A failing test is fixed at its cause. A test changes only when the
  behaviour it pins changed on purpose, never to get a pass.
- Host tests and the pair build never stand for the keyboard: a change that
  needs hardware acceptance says so, and nothing is flashed to verify it.

### Documentation

- A lasting rule goes into the document that governs it in the same change: a
  decision as the next ADR, a contract in `docs/spec/`, a cross-module flow in
  `docs/architecture/`, a module's internals in a `README.md` beside it.
  Verification runs, counts and dated progress belong in commit messages.
- A change to what a person authoring or using the keyboard sees updates
  the page of the [firmware guide](docs/guide/README.md) it touches, and
  `README.md` when a capability changes. The README stays a short pitch in
  Noah's voice; development material goes in `docs/development.md`.
- A change to a wire format, fixture bytes, a spec's contract, or the probes
  and sources Ark compiles names the Ark follow-up
  ([independence](docs/development.md#independence-from-ark)).
- Names in code and docs use `GLOSSARY.md`'s terms, never the words it lists
  to avoid.

## Checked

| Rule | Check |
| --- | --- |
| Runtime modules never include `noah_keymap.h`; keymap files never include `noah_runtime.h`; host shims include QMK through the resolved path | `run_feature_gate_compile_tests.sh` |
| Every manifest source compiles in each feature configuration | `run_feature_gate_compile_tests.sh` |
| Every tracked `.c` file under `users/noah/` is in a source list of `source_manifest.mk` or allowlisted with how it is built; every listed file exists | `run_source_manifest_tests.sh` |
| Every firmware build runs real-profile validation and fails with it | `users/noah/rules.mk`; `run_feature_gate_compile_tests.sh` checks `rules.mk` still calls it |
| Authored data places only supported actions, with no raw layer actions | `noah_action_supported_at()` in `run_keymap_validation_tests.sh` and `run_real_profile_validation_tests.sh` |
| `authored_macro_bank.h` matches the profile's `VIA_MACROS()` | `run_real_profile_validation_tests.sh` |
| The weak hook defaults call their `noah_*` helpers, and a test override that chains keeps the behaviour | `run_hook_chaining_tests.sh` |
| Any other userspace or keymap file that defines one of those hooks calls the `noah_*` helpers its default calls, or replaces it on purpose with `// Replaces noah_<helper>(): <why>` in its body ([hook overrides](docs/guide/hooks.md#replacing-a-default)) | `run_hook_override_tests.sh` |
| The fork-only and internal QMK symbols `compat/` adapts appear nowhere else in userspace or keymap C, except allowlisted hooks QMK calls by name | `run_compat_boundary_tests.sh` |
| The QMK and fork contracts `compat/` relies on hold in the selected QMK (`QMK_ROOT`; the pin under verify) | `run_qmk_contract_checks.sh` |
| No `.c` file under `users/` or `keyboards/` reads a weak const object it defines | `tests/host/weak_const_default_test.py` (in `run_all_host_tests.sh`) |
| No tracked file outside prose, `docs/`, `tests/fixtures/` and `measurements/` names the app; frozen fixture hashes match | `run_firmware_client_independence_tests.sh` |
| Every runner runs in the build image; every workflow's `image:` equals `tools/build-image` | `tests/host/pair_build_script_test.py` (in `run_all_host_tests.sh`) |
| The pair builds only at the BK pin, in the image | `tools/build-firmware-pair.sh`, held by `pair_build_script_test.py` |
| The removed split baud selector fails the build | `run_split_transport_build_tests.sh` |
| A build with `VIA_ENABLE = yes` fails, naming D-F31 | `users/noah/rules.mk`; `run_via_refusal_build_tests.sh` |
| Generated host layout tables reproduce from their sources | `run_host_layout_tests.sh` |
| Reviewed stack paths and SRAM regression policies, per half | `NOAH_RESOURCE_CHECKS=yes` pair build; `run_firmware_stack_budget_tool_tests.sh`, `run_firmware_memory_budget_tool_tests.sh` |
