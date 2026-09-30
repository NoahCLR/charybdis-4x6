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
worktree and is squash-landed back onto `dev` locally; `main` is the released
line and only moves when a release promotes `dev` into it. Landed commits, promotions
and release tags carry trailers naming the Ark and QMK commits they were tested
with, so `git log` answers what any build went with. A release is one date tag,
`vYYYY.MM.DD`, on firmware, Ark and the QMK fork together. This repository is a GitHub
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

Release CI runs the complete host suite before building the side-specific pair,
against the matching QMK tag. Both UF2 files are attached to a draft release;
the shared vault's `release VERSION --push` makes the releases public only after
firmware and Ark CI pass and both firmware assets are present. If publication
is interrupted, rerun the same command: it resumes the saved release commits,
even if `dev` has since advanced. Direct tag pushes leave the release in draft. The
third-party action that attaches the pair runs with write access, so it is pinned
to a reviewed commit hash; move the pin deliberately, after reading the new code.

Development runs the host suite with macOS clang; CI runs it with GCC in the
QMK container, on every `dev` push and pull request (`host_tests.yml`) as well
as before a release build. Host tests put QMK's directories on
`C_INCLUDE_PATH`, so both compilers treat QMK as third-party system headers: a
warning inside QMK cannot fail a build, while warnings in our own sources and
test shims remain errors.

The pair builder isolates QMK CLI configuration for the compiler and its code
generators, so saved `overlay_dir`/`qmk_home` values cannot redirect a task build
to the main checkout. Explicit keymap paths also override old symlinks inside
the QMK tree. Local `verify` records the full tested inputs and artifact checksums. `land`
reuses that result only while those inputs match, and files its recorded inputs
beside the firmware pair. The installed pre-push hook requires a matching local
verification receipt for new protected-branch commits and a stack certificate
for release tags; a hand-written `Stack-Tested` trailer is insufficient.

## The BK pin

The UF2 is this userspace and the BK fork compiled together, so this repository
names the exact BK commit it builds with in `qmk-pin.json`: a published commit
on the fork's `noah-userspace-contracts-dev`. Everything uses it:
`tools/build-firmware-pair.sh` refuses a BK checkout that is not at the pin or
has local changes (`NOAH_ALLOW_UNPINNED_QMK=1` allows it for a trial, printed
as such), CI's `Host suite (GCC)` and the release build check BK out at the pin,
and the release build refuses a BK tag that is not the pinned commit. A
non-required CI job (also nightly) runs the host suite against the BK dev head
and reports how far it is ahead of the pin. The vault's `verify` checks BK out
at the pin itself, and its push hook refuses a push whose pin is not on the
published BK dev branch.

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
from the Profile Wire encoder. The host suite runs it; every release build
attaches it as `firmware-contract.json`. Firmware only states the contract; the
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
released, tested stack. GitHub `main` requires a pull request and two checks,
including for administrators: `Promotion from dev` and `Host suite (GCC)`, the
host suite run on the promotion PR itself. `Promotion from dev` accepts only this
repository's `dev` branch and a merge tree identical to that branch. Force pushes
and deletion are blocked. GitHub PR merging uses merge commits; squash and rebase
merging are disabled so the promoted development history stays reachable.

`release VERSION` checks and tests without publishing: its preflight requires
the BK pin in `qmk-pin.json` to be on the BK fork's development branch and not
behind its released line, and its stack test runs exactly what `main` will hold
(this repository's `dev` at its BK pin). `release VERSION --push` then promotes
the BK fork's released line (fast-forwarded to the pin), this repository and the
client in that order: it publishes `dev`, opens or resumes the promotion PR,
waits until GitHub reports every required check passed, merges, and reconciles
local `main` to GitHub's merge identity. The merge message carries the `dev`
tip's stack and verification trailers. Retries resume the frozen preparation.
Direct `main` pushes are rejected by the local hook as well. Normal task
development still lands locally onto `dev`.

`dev` cannot be force-pushed or deleted on GitHub, and published `v*` release
tags cannot be moved or deleted (rulesets without bypass). Merge commits take
the PR's title and body, so a merge from the GitHub page carries the same
verification trailers as one made by `release`.
