# Stage 01 — Transport Prerequisites And Live Link

Status: software implementation complete; real-board/VS Code-host exit evidence pending

Implementation note: Review 19 Findings 1 and 4 are resolved in their owning
review. The injected fake, serialized request coordinator, lazy `node-hid`
adapter, read-only firmware channel, shared C/JavaScript golden reports, and
Studio compatibility UI have landed. Native enumeration ran successfully but
found no attached matching interface, so the hardware matrix remains open and
the project does not claim a proven board connection yet.

## Objective

Create a production-quality, read-only Live Link between Profile Studio and the
intended keyboard, then add the smallest versioned custom command surface needed
for capability and status discovery.

At the end of this stage, Studio knows what it is connected to and whether live
profile operations are compatible. It does not yet write bulk profile state.

## Entry Criteria

- Stage 00 exit criteria are recorded.
- D-009 through D-016 have accepted outcomes.
- Profile Wire capability and status bytes have golden fixtures.
- Review 19 Finding 1 is resolved in review/2026-08-16-review-02 with its
  required tests and compile evidence.
- Review 19 Finding 4 has an accepted effect/handler alignment decision if the
  Live Link touches that surface.

## Scope

In scope:

- isolated extension-host HID adapter;
- fake device adapter;
- device enumeration and identity;
- connect, disconnect, cancel, timeout, and serialized request handling;
- standard VIA protocol and firmware version reads;
- custom live-profile capability and status reads;
- explicit incompatible schema and capacity UI states;
- source, compiled-default, active, pending, committed, and peer digest/status
  presentation when available;
- transport diagnostics without exposing sensitive payload dumps by default.

Out of scope:

- candidate writes;
- persistent profile storage;
- RGB or behavior activation;
- source pull or push;
- dynamic combos or layers.

## Firmware Contracts

Add a keyboard-specific custom VIA channel or the Stage 00-selected equivalent.
Every request:

- is exactly 32 bytes at the Raw HID boundary;
- validates command, channel, operation, declared length, range, and reserved
  bytes before access;
- returns a stable structured status;
- is idempotent for reads;
- never performs long EEPROM, validation, or digest work inside an unsafe USB
  callback;
- reports protocol, schema, firmware, capacities, feature flags, and current
  generation identities.

Keep fork-specific constants and Raw HID assumptions in users/noah/lib/compat/.

## Profile Studio Contracts

Create an interface resembling:

- listDevices()
- connect(deviceId)
- request(frame, timeout, cancellation)
- disconnect()
- onDisconnect()

The concrete adapter and fake adapter implement the same contract. UI/webview
code receives semantic connection state, never HID handles.

Only one request may be outstanding per device. Unexpected replies, stale
transaction ids, timeout, disconnect, and contention are visible states.

## Likely Files

- tools/charybdis-profile-studio/extension.js or extracted transport modules
- tools/charybdis-profile-studio/package.json
- Profile Studio transport tests and fake harness
- users/noah/lib/profile/protocol/ initial capability/status module
- users/noah/lib/compat/ Raw HID and VIA contract additions
- users/noah/source_manifest.mk
- users/noah/lib/macro/via_macro_defaults.c only if hook composition requires a
  deliberate integration change
- QMK contract and feature-gate tests
- Profile Studio documentation

## Deliverables

- [x] Concrete HID adapter
- [x] Fake device adapter
- [x] Serialized request coordinator
- [x] Capability/status firmware handler
- [x] Golden cross-language fixtures
- [x] Connection and compatibility UI
- [x] Disconnect, timeout, and contention behavior
- [x] Diagnostics and error mapping
- [x] Updated source manifest and user-facing tooling docs
- [ ] Real-board enumerate/connect/unplug/replug/contention evidence

## Verification

Required targeted coverage:

- Profile Studio syntax/check suite
- transport adapter unit tests
- fake device happy path, malformed reply, timeout, cancellation, disconnect,
  stale reply, and incompatible capability tests
- QMK contract checks
- custom VIA command classification and malformed-frame tests
- feature-gate compile tests
- split tests if status includes peer state
- full host suite
- firmware compile
- fresh target resource gates if new static buffers or callback stack paths land
- real-board enumerate, connect, repeated query, unplug, replug, and VIA
  contention matrix
- git diff --check

## Exit Criteria

- Studio reliably identifies the intended board without depending only on a
  broad VID/PID match.
- A fake device can exercise all connection UI states in automation.
- Firmware rejects every malformed capability/status frame without mutation.
- Studio displays compatible/incompatible state and device generations.
- Repeated reads are deterministic and recover after disconnect.
- No candidate, EEPROM, RGB, or behavior write exists yet.
- Tests and real-board evidence are recorded in progress.md.

All software-only criteria are implemented. The first, fifth, and final
criteria remain unclosed until a matching physical board is available for the
recorded hardware matrix.

## Handoff

Stage 02 receives a stable transport adapter, frozen capability/status contract,
golden fixtures, and named maximum payload per frame. It must not reach into HID
details from storage or runtime code.
