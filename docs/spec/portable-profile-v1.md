# Portable profile

The portable profile is the keyboard's complete supported backup: one
canonical profile blob holding every domain the build supports, read from the
keyboard and restored by an Apply. This file says what a backup holds, how a
complete readback is put together, which fields name a layer, and when a
backup restores. The blob, its domains and digests are
[Profile Wire](profile-wire-v1/README.md)'s (each domain has its own file
there); where a half stores a profile and which profiles it admits is the
[device-resident profile](device-resident-profile.md)'s; how an Apply reaches
both halves is the [logical profile transaction](logical-profile-transaction-v1.md)'s.
Which authored values become profile data at all is the
[field classification](field-classification.md).

## What a backup is

A backup is a canonical profile blob
([blob and digests](profile-wire-v1/blob-and-digests.md)) holding every
domain in the build's supported domain mask (capability page 1 byte 12): on
this keyboard all seven, lighting `0x10`, key behaviours `0x20`, combos
`0x30`, settings `0x40`, pointing `0x50`, keymap `0x60` and macros `0x70`, in
registry order. It is at most 130,048 bytes, the blob ceiling. The profile is
the whole of it: nothing outside the blob is part of a backup.

A backup's identity is the blob's schema (header bytes 4–5), the domain
versions in its envelopes, and the action-ABI digest of the firmware it was
read from. The blob does not carry that digest; it is the connected
keyboard's (capability page 0 bytes 13–16), and restoring declares it in the
candidate's BEGIN ([candidate transfer](profile-wire-v1/candidate-transfer.md#begin)).

Held by: `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`, `validate_portable_import`: a complete profile validates with every known domain present), `tests/fixtures/populated_profile_v4.fixture`, `tests/fixtures/maximum_profile_v4.fixture`.

## A complete readback

A complete readback is the profile the keyboard is running, with no domain
left out:

1. Read the active profile with GET `0x21`
   ([live edits](profile-wire-v1/live-edits.md#the-active-profile-get-0x21)):
   the working copy when there are unsaved edits, otherwise the saved
   profile, otherwise the compiled defaults. GET `0x04` reads the saved
   profile alone ([HID channel](profile-wire-v1/hid-channel.md#payload-readback--get-0x04-and-0x05)).
2. For each supported domain the active profile omits, take that domain from
   the compiled defaults, GET `0x05`, and place it in registry order. The
   keyboard runs exactly that domain whenever the active profile omits it
   (each domain file says so for its own domain), so the result is what the
   keyboard does. An omitted domain never means "the destination's
   defaults": a backup carries every domain explicitly.
3. Read page 0 again after the data pages. The same session and revision
   (GET `0x21`), or the same generation and digest (GET `0x04`), prove the
   bytes are one profile; anything else means it changed during the read.

The settings in a backup are the stored settings, never QMK's live copies
(D-F32, [ADR](../adr/0032-live-edits-working-copy-and-save.md)). GET `0x07`
reads the active settings domain alone, or the compiled defaults' when none is
active ([settings domain](profile-wire-v1/settings-domain.md#settings-readback--get-0x07)).
A keyboard adjustment (DPI or a lighting key) shows in QMK at once and reaches
the profile only once it has become an edit of the working copy
([keyboard adjustments](profile-wire-v1/live-edits.md#keyboard-adjustments)).

Held by: `tests/host/run_qmk_profile_edit_channel_tests.sh` (`test_gets`), `tests/host/run_profile_working_copy_tests.sh` (`test_resizing_macro_edit_and_working_reads`), `tests/host/run_profile_wire_v1_tests.sh` (`qmk_profile_channel_test.c`, the payload and compiled read tests), `tests/host/run_qmk_portable_profile_tests.sh` (`qmk_portable_profile_test.c`). No test composes a readback from omitted domains.

## What a backup does not hold

These are read from the keyboard but are not configuration, so no backup,
digest or comparison includes them:

| Not in a backup | Where it is read |
| --- | --- |
| QMK's live values and the combos and behaviours overrides (toggles) | GET `0x22` ([live edits](profile-wire-v1/live-edits.md#runtime-get-0x22)) |
| The edit mode, stored outside every profile | GET `0x27` ([live edits](profile-wire-v1/live-edits.md#edit-mode-get-0x27)) |
| QMK's combo enable flag, part of combo readback's digest | GET `0x06` ([combo domain](profile-wire-v1/combo-domain.md#native-combo-readback--get-0x06)) |
| The brightness limit, the effect names and the key options the build supports | GET `0x08` ([settings domain](profile-wire-v1/settings-domain.md#editor-metadata--get-0x08)) |
| The detected host | GET `0x0B` ([HID channel](profile-wire-v1/hid-channel.md#host-os--get-0x0b)) |
| Flash pause figures | GET `0x0D` ([HID channel](profile-wire-v1/hid-channel.md#flash-pauses--get-0x0d)) |

Transient state is not configuration either: keys held, layers turned on for
now, locks, a running macro, pointer motion. Neither is what only a reflash
changes: executable hooks, hardware geometry, the features a build includes
and its ceilings ([field classification](field-classification.md)). A build
whose combos depend on code of the user's own (GET `0x06` page 0 byte 5,
build flags 0–5) runs combos a backup cannot describe; this firmware sets
none of those flags.

Held by: `tests/host/run_qmk_profile_edit_channel_tests.sh` (`test_gets`), `tests/host/run_combo_readback_tests.sh` (`qmk_combo_readback_test.c`), `tests/host/run_qmk_portable_editor_tests.sh` (`qmk_portable_editor_test.c`).

## Logical form

Two encodings of one profile can differ in bytes that are not content. The
macro bank may hold stale bytes after its 128th terminator
([macro domain](profile-wire-v1/macro-domain.md#bank-layout)); they change the
blob's digest and CRC32, not what the keyboard does. The logical form in
`tests/host/profile_logical_form.py` decodes each domain field by field into
plain values (an action as the native keycode it stands for, the macros as
their 128 streams without the capacity after them), so two encodings of one
profile compare equal there. The compiled defaults must equal Noah's profile
in this form, domain by domain, apart from the differences
`tests/fixtures/folded_keyboard_profile.json` lists with their reasons.

Held by: `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`folded_keyboard_profile_test.py`), `tests/fixtures/folded_keyboard_profile.json`.

## Layers and their references

The bank has sixteen layers, ids 0–15 (D-F14,
[ADR](../adr/0014-bigger-profiles.md)); layer 0 is the base layer. A layer
has no identity beyond its index, so the profile names layers by index
throughout. `LT()` and `LM()` hold their layer in four bits, so every layer
they can express is a bank layer; the five-bit layer actions (`MO()`, `TG()`,
`TO()`, `TT()`, `OSL()`) and layer locks refuse layers 16 and above, and
`DF()` and `PDF()` are refused everywhere
([where an action may be placed](profile-wire-v1/blob-and-digests.md#where-an-action-may-be-placed)).

Every field that names a layer, so moving a layer means rewriting all of them
together:

| Domain | Field |
| --- | --- |
| Keymap `0x60` | the layer's position in the domain; `LT`, `LM`, `MO`, `TG`, `TO`, `TT`, `OSL` and `LOCK_LAYER` keycodes |
| Key behaviours `0x20` | layer actions as a row's target, tap or holds (action kinds 2 and 3, and kind-1 layer keycodes); each row's allowed-layers mask |
| Combos `0x30` | layer actions as inputs or output; each row's allowed-layers mask |
| Settings `0x40` | scalar 5 (auto-mouse pointer layer); scalars 9 (auto-sniping layers), 23 (default layers), 29 and 30 (layer masks); the layer records, by position, and each record's combo reference layer; the layer names, by position |
| Pointing `0x50` | each linked slot's linked layer (byte 9 of its record, D-F62) |
| Lighting `0x10` | each layer colour's layer id; each layer group row's selector (a layer, or `0xff` for all) |

In the pointing domain only a link names a layer: a slot's pointer-layer byte
is a policy, and layer keycodes are refused as pointing outputs
([pointing domain](pd-mode-domain-v1.md)). The firmware checks every reference
against the bank when it validates a profile; it cannot tell a moved layer
from an edited one.

Held by: `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`, `test_keymap_placement_policy`), `tests/host/run_action_dispatch_tests.sh` (`action_dispatch_test.c`), `tests/host/run_profile_validator_v1_tests.sh` (`profile_validator_v1_test.c`), `tests/host/run_profile_settings_v1_tests.sh` (`profile_settings_v1_test.c`). No test holds the completeness of the reference list.

## Restoring a backup

A backup restores by an Apply of its blob as a candidate, like any Apply
([candidate transfer](profile-wire-v1/candidate-transfer.md); its order across
both halves is the [logical profile transaction](logical-profile-transaction-v1.md)'s).
The keyboard validates the whole blob against the running firmware: schema,
every domain version, the declared action-ABI digest, every ceiling and every
action's placement. It becomes the next generation on both halves or nothing
does. No part of a backup is translated (D-F10,
[ADR](../adr/0010-firmware-accepts-only-current-formats.md)): a blob in an
earlier schema or domain version, or declared under another action-ABI digest,
is refused.

The action-ABI digest covers the action vocabulary, the layer and slot counts
and `NOAH_FIRMWARE_VERSION`, and no authored row
([action-ABI digest](profile-wire-v1/blob-and-digests.md#action-abi-digest)).
So a backup restores onto any build of the same firmware version whatever its
compiled defaults are, including a build with no authored behaviours or
combos, and onto no other firmware version.

Held by: `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`: `--import-profile` with the populated and maximum profiles, `--empty-profile` on a build without behaviours or combos, `--reject-profile` refusing an earlier schema as `INCOMPATIBLE_SCHEMA`), `tests/fixtures/client-regression/portable.bin.pd5`.

## Reflashing and backups

A saved profile names the compiled-default and action-ABI digests it was saved
under. After a reflash that changes either one (new authored defaults, a new
firmware version), the half does not use it and runs its compiled defaults
([device-resident profile](device-resident-profile.md)). A backup taken before
the reflash restores if its action-ABI digest still matches, that is when only
the authored defaults changed; a new firmware version refuses it.

Held by: `tests/host/run_profile_store_tests.sh` (`profile_store_test.c`, `test_boot_rejects_old_compiled_default_identity`), `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`).
