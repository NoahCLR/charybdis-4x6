# Plan: CI host tests and local hooks

Parked until firmware and Live both have their remotes. Nothing here is
implemented. Live's half is `docs/plans/ci-and-hooks.md` in the Live repository.
When done, fold the lasting rules into `AGENTS.md` and
[live compatibility](../architecture/live-compatibility.md), then delete this
file (D-L07).

Goal: every firmware push runs the gates agents run locally, and cheap failures
are caught before a commit rather than in review.

## Where things stand

CI (`.github/workflows/build_binaries.yaml`) only compiles, against the QMK fork
branch `noah-userspace-contracts`. The full host suite runs only when an agent
runs it. There are no hooks. Measured locally: the full host suite takes about
three minutes; the independence guard and `profile_introspect.py --check`
together take under a second.

Hooks are a convenience: they only run in a clone where someone installed them,
and `--no-verify` skips them. CI is the enforcement.

## Steps

1. **Host suite in CI.** Add a job beside the build: check out the QMK fork at
   `noah-userspace-contracts` as the build job does, set `QMK_ROOT`, install
   `ripgrep` (guards that shell out to `rg` pass vacuously without it) and a C
   compiler with sanitizer support, then run `sh tests/host/run_all_host_tests.sh`.
2. **Hooks with prek.** Add `.pre-commit-config.yaml`; `prek` and `pre-commit`
   both read it, prek is preferred for its single binary. Pre-commit only, all
   under a second:
   - trailing whitespace and end-of-file, excluding `measurements/` (captures are
     never edited) and binary fixtures;
   - `sh tests/host/run_firmware_client_independence_tests.sh`;
   - `python3 tools/profile_introspect.py --check` when one of its authored
     inputs is staged.
   No pre-push hook: the three-minute suite belongs in CI.
3. **Agent rules.** In `AGENTS.md`: run `prek install` once per clone (hooks sit
   in the shared `.git/hooks`, so every worktree gets them); never commit with
   `--no-verify`; fix a failing hook rather than bypass it.

## Acceptance

- A planted failing host test fails the CI job, and a clean push passes it.
- A staged whitespace error, a staged `charybdis-live` reference and a stale
  introspection output each block a commit.
- A commit in a task worktree runs the same hooks.

## Open questions

- Whether the sanitizer runners and QMK-contract checks work unchanged on
  GitHub's Ubuntu runners.
- Whether to enforce `clang-format`; first confirm the tree is clean under it.
