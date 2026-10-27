# Charybdis firmware — agent instructions

Production firmware for the Charybdis 4x6: a QMK userspace runtime, built
against the BK fork, that keeps its profile on the keyboard. Charybdis Ark is
a client of the keyboard, never of this repository.

## How a change is made

Start with `git status --short`, and never reset, stage or commit work you
did not write.

Only Noah works here. His private notes vault, `charybdis-notes`, sits beside
the main checkout (from any worktree:
`"$(dirname "$(git rev-parse --path-format=absolute --git-common-dir)")/../charybdis-notes"`)
and holds the work queue and the tools every change goes through. If it is
there, read its `AGENTS.md` before your first change in a session: **Every
agent, wherever it starts** and **Branches, landing and pushing**. It never
overrides this file.

Without the vault: branch from `dev` as `<type>/<slug>`, run the checks below,
and open a pull request into `dev` on `NoahCLR/charybdis-4x6`. Never push
`dev` or `main` directly, and nothing goes upstream to Bastard Keyboards.
Noah lands and releases.

## Checks

Runners are `sh tests/host/<runner>` and run in the build image (Docker
running). Run [the area's runners](docs/development.md#checks) while you work,
then:

1. Authored profile data changed: `run_real_profile_validation_tests.sh`.
2. Wiring, source lists, compat or header boundaries changed: `run_feature_gate_compile_tests.sh`.
3. Always `run_all_host_tests.sh`, then the pair, `sh tools/build-firmware-pair.sh`
   (a plain `qmk compile` lacks the live-profile owner, D-F35).

The vault's `_agents/bin/verify` is the handoff gate and runs these as they
apply; docs-only changes run only `git diff --check`. Name the checks that ran
and any skipped, and for boundary work the contracts touched.

## Guardrails no check covers

- Edit only this repository unless the task names a sibling. Read QMK
  behaviour in the BK checkout. A BK change lands in BK first, then is
  re-pinned here with `sh tools/pin-qmk.sh`.
- A new source file is listed in `users/noah/source_manifest.mk` (held by
  `run_source_manifest_tests.sh`) and wired into any host runner or compile
  gate that mirrors it, in the same change.
- Never flash firmware or Apply a profile to verify a change; host tests and
  the pair never prove hardware acceptance.
- Read [`measurements/README.md`](measurements/README.md) before taking or
  quoting a measurement; never edit a capture file.
- A change to a wire format, fixture bytes, a spec's contract, or a probe Ark
  compiles names the Ark follow-up ([independence](docs/development.md#independence-from-ark)).
- A lasting decision is the next ADR (D-F<n>, [domain docs](docs/agents/domain.md));
  plans and status live in the vault, never in `docs/`, and pull requests
  never quote or link it.

## Resources

Read [the resource policy](docs/spec/resource-policy.md) before interpreting
or reporting RAM, allocator, EEPROM-cache or stack values. Report each half on
its own (270,336 bytes of physical SRAM per RP2040). Policy thresholds are
regression policies, never RAM headroom; the core-memory span is not free
memory; a stack-gate pass covers only its named paths.

## Where to look

- [`GLOSSARY.md`](GLOSSARY.md): the device and wire words.
- [`docs/adr/`](docs/adr/): decisions (D-F<n>); read those touching the area first.
- [`docs/spec/`](docs/spec/README.md): the contracts (Profile Wire, split, domains, storage).
- [`docs/architecture/`](docs/architecture/README.md): who owns what, layering, cross-module flows.
- [`CODING_STANDARDS.md`](CODING_STANDARDS.md): what review holds a change to.
- [`docs/development.md`](docs/development.md): worktrees, every check, the
  build image, the BK pin, CI and releases, Ark.

## Agent skills

### Issue tracker

Issues live in Noah's private notes vault beside the checkout, not on GitHub. See `docs/agents/issue-tracker.md`.

### Triage labels

The triage roles are the vault's `status` and `kind` fields. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: `GLOSSARY.md` and `docs/adr/` at the root. See `docs/agents/domain.md`.
