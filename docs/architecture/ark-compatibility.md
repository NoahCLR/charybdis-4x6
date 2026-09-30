# Firmware independence and Ark integration

Firmware builds and its full host suite require no Charybdis Ark checkout.
The former in-tree app has been removed. There is no Ark-root selector or fallback
in firmware runners. The five C test runners use firmware-owned fixtures; PD,
macro-size and populated profile regression vectors live under
`tests/fixtures/client-regression/`, with producer revision and checksum records.
Portable editor/profile C tests assert their own emitted page contract.
`run_firmware_client_independence_tests.sh` guards this boundary and fixture
integrity. Firmware tooling checks no longer run the app's suite.

Ark owns `tests/integration/` and its `npm run test:compat` command. Those five
runners compile probes from an explicitly selected firmware checkout and compare
them with the selected Ark codecs. Their build recipes must follow firmware
source/build changes. Firmware does not invoke or require these runners, even
for its ordinary full-suite CI. Protocol changes should document the changed
contract and retained compatibility; Ark tests that contract before merging
its corresponding implementation. A firmware-only developer can complete the
firmware gates without installing Ark. Joint work runs both suites and Ark's
optional cross-repository integration gate.

## What Ark consumes

Independence runs one way: firmware never reads Ark, but Ark reads firmware.
These firmware paths are Ark's inputs, and changing them does not fail any
firmware gate:

- Ark's `tests/integration/` runners compile the probes
  `tests/host/{profile_compiled_defaults_v1,profile_pd_v1,qmk_portable_editor,qmk_portable_profile}_test.c`
  and `tests/host/macro_program_size_probe.c` with sources under
  `users/noah/lib/{profile,macro,action,compat}/`, headers under
  `tests/host/include/`, and `tests/host/noah_host_qmk_env.sh`.
- Ark's `upstream/` snapshot pins wire fixtures under `tests/fixtures/` and the
  wire, domain, transaction and resource specs under `docs/architecture/`.

A firmware change that alters a wire format, fixture bytes or a spec's contract,
or that moves or rewires those probes and sources, is complete on the firmware
side once firmware's own gates pass. Name the Ark follow-up in the handoff:
refresh Ark's `upstream/` snapshot from the committed firmware revision, update
its codecs or integration recipes, then run `npm run test:compat` from Ark with
explicit paths. For joint protocol work, run that bridge before merging either
side.

## Local and published revisions

Local work needs no remote in either direction: firmware gates read only this
checkout, and Ark's bridge compares local working copies by path. Publishing
does. Ark pins a firmware commit, and before pushing a pin change it requires
that commit to be on this repository's remote `dev`. A squash or rebase merge
replaces a branch's commits, so an Ark pin to one of them must move to the
merged commit; say so in the merge handoff. Likewise, firmware builds exactly
the BK commit `qmk-pin.json` names, so push that commit to the fork's
`noah-userspace-contracts-dev` before pushing firmware that pins it: CI checks
BK out at the pin, and the push hook refuses an unpublished pin.

## Diagnostics

Firmware diagnostics owns its Node dependency in `tools/package.json` and its
lockfile. Use `npm ci --prefix tools` for hardware diagnostics; neither firmware
compilation nor its C host tests need that native HID installation. No firmware
test or diagnostic tool resolves code or dependencies through the app.

## Release verification

Firmware release CI runs this repository's full host suite and builds both
physical-half images against the corresponding QMK tag. It uploads the pair to
a draft release. The shared release command tests an immutable three-repository
candidate, retains its compatibility report and build hashes, and waits for
both firmware and Ark's tagged CI before publishing. Firmware's own build and
host runners remain independent of the client. These gates do not substitute
for the interruption and keyboard acceptance procedures.
