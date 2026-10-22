# Firmware pins its BK commit; `main` is a released stack

Cited as D-F04.

The flashed image is this userspace and the BK fork compiled together, so
firmware names the exact BK commit it builds with in `qmk-pin.json`, a
published commit on BK's `noah-userspace-contracts-dev`. It is a plain file,
not a submodule, and code enforces it: the pair build refuses any other BK, CI
and releases check BK out at the pin, and re-pinning (`tools/pin-qmk.sh`) is a
deliberate firmware change. Firmware and Ark agree by contract, not by commit:
firmware states what it answers (`tests/host/run_contract_probe.sh`) and Ark
judges it. `main` moves only by a release, after that exact stack has been
tested.
