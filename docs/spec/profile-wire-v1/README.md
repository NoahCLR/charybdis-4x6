# Profile Wire

Profile Wire is the protocol a client speaks to the keyboard over Raw HID to
read, edit, save and apply the profile. The master half answers it; the slave
half is reached only through the master half over the
[split protocol](../profile-split-v1.md). This folder holds the protocol in
layers, from the 32-byte report up to each domain's payload. How a save
reaches durable storage on both halves is in the
[logical profile transaction](../logical-profile-transaction-v1.md); where the
profile lives and which formats a half accepts is in the
[device-resident profile](../device-resident-profile.md); the complete backup
is the [portable profile](../portable-profile-v1.md). The files and the C
modules that implement this protocol keep their `v1` names.

## Conventions

Every multi-byte integer is little-endian. Every reserved byte and every
reserved flag bit is zero, in requests and in replies; a request with a
nonzero reserved byte is refused as malformed. Every profile length and
offset is 32 bits wide, on the wire and in storage (D-F28,
[`0028`](../../adr/0028-new-base-32-bit-profile-lengths.md)). The firmware's
encoders write one canonical representation; its decoders refuse wrong
ordering, padding and unused data.

Held by: `tests/host/run_profile_wire_v1_tests.sh` (`profile_wire_v1_test.c`, `qmk_profile_channel_test.c`), `tests/host/run_profile_blob_v1_tests.sh` (`profile_blob_v1_test.c`).

## Layers and files

| Layer | What it covers | File |
| --- | --- | --- |
| Transport | Raw HID reports, the read envelope, capability, status and payload pages, every value id | [hid-channel.md](hid-channel.md) |
| Results | Read status codes, candidate admissions and error ids, live-edit results, busy reasons | [errors.md](errors.md) |
| Apply | Candidate begin, chunk, reuse, stream, validate, commit, abort and its status pages | [candidate-transfer.md](candidate-transfer.md) |
| Live edits | Session and revision, splices, Save, Revert, edit mode, Restart | [live-edits.md](live-edits.md) |
| Profile blob | Blob header, domain envelope, domain registry, action encoding, digests | [blob-and-digests.md](blob-and-digests.md) |
| Domains | Key behaviours `0x20`, combos `0x30`, settings `0x40`, keymap `0x60`, macros `0x70` | [key-behavior-domain.md](key-behavior-domain.md), [combo-domain.md](combo-domain.md), [settings-domain.md](settings-domain.md), [keymap-domain.md](keymap-domain.md), [macro-domain.md](macro-domain.md) |
| Domains elsewhere | Lighting `0x10`, pointing `0x50` | [rgb-domain-v1.md](../rgb-domain-v1.md), [pd-mode-domain-v1.md](../pd-mode-domain-v1.md) |

Held by: no test (an index).

## Version identity

A keyboard identifies itself by its capability page alone (D-F31,
[`0031`](../../adr/0031-via-is-not-supported.md)); there is no separate
protocol-version or firmware-version read. Capability page 0
([hid-channel.md](hid-channel.md#capability-pages--get-0x01)) carries:

| Identity | Value in this firmware | Where |
| --- | --- | --- |
| Profile Wire version | major `2`, minor `0` | page 0 bytes 2–3 |
| Profile schema version | major `4`, minor `2` (D-F62: pointing v4; D-F26: settings v10) | page 0 bytes 4–5, and blob header bytes 4–5 |
| Capability layout | `3`, three pages | page 0 bytes 0–1 |
| Firmware version | `NOAH_FIRMWARE_VERSION`, never zero | page 0 bytes 17–20 |
| Action-ABI digest | see [blob-and-digests.md](blob-and-digests.md#action-abi-digest) | page 0 bytes 13–16 |
| Compiled-default digest | see [blob-and-digests.md](blob-and-digests.md#digests-and-checksums) | page 0 bytes 21–24 |

Everything this base does is implied by its Profile Wire and schema versions.
The feature word holds only what a build varies by, never a step in the
firmware's history; a later capability raises the minor version (D-F28). The
store format, `6`, is implied by the Profile Wire version and never travels.
The firmware accepts only the formats it writes: a blob, candidate or saved
profile in another schema or store format is refused, never translated
(D-F10, [`0010`](../../adr/0010-firmware-accepts-only-current-formats.md)).

Held by: `tests/host/run_profile_wire_v1_tests.sh` (`profile_wire_v1_test.c`, `qmk_profile_channel_test.c`), `tests/fixtures/profile_wire_v1_reads.fixture`.

## Fixtures

The golden vectors under `tests/fixtures/` are part of the contract. The
firmware's own runners read them, and `tests/host/run_contract_probe.sh`
publishes the SHA-256 of every file there beside the capability pages, so a
client can tell which vectors this firmware answers to.

| Fixture | What it holds | Read by |
| --- | --- | --- |
| `profile_wire_v1_reads.fixture` | Capability pages 0–2 and status pages 0–1, request and response, 32 bytes each | `run_profile_wire_v1_tests.sh` |
| `profile_blob_v1.fixture` | The empty blob, a two-domain blob with their FNV-1a and CRC32, one action per kind | `run_profile_blob_v1_tests.sh` |
| `profile_candidate_v1.fixture` | Candidate begin, chunk, validate, commit and abort requests, their admissions, candidate status pages 0 and 3 | `run_profile_candidate_transaction_tests.sh` |
| `profile_validator_v1.fixture` | A valid lighting and key-behaviour profile with its length, checksums, domain mask and domain offsets | `run_profile_validator_v1_tests.sh` |
| `compiled_profile_pd_v2.fixture` | The authored profile as compiled: length, CRC32, FNV-1a, action-ABI digest and every byte | `run_profile_compiled_defaults_v1_tests.sh` |
| `populated_profile_v4.fixture` | A populated seven-domain profile | `run_profile_domain_owner_round_trip_tests.sh`, `run_profile_working_copy_tests.sh`, `run_profile_working_sync_tests.sh` |
| `maximum_profile_v4.fixture` | Every table at its simultaneous maximum, generated by `tests/host/make_maximum_profile.py` | `run_profile_domain_owner_round_trip_tests.sh`, `run_profile_split_reconciler_tests.sh` |
| `key_behavior_domain_v1.fixture` | Key-behaviour domain `0x20` version 2 vectors | `run_key_behavior_domain_v1_tests.sh` |
| `combo_domain_v3.fixture`, `combo_output_behaviors_v1.fixture` | Combo domain `0x30` version 3 payloads | `run_active_profile_combo_runtime_tests.sh` |
| `combo_readback_v3.fixture` | Native combo readback (GET `0x06`) pages | `run_combo_readback_tests.sh` |
| `rgb_domain_v1.fixture`, `rgb_domain_v4.json` | Lighting domain `0x10` vectors | `run_profile_rgb_v1_tests.sh`, `run_profile_rgb_v4_tests.sh`, `run_active_profile_rgb_runtime_tests.sh`, `run_rgb_layer_render_tests.sh` |
| `pd_mode_domain_v4.json` | Pointing domain `0x50` version 4 vectors | `run_profile_pd_v1_tests.sh`, `run_pd_profile_integration_tests.sh`, `run_profile_compiled_defaults_v1_tests.sh` |
| `pd_mode_domain_v1.json` | A test-only snapshot of the compiled pointing modes | `run_pointing_mode_handlers_tests.sh` |
| `folded_keyboard_profile.json` | Noah's profile in logical form, which the compiled defaults must reproduce | `run_profile_compiled_defaults_v1_tests.sh` |
| `host_layouts_v1.json` | The host layout catalogue | `tools/host_layouts/build.py`, `run_host_layout_tests.sh` |
| `client-regression/` | Frozen pointing and macro corpora and one earlier-format profile the firmware refuses; see its `README.md` | `run_profile_pd_v1_tests.sh`, `run_macro_program_size_tests.sh`, `run_profile_compiled_defaults_v1_tests.sh` |

Held by: the runners named in the table.
