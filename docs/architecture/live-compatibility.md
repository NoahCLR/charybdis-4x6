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

Firmware diagnostics owns its Node dependency in `tools/package.json` and its
lockfile. Use `npm ci --prefix tools` for hardware diagnostics; neither firmware
compilation nor its C host tests need that native HID installation. No firmware
test or diagnostic tool resolves code or dependencies through the app.
