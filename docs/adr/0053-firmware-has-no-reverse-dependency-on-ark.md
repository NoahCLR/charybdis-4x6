# Firmware has no reverse dependency on Ark

Cited as D-F53. Was D-L46.

Firmware builds, host tests and diagnostics need no Ark checkout, and no
tracked firmware file outside prose and frozen fixtures names the app;
`run_firmware_client_independence_tests.sh` enforces both, exempting only
CI's release gate (D-F06). Firmware owns frozen regression vectors in
`tests/fixtures/client-regression/`; Ark owns cross-repository integration and
reads explicitly selected firmware checkouts. Contract:
[`ark-compatibility.md`](../architecture/ark-compatibility.md).
