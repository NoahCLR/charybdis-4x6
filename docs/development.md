# Developing this firmware

How this repository is worked on, checked, pinned and released. What the
firmware does and how to configure it is in the [README](../README.md) and the
[firmware guide](guide/README.md); how the code is shaped is in
[architecture](architecture/README.md). The editor's build, test and
compilation-database tasks are in the [VS Code workflow](../.vscode/README.md).

## Local repositories and worktrees

Noah's main checkouts sit side by side in one workspace folder
(`/Users/noah/dev/charybdis` on his main Mac). A Mac has only the ones it works
on; building firmware needs the BK checkout beside it:

- Firmware: `charybdis-4x6` ([NoahCLR/charybdis-4x6](https://github.com/NoahCLR/charybdis-4x6)).
- Ark: `charybdis-ark` ([NoahCLR/charybdis-ark](https://github.com/NoahCLR/charybdis-ark)).
- BK, the QMK fork this firmware compiles with: `bastardkb-qmk`
  ([NoahCLR/bastardkb-qmk](https://github.com/NoahCLR/bastardkb-qmk):
  development on `noah-userspace-contracts-dev`, released line
  `noah-userspace-contracts`, legacy upstream mirror `main`).
- Work queue: `charybdis-notes`, a private Obsidian vault holding notes,
  tasks, active plans and keyboard checks for all three repositories, and the
  tools every change goes through. Its `AGENTS.md` says how a task is refined,
  picked up and landed.

`dev` is the trunk. Each task branches from `dev` as `<type>/<slug>` in its
own worktree, is verified locally and reaches `dev` through a pull request,
which the vault's `land` squash-merges for exactly the verified commit; no CI
runs for that merge, because local verification is the gate. `dev` accepts
changes only through pull requests. `main` is the released line and moves only
when a release promotes `dev` into it. Each landed commit's message ends with
what verify ran, and the BK commit it was built with is the one its own
`qmk-pin.json` names. A firmware release is a date tag, `vYYYY.MM.DD`, on this
repository and on the BK commit it is built with; Ark releases separately, and
both release together only when the contract between them changes. This
repository is a GitHub fork of Bastard Keyboards' userspace: push only to
`NoahCLR/charybdis-4x6`, never upstream. The clone's `gh` default and
pre-push hook enforce that.

Use the worktree assigned to your task; `git worktree list` shows the others.
Relative sibling paths describe the main workspace layout and may not hold in
a worktree: select QMK explicitly with `QMK_ROOT` for host tests and
`QMK_HOME` for the QMK CLI, and set `QMK_USERSPACE` to the firmware worktree
being built. Inspecting another checkout is fine; editing it needs that scope
in the task. `../builds` is output, not source.

## Checks

Every runner is `sh tests/host/<runner>`. Run the ones for the area you touch
while you work, then the gates below before handing back.

| Area | Runners |
| --- | --- |
| Key runtime | `run_participation_tests.sh`, `run_key_runtime_release_matrix_tests.sh`, `run_key_runtime_modifier_hold_integration_tests.sh`, `run_pd_mode_key_runtime_integration_tests.sh`, `run_key_runtime_layer_lock_integration_tests.sh`, `run_key_runtime_scenario_tests.sh`, `run_key_runtime_integration_harness_tests.sh`, `run_tap_depth_eight_tests.sh` (the second tap-depth configuration) |
| Pointing | `run_pd_mode_tests.sh`, `run_pd_mode_handlers_tests.sh`, `run_pd_runtime_tests.sh`, `run_pointer_layer_policy_tests.sh`, `run_split_runtime_sync_tests.sh` |
| Authored profile | `run_key_behavior_lookup_tests.sh`, `run_key_behavior_validation_tests.sh`, `run_keymap_validation_tests.sh`, `run_real_profile_validation_tests.sh` |
| Lighting | `run_rgb_validation_tests.sh`, `run_rgb_layer_render_tests.sh` |
| Hooks and ownership | `run_hook_chaining_tests.sh`, `run_hook_override_tests.sh`, `run_keyboard_mod_ownership_tests.sh`, `run_owned_keycode_tests.sh`, `run_held_action_tests.sh`, `run_layer_ownership_tests.sh` |
| Macros, Raw HID, QMK contracts | `run_qmk_contract_checks.sh`, `run_compat_boundary_tests.sh`, `run_action_lifecycle_tests.sh`, `run_qmk_portable_profile_tests.sh`, `run_macro_payload_tests.sh`, `run_profile_keymap_macros_runtime_tests.sh`, `run_profile_wire_v1_tests.sh` |
| Shared runtime and tracing | `run_runtime_init_order_tests.sh`, `run_runtime_debug_tests.sh`, `run_runtime_trace_tests.sh` |

Each flow in [architecture](architecture/README.md) ends with the runners that
hold it. Then, as they apply:

1. Authored keymap, combo, macro or lighting data changed:
   `run_real_profile_validation_tests.sh`. A `MACROS()` change regenerates
   the profile's `authored_macro_bank.h` with
   `NOAH_WRITE_AUTHORED_MACRO_BANK=1 sh tests/host/run_real_profile_validation_tests.sh`
   (pass the keymap path for another profile); validation refuses a stale copy.
2. Runtime wiring, source lists, compat surfaces or header boundaries changed:
   `run_feature_gate_compile_tests.sh`, and `run_source_manifest_tests.sh` for
   source lists.
3. Always, before handing back: `run_all_host_tests.sh`.
4. Then the flashable pair: `sh tools/build-firmware-pair.sh`. A plain
   `qmk compile` builds a generic image without the live-profile owner
   (D-F35), which is not the firmware anyone flashes. From a task worktree set
   `QMK_ROOT` to the QMK checkout and `BUILD_ROOT` to an output folder.

The vault's `_agents/bin/verify` runs steps 1–4 as they apply, sets both
variables and records the pass `land` reuses. Docs-only changes skip the host
suite and the pair; `git diff --check` still runs. Python-only tooling changes
run their own checks and skip the pair unless they change generated firmware
inputs, source manifests, build wiring or authored profile data.

### How host tests run

Host tests run in the build image `tools/build-image` names, the one the pair
is built in and CI runs (D-F05), with its GCC, sanitizers, Python, Node and
ripgrep. Every `tests/host/run_*.sh` sources `tests/host/in_build_image.sh`
first: started on the Mac, it reruns the runner in that image with Docker,
without network; inside the image (CI, or one runner calling another) it does
nothing. Docker must be running; nothing falls back to the Mac's compiler, so
a local pass and CI's are the same compiler. The image is pulled once, on a
Mac's first run.

Compiling through Docker's file sharing with macOS is slow, so the runner
reads copies at the original paths (`tools/qmk-copies.sh`): the checkout is
copied into memory each run, and QMK into a Docker volume once per commit
(`charybdis-host-qmk-<commit>`; making one removes all but the three newest).
A QMK with local changes, untracked files or submodules off their commits gets
a volume named by its commit and a hash of those changes, shared by the host
tests and the pair build of a run. `tools/build-firmware-pair.sh` compiles the
same copies and writes only the pair into `BUILD_ROOT`, never BK's output
path, so pair builds in several worktrees, and a pair build beside the host
suite, run at once without a lock. A BK checked out inside the firmware
checkout is used in place by the host tests; the pair build refuses it outside
the image. A runner that writes into the checkout or QMK
(`run_all_profile_compile_tests.sh`, the stack-budget checks, real-profile
validation with `NOAH_WRITE_AUTHORED_MACRO_BANK=1`) works on the checkouts
themselves. `NOAH_BUILD_IMAGE=<image>` runs tests or builds in an unpinned
image, such as one `tools/make-build-image.sh` built; output and the pair's
note say so.

Undefined behaviour halts the test that reaches it
(`UBSAN_OPTIONS=halt_on_error=1`). QMK's directories are on `C_INCLUDE_PATH`
as system headers, so a warning inside QMK cannot fail a build while warnings
in our sources and test shims stay errors.

`run_all_host_tests.sh` runs its checks side by side, `NOAH_HOST_TEST_JOBS` at
a time (default: one per CPU the image sees), each in its own temporary
directory, prints output in list order and repeats each failure under its
name. `NOAH_HOST_TEST_JOBS=1` runs them one at a time; `NOAH_HOST_TEST_TIMES=1`
lists the slowest. The split frame CRC harness, which times a real thread
against a 20 ms deadline, runs alone after the others.

### Per-half resource acceptance

Run the vault's `verify` with `NOAH_RESOURCE_CHECKS=yes` to build an
instrumented pair in the pinned image. The pair builder checks each half's
linked SRAM accounting and both reviewed-path stack manifests, and keeps that
half's ELF, map, flags and gate reports beside the `_resources` pair, so one
half's evidence never stands for the other's. Every gate runs on both halves;
if any failed, the build fails naming each failed report. Build the ordinary
pair again without the option before handing firmware over: instrumentation
changes code generation. How to read and report the numbers is the
[resource policy](spec/resource-policy.md); none of these checks establish
allocator or stack high-water, or timing, on the keyboard.

## CI and releases

CI runs for releases, not for development (D-F06). A release's `dev` → `main`
pull request must pass four checks, including for administrators:
`Promotion from dev` (`.github/workflows/main-promotion.yml`: only this
repository's `dev`, with a merge tree identical to it), `Host suite (GCC)`,
`Pair build` (the side-specific pair at the pinned BK commit, kept as the
`firmware-pair` artifact with its SHA-256, and `firmware-contract.json`) and
`Agreement with Ark main` (Ark's agreement check against this firmware). The
vault's `release` prepares the release and opens that pull request;
`release --publish` waits for the checks, re-checks agreement against Ark's
`main` as it is then, fast-forwards and tags BK's released line at the pin,
merges for exactly the prepared `dev` head, tags it and publishes exactly the
pair `Pair build` made; nothing is rebuilt at the tag. Rerun it if
interrupted: it resumes. Promotions merge as merge commits, so development
history stays reachable; task pull requests into `dev` are squashed, and a
release's notes are its promotion's commit message.

The release workflow (`.github/workflows/ci.yml`) also runs the host suite nightly on
`dev`, never blocking, at the pin and against the BK dev head as an early
warning; `Pair build` can be started on demand for any commit. Scheduled runs
start from `main`, so a workflow change takes effect once a release promotes
it. Every CI job that builds or tests runs in the image and sets
`NOAH_IN_BUILD_IMAGE=1`. `dev`, `main` and published `v*` tags cannot be
force-pushed, moved or deleted on GitHub; the local pre-push hook refuses
direct pushes to `dev` and `main` and accepts only annotated release tags on
the released line.

## The BK pin

The UF2 is this userspace and BK compiled together, so `qmk-pin.json` names
the exact BK commit: a published commit on BK's
`noah-userspace-contracts-dev` (D-F04). `tools/build-firmware-pair.sh` refuses
a BK checkout that is not at the pin, has tracked changes, different or
uninitialized submodules. `NOAH_ALLOW_UNPINNED_QMK=1` builds against BK as it
is, recorded in the pair's note as "not the pin"; VS Code's build tasks set
it, and verify uses it for trials and BK changes. Such a pair is never
released: a release publishes CI's pair, built at the pin, only when it equals
a filed pair byte for byte.

BK runs no CI of its own: a BK change reaches users only through a re-pin,
tested here. Needing a BK change means landing its pull request on
`noah-userspace-contracts-dev` first (BK lands merge commits, so a pinned
commit stays valid), then re-pinning in the task that uses it with
`sh tools/pin-qmk.sh [REV]` (default: the published BK dev head). It lists the
BK commits since the old pin by area: hooks and core, keycode numbering, Raw
HID, RGB matrix, the Charybdis board and submodules. Keycode, Raw HID or RGB
changes need an Ark follow-up. This firmware does not build BK's VIA code
(D-F31).

## The build image

`tools/build-image` names the image every pair is built and every host test
runs in, by its multi-arch index digest; every workflow's `image:` equals it.
A pair is reproducible: QMK's version stamps are fixed (`SKIP_VERSION`, so the
`QK_VERSION` keycode prints placeholders) and source paths are mapped, so one
userspace commit and BK pin give the same bytes on any machine, and each
pair's note records its inputs, image and SHA-256.

Build and publish a new image only with `sh tools/make-build-image.sh <tag>`;
its header says what it checks before it pushes and pins. To move to a newer
official QMK image, read its index digest (`docker buildx imagetools inspect
ghcr.io/qmk/qmk_cli:latest`, the top-level `Digest`), set it as `QMK_CLI` in
`tools/build-image.dockerfile` and run the script with a new tag. If it
reports that the pair changed, the compiler changed: run the resource gates and
record the memory effect in the commit that pins the image
([resource policy](spec/resource-policy.md#running-the-gates)). If the official
image moves Python to a new minor version, the vault's `vpython` moves first.

## The firmware contract

`sh tests/host/run_contract_probe.sh` states what this firmware promises a
client: the exact Profile Wire capability pages the keyboard answers once its
live-profile owner is up (versions, capabilities, capacities, the action-ABI
and compiled-default digests), the BK pin, and the SHA-256 of every fixture
under `tests/fixtures`. It builds them from the code the keyboard runs: the
Raw HID channel and the probe share `lib/compat/qmk_profile_capabilities.h`,
the digests come from the compiled-defaults code the owner uses, and the bytes
from the Profile Wire encoder. `--output FILE` writes outside the container,
so a client can name any path. The host suite runs it; a release's
`Pair build` publishes it as `firmware-contract.json`. Firmware only states
the contract; the client's agreement check judges it.

## Independence from Ark

Firmware builds, the full host suite and diagnostics need no Ark checkout
(D-F53). Firmware owns its regression fixtures and C contracts: the populated
profile, pointing and macro-size vectors live in
`tests/fixtures/client-regression/` with producer and checksum records, and
`run_firmware_client_independence_tests.sh` fails if a tracked file outside
prose, `docs/`, `tests/fixtures/` and `measurements/` names the app. CI's
release gate, `Agreement with Ark main`, is the one exemption (D-F06).

Independence runs one way: Ark reads firmware. These paths are Ark's inputs,
and changing them fails no firmware gate:

- Ark's integration runners (`tests/integration/`, `npm run test:compat`)
  compile the probes
  `tests/host/{profile_compiled_defaults_v1,profile_pd_v1,qmk_portable_editor,qmk_portable_profile}_test.c`
  and Ark's own probes, with sources under `users/noah/lib/`, headers under
  `tests/host/include/`, the keymap and `tests/host/noah_host_qmk_env.sh`, and
  read `tests/fixtures/compiled_profile_pd_v2.fixture` and the old-format
  rejection vector `tests/fixtures/client-regression/portable.bin.pd5`.
- Ark's agreement check runs `tests/host/run_contract_probe.sh`.
- Ark's `upstream/` snapshot pins the wire fixtures under `tests/fixtures/`,
  the specs under `docs/spec/` and `GLOSSARY.md`.

A change that alters a wire format, fixture bytes or a spec's contract, or
moves or rewires those probes and sources, is complete here once firmware's
gates pass; name the Ark follow-up in the handoff: refresh Ark's `upstream/`
snapshot from the landed firmware commit, update its codecs or integration
recipes, then run `npm run test:compat` from Ark with explicit paths. Joint
protocol work runs that bridge before merging either side.

Local work needs no remote: firmware gates read only this checkout, and Ark's
bridge compares local checkouts by path. Publishing does. Ark pins a firmware
commit, and an Ark pin change lands only once that commit is on this
repository's published `dev`. Squash-merging replaces a branch's commits, so
an Ark pin to a task-branch commit must move to the landed one. Land
cross-repository work bottom-up: BK, then firmware, then Ark.

Hardware diagnostics own their Node dependency in `tools/package.json`; install
it with `npm ci --prefix tools` before using
`tools/capture-split-diagnostics.cjs`. Neither the firmware build nor the C
host tests need it.

## Split transport comparison builds

Both halves use a fixed 460,800-baud split link with frame CRC and coalesce
repeated lighting-activity messages by default (D-F52); flash both halves from
the same pair. The removed `NOAH_SPLIT_BAUD` selector fails the build.
Comparison pairs, each marked in its artifact names:

| Variable | Pair | Suffix |
| --- | --- | --- |
| `NOAH_SPLIT_ACTIVITY_COALESCE=no` | every activity message sent | `_no_activity` |
| `NOAH_SPLIT_CRC=no` | no frame checksum; refuses a half built with it at the handshake | `_no_crc` |
| `NOAH_SPLIT_DIAGNOSTICS=yes` | the ten-second split transaction recorder | `_diagnostic` |
| `NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS=yes` | the pointing-cadence recorder with per-stage loop timing | `_cadence` |

```sh
NOAH_SPLIT_ACTIVITY_COALESCE=no sh tools/build-firmware-pair.sh
```

After flashing a measurement pair, `node tools/capture-split-diagnostics.cjs`
reads both recorders. See [the activity contract](spec/split-activity-sync.md)
and follow [the capture procedure](../measurements/pointing-cadence/README.md)
so captures compare; recorded sets live under
[`measurements/`](../measurements/README.md), never edited.
