# Developing this firmware

How this repository is developed, tested and released. What the firmware does
and how to configure it is in the [README](../README.md) and the
[firmware guide](GUIDE.md).

For the editor's build, test and compilation-database tasks, see the
[VS Code workflow](../.vscode/README.md). Charybdis Ark's editor tasks now belong
to its independent sibling repository.

## Local repositories and worktrees

On Noah's machine, the main checkouts are:

- Firmware: `/Users/noah/dev/charybdis/charybdis-4x6`.
- Ark: [NoahCLR/charybdis-ark](https://github.com/NoahCLR/charybdis-ark)
  (`/Users/noah/dev/charybdis/charybdis-ark` locally).
- Upstream QMK build dependency: `/Users/noah/dev/charybdis/bastardkb-qmk`
  ([NoahCLR/bastardkb-qmk](https://github.com/NoahCLR/bastardkb-qmk):
  development on `noah-userspace-contracts-dev`, released line
  `noah-userspace-contracts`, legacy upstream mirror `main`).
- Work queue: `/Users/noah/dev/charybdis/charybdis-notes`, an Obsidian vault
  (private [NoahCLR/charybdis-notes](https://github.com/NoahCLR/charybdis-notes))
  holding notes, tasks, active plans and keyboard checks for all three
  repositories. Its `AGENTS.md` says how a task is refined and picked up.

Branches: `dev` is the trunk. Each task branches from `dev` in its own
worktree, is verified locally and reaches `dev` through a pull request, which
the vault's `land` merges (squash) for exactly the verified commit; no CI runs
for that merge, because local verification is the gate. GitHub accepts changes
to `dev` only through pull requests, and it is never pushed directly. `main` is the released
line and only moves when a release promotes `dev` into it. Each landed commit's
message ends with what verify ran, and the BK commit it was built with is the
one its own `qmk-pin.json` names. A firmware release is a date tag,
`vYYYY.MM.DD`, on this repository and on the BK commit it is built with; Ark
releases separately, and both release together only when the contract between
them changes. This repository is a GitHub
fork of Bastard Keyboards' userspace: push only to `NoahCLR/charybdis-4x6`, and
never push or open a pull request upstream. The clone's `gh` default and
pre-push hook enforce that.

Use the worktree assigned to your task. Run `git worktree list` in the relevant
repository to discover its other checkouts; do not assume the main checkout
contains another agent's branch. Relative sibling paths below describe the main
workspace layout and may not hold in a worktree. Select QMK explicitly with
`QMK_ROOT` for host tests and `QMK_HOME` for the QMK CLI, and set `QMK_USERSPACE`
to the firmware worktree being built.

Firmware agents own this checkout's C code, tests and firmware documentation.
Ark agents own the independent app checkout and its documentation; read its
`AGENTS.md` and `README.md` there. The former `tools/charybdis-live/` copy has
been removed; its source history remains in Git. Inspecting another checkout
is allowed;
editing it requires that scope in the task. Coordinate shared QMK build output
and keyboard access with other agents. Firmware tests and builds require no
Ark checkout; Ark owns the optional cross-repository integration tests.

## Tests and CI

CI runs for releases, not for development (D-F06). A release's `dev` → `main`
pull request runs the complete host suite with GCC in the QMK container, builds
the side-specific pair in the release image at the pinned BK commit
(`Pair build`, kept as the `firmware-pair` artifact with its SHA-256) and checks
agreement with the Ark release it will sit next to. The shared vault's
`release --publish` publishes exactly that pair; nothing is rebuilt at the tag.
If publication is interrupted, rerun the same command: it resumes. The same
workflow (`.github/workflows/ci.yml`) also runs nightly on `dev`, never
blocking, and `Pair build` can be started on demand to build any commit.

Development runs the host suite with macOS clang; CI runs it with GCC in the
QMK container on the promotion pull request and nightly. Host tests put QMK's directories on
`C_INCLUDE_PATH`, so both compilers treat QMK as third-party system headers: a
warning inside QMK cannot fail a build, while warnings in our own sources and
test shims remain errors.

The pair builder isolates QMK CLI configuration for the compiler and its code
generators, so saved `overlay_dir`/`qmk_home` values cannot redirect a task build
to the main checkout. Explicit keymap paths also override old symlinks inside
the QMK tree. Local `verify` records a pass for the tested tree, the pinned BK
commit and the build options, with the pair's SHA-256. `land` reuses it only
while those inputs match, merges the task's pull request for exactly the
verified commit, and files the pair with the build note beside it. The installed pre-push hook refuses direct pushes to `dev`
and `main`, and accepts only annotated release tags on the released line.

## The BK pin

The UF2 is this userspace and the BK fork compiled together, so this repository
names the exact BK commit it builds with in `qmk-pin.json`: a published commit
on the fork's `noah-userspace-contracts-dev`. Everything uses it:
`tools/build-firmware-pair.sh` refuses a BK checkout that is not at the pin or
has local changes, and CI's `Host suite (GCC)` and `Pair build` check BK out at
the pin. `NOAH_ALLOW_UNPINNED_QMK=1` builds against the BK checkout as it is,
printed as such and recorded in the pair's note as "not the pin": VS Code's
build tasks set it, because they are development builds of the checkouts you
have open, and verify uses it for trials and BK changes. Such a pair is never
released: a release publishes CI's pair, built at the pin, only when it equals
a filed pair byte for byte.
A non-required nightly job runs the host suite against the BK dev head and
reports how far it is ahead of the pin. The vault's `verify` checks BK out
at the pin itself, and its `land` refuses a pull request whose pin is not on
the published BK dev branch.

The BK fork runs no CI of its own (GitHub Actions stay off, so its upstream
workflows never run): a BK change reaches users only through a firmware re-pin,
which is tested here. Its released line `noah-userspace-contracts` moves only
by a release, to exactly the pinned commit.

Re-pin with `sh tools/pin-qmk.sh [REV]` (default: the published BK dev head).
It lists the BK commits since the old pin and what changed by area: hooks and
core, keycode numbering, VIA, RGB matrix, the Charybdis board and submodules.
Keycode, VIA or RGB changes need an Ark follow-up.

## The firmware contract

`sh tests/host/run_contract_probe.sh` states what this firmware promises a
client: the exact Profile Wire capability pages the keyboard answers once its
live-profile owner is up (versions, feature flags, capacities, the action-ABI
digest and the compiled-default digest), the BK pin, and the SHA-256 of every
fixture under `tests/fixtures`. It builds them from the code the keyboard
runs: the VIA channel and the probe share `lib/compat/qmk_via_profile_capabilities.h`,
the digests come from the compiled-defaults code the owner uses, and the bytes
from the Profile Wire encoder. The host suite runs it; a release's `Pair build`
produces it as `firmware-contract.json`, which the release publishes. Firmware only states the contract; the
client's agreement check judges it.

## Charybdis Ark

The app's current guide is `README.md` in the independent Ark checkout listed
under [local repositories](#local-repositories-and-worktrees) above; its `docs/`
directory owns app development guidance. Earlier in-tree guides remain in Git
history.
The direction, the decisions behind it, and what is deliberately left
undesigned are in
[`docs/LIVE_EDIT_APP_DIRECTION.md`](LIVE_EDIT_APP_DIRECTION.md).

Firmware builds and host tests do not require Ark. The independent Ark
repo owns the optional `npm run test:compat` integration gate for joint protocol
work. See [the independence contract](architecture/ark-compatibility.md).
Install this repo's diagnostics dependencies with `npm ci --prefix tools` before
using `tools/capture-split-diagnostics.cjs`; its HID dependency is independent
of Ark.

## Split transport comparison builds

The firmware coalesces split activity messages by default: it sends fewer
repeated lighting-activity messages to the other half while still reading its
keys on every scan. The split link always runs at QMK's default 230,400 baud; a
faster link garbled split messages and was removed (D-L43).

```sh
NOAH_SPLIT_ACTIVITY_COALESCE=no sh tools/build-firmware-pair.sh
```

builds the uncoalesced comparison pair, with `_no_activity` in its artifact
names. Every split frame carries a checksum by default, so a garbled message
is refused instead of shown; `NOAH_SPLIT_CRC=no` builds the comparison pair
without it (`_no_crc`). Always flash both halves from the same pair: halves
built with and without the checksum refuse each other at the handshake. For a measurement pair add `NOAH_SPLIT_DIAGNOSTICS=yes` (the ten-second
transaction recorder, `_diagnostic`) and `NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS=yes`
(the pointing-cadence recorder with per-stage loop timing, `_cadence`), then run
`node tools/capture-split-diagnostics.cjs` after flashing; it reads both. Build with Homebrew Python 3.12+ on PATH for the profile tooling.
See [the activity contract](architecture/split-activity-sync.md), and follow
[the capture procedure](../measurements/pointing-cadence/README.md) so captures
compare; recorded sets live under [`measurements/`](../measurements/README.md).

## Timing compatibility

Handled keys preserve physical gesture timing across combo/tapping buffering;
authored layer-tap behaviours use one tap/hold decision. See the
[timing contract](INTERACTION_MODEL.md#physical-gestures-and-buffered-delivery).
This requires the paired QMK fork changes and an Ark client recognizing Profile
Wire feature bit 17. The current layout and timing defaults are unchanged.

Runtime-owned tapping now also covers authored MT/OSM rows and intrinsic TT/OSL
keys (Profile Wire bit 18). Keys pressed while such a key is undecided wait for
its tap or hold, as QMK's tapping does, so rolls keep their order. Native
unhandled dual-role keys remain native. The
[release timing contract](INTERACTION_MODEL.md#release-intervals-and-timing-advice)
also defines the impossible release interval Ark reports.

The unified gesture ownership plan (in the work-queue vault)
explains why native LT/MT/OSM still exist and the proposed migration to one
classifier, including combo-output behaviours as a required acceptance case.

## Open release gates

Physical migration, power-interruption, pointing cadence and stack high-water
acceptance remain release gates; see the
[PD-mode domain contract](architecture/pd-mode-domain-v1.md#hardware-acceptance).
The gesture timing investigation (the Button 3 double-hold failure,
combo/tapping interactions and the current-layout acceptance plan) is tracked in
the work-queue vault as the plan *Gesture timing and combo arbitration*.

## Protected main promotions

`main` moves only by the shared vault's `release`, so every `main` is a
released, tested stack. GitHub `main` requires a pull request and four checks,
including for administrators, all run on the promotion pull request itself
(D-F06): `Promotion from dev`, `Host suite (GCC)`, `Pair build` and
`Agreement with Ark main`. `Promotion from dev` accepts only this repository's
`dev` branch and a merge tree identical to that branch. Force pushes and
deletion are blocked. Promotions merge as merge commits, so the promoted
development history stays reachable; task pull requests into `dev` are squashed.

`release` prepares a release: it decides from Ark's agreement check whether
this repository releases alone, before Ark or together with it, requires the BK
pin in `qmk-pin.json` to be on the BK fork's development branch and not behind
its released line, drafts the notes and opens the `dev` → `main` pull request.
`release --publish` waits for the required checks, re-checks agreement against
Ark's `main` as it is then, fast-forwards and tags the BK fork's released line
at the pin, merges the pull request for exactly the prepared `dev` head, tags it
and publishes the GitHub release with the pull request's `Pair build` files and
their SHA-256. Direct `main` pushes are rejected by the local hook as well.
Normal task development reaches `dev` through task pull requests.

CI does not run on `dev` or on pull requests into it: local verification is the
gate there. Nightly, never blocking, it runs the host suite on `dev` and against
the BK dev head. Scheduled runs start from `main`, so they check out `dev`
explicitly and take effect once a release has promoted the workflow.

`dev` cannot be force-pushed or deleted on GitHub and accepts changes only
through pull requests, and published `v*` release tags cannot be moved or
deleted (rulesets without bypass). Merge commits take the pull request's title
and body, so a release's notes are its promotion's commit message.
