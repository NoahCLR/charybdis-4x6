# Firmware independence and Live integration

Firmware builds and its full host suite require no Charybdis Live checkout.
The former in-tree app has been removed. There is no Live-root selector or fallback
in firmware runners. The five C test runners use firmware-owned fixtures; PD,
macro-size and populated profile regression vectors live under
`tests/fixtures/client-regression/`, with producer revision and checksum records.
Portable editor/profile C tests assert their own emitted page contract.
`run_firmware_client_independence_tests.sh` guards this boundary and fixture
integrity. Firmware tooling checks no longer run the app's suite.

Live owns `tests/integration/` and its `npm run test:compat` command. Those five
runners compile probes from an explicitly selected firmware checkout and compare
them with the selected Live codecs. Their build recipes must follow firmware
source/build changes. Firmware does not invoke or require these runners, even
for its ordinary full-suite CI. Protocol changes should document the changed
contract and retained compatibility; Live tests that contract before merging
its corresponding implementation. A firmware-only developer can complete the
firmware gates without installing Live. Joint work runs both suites and Live's
optional cross-repository integration gate.

## What Live consumes

Independence runs one way: firmware never reads Live, but Live reads firmware.
These firmware paths are Live's inputs, and changing them does not fail any
firmware gate:

- Live's `tests/integration/` runners compile the probes
  `tests/host/{profile_compiled_defaults_v1,profile_pd_v1,qmk_portable_editor,qmk_portable_profile}_test.c`
  and `tests/host/macro_program_size_probe.c` with sources under
  `users/noah/lib/{profile,macro,action,compat}/`, headers under
  `tests/host/include/`, and `tests/host/noah_host_qmk_env.sh`.
- Live's `upstream/` snapshot pins wire fixtures under `tests/fixtures/` and the
  wire, domain, transaction and resource specs under `docs/architecture/`.

A firmware change that alters a wire format, fixture bytes or a spec's contract,
or that moves or rewires those probes and sources, is complete on the firmware
side once firmware's own gates pass. Name the Live follow-up in the handoff:
refresh Live's `upstream/` snapshot from the committed firmware revision, update
its codecs or integration recipes, then run `npm run test:compat` from Live with
explicit paths. For joint protocol work, run that bridge before merging either
side.

## Local and published revisions

Local work needs no remote in either direction: firmware gates read only this
checkout, and Live's bridge compares local working copies by path. Publishing
does. Live pins a firmware commit, and before pushing a pin change it requires
that commit to be on this repository's remote `main`. A squash or rebase merge
replaces a branch's commits, so a Live pin to one of them must move to the
merged commit; say so in the merge handoff. Likewise, push a QMK fork commit to
`noah-userspace-contracts` before pushing firmware that needs it, since CI
builds against that remote branch.

## Diagnostics

Firmware diagnostics owns its Node dependency in `tools/package.json` and its
lockfile. Use `npm ci --prefix tools` for hardware diagnostics; neither firmware
compilation nor its C host tests need that native HID installation. No firmware
test or diagnostic tool resolves code or dependencies through the app.
