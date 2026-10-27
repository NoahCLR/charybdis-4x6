# Profile blob, actions and digests

The canonical profile blob is the one byte form of a profile: what a
candidate uploads, what a profile slot stores after its record header, what a
working copy holds and what payload readback returns. This file covers its
header, the domain envelope, the domain registry, the four-byte action
encoding every domain shares, and the checksums and digests that name a
profile. Each domain's payload is its own file (see the
[index](README.md#layers-and-files)); which domains a half admits and how a
saved profile is stored is the [device-resident profile](../device-resident-profile.md);
the names of the refusals below are listed in [errors](errors.md#validation-results).

## Blob header

Eight bytes, then the domains:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | ASCII magic `NLP1` |
| 4 | 1 | schema major, `4` |
| 5 | 1 | schema minor, `2` |
| 6 | 1 | domain count, `0..7` |
| 7 | 1 | flags: exactly `0x01` (bit 0 canonical encoding; bits 1–7 reserved) |

The blob carries no length of its own. The carrier supplies it as a 32-bit
field: a candidate's BEGIN, a profile slot's record header, payload readback's
metadata page. A blob is `8..130,048` bytes: the upper bound is one 128 KiB
profile slot less the first kilobyte its header and commit marker take
(D-F29, [`0029`](../../adr/0029-profile-slots-in-their-own-flash-region.md)).
The empty profile is the eight bytes `4e 4c 50 31 04 00 00 01`.

A header is refused, in this order: shorter than 8 bytes `TRUNCATED`; longer
than 130,048 `CAPACITY_EXCEEDED`; wrong magic `INVALID_MAGIC`; any other
schema `INCOMPATIBLE_SCHEMA` (no earlier schema is read, D-F10,
[`0010`](../../adr/0010-firmware-accepts-only-current-formats.md)); a
reserved flag bit set `RESERVED_FLAGS`; flag bit 0 clear `NONCANONICAL`; more
than seven domains `CAPACITY_EXCEEDED`.

Held by: `tests/host/run_profile_blob_v1_tests.sh` (`profile_blob_v1_test.c`), `tests/fixtures/profile_blob_v1.fixture`.

## Domain envelope

Each domain is six bytes of envelope and its payload:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | domain id |
| 1 | 1 | domain version |
| 2 | 4 | payload length, uint32 |
| 6 | N | payload |

Domains follow the header back to back, each id at most once, in strictly
ascending id order, with no padding between them and nothing after the last.
A domain is refused `UNKNOWN_DOMAIN` when the registry below does not know its
id, and `UNKNOWN_DOMAIN_VERSION` when its version is not the registry's
current one; there is no skippable or optional unknown domain. A repeated id
is `DUPLICATE_DOMAIN`, a lower one `DOMAIN_ORDER`, a payload past the end of
the blob `TRUNCATED`, and bytes after the declared domains `TRAILING_BYTES`.
The firmware's encoder writes the domains in registry order whatever order it
is given them in.

Held by: `tests/host/run_profile_blob_v1_tests.sh` (`profile_blob_v1_test.c`), `tests/fixtures/profile_blob_v1.fixture`.

## Domain registry

One registry row per domain (`users/noah/lib/profile/schema/profile_domain_registry.h`).
A row's index is its position; its bit in every domain mask (a candidate's
requested domains, a record's domain mask, capability page 1 byte 12) is
`1 << index`.

| Index | Mask bit | Id | Version | Domain | Payload |
| ---: | ---: | ---: | ---: | --- | --- |
| 0 | `0x01` | `0x10` | 4 | Lighting | [rgb-domain-v1.md](../rgb-domain-v1.md) |
| 1 | `0x02` | `0x20` | 2 | Key behaviours | [key-behavior-domain.md](key-behavior-domain.md) |
| 2 | `0x04` | `0x30` | 3 | Combos | [combo-domain.md](combo-domain.md) |
| 3 | `0x08` | `0x40` | 8 | Settings: layer records, custom keys and their shortcuts, layer, macro and custom-key names, the keyboard name | [settings-domain.md](settings-domain.md) |
| 4 | `0x10` | `0x50` | 3 | Pointing slots | [pd-mode-domain-v1.md](../pd-mode-domain-v1.md) |
| 5 | `0x20` | `0x60` | 1 | Keymap | [keymap-domain.md](keymap-domain.md) |
| 6 | `0x40` | `0x70` | 1 | Macros | [macro-domain.md](macro-domain.md) |

The registry is the wire vocabulary in every build. Which domains a build
admits is separate: its supported domain mask (capability page 1 byte 12) is
the domains its compiled defaults hold, plus combos in a build with combos
and settings in a build that saves (`NOAH_PORTABLE_PROFILE_ENABLE`), and zero until the live-profile owner reports. The pointing domain is required
in every profile the keyboard validates. Admission is the
[device-resident profile](../device-resident-profile.md)'s.

Held by: `tests/host/run_profile_blob_v1_tests.sh` (`profile_blob_v1_test.c`, the registry test), `tests/host/run_profile_wire_v1_tests.sh` (`qmk_profile_channel_test.c`, the supported domain mask), `tests/host/run_profile_validator_v1_tests.sh` (`profile_validator_v1_test.c`).

## How the firmware owns a domain

A registry row names its domain module. The whole-profile validator and the
compiled-defaults writer reach each domain only through a switch generated
from the rows, calling `validate_<module>` and
`noah_profile_<module>_compiled_v1_write`, so a row added without both does
not compile (D-F11, [`0011`](../../adr/0011-one-definition-per-profile-domain.md);
D-F13, [`0013`](../../adr/0013-registry-row-names-its-domain-module.md)). A
domain whose compiled encoder writes nothing is absent from the build's
compiled defaults (D-F12, [`0012`](../../adr/0012-one-complete-compiled-profile.md)).

One envelope walker checks magic, schema, flags, domain count, versions,
order, payload extents and completion. It takes one already-read blob or
domain header at a time and reads nothing itself, so the store's check, the
boot scan, the prepared-commit check and the whole-profile validator share
one shape policy while each keeps its own read budget; the validator reads at
most 20 bytes a step. Checksums and declared masks stay the store's;
semantic references and placement stay the validator's. A validated profile
keeps decoded views for lighting and key behaviours only; every other
domain's runtime finds its payload by walking the envelope through the
profile's reader, after checking the domain mask.

Held by: `tests/host/run_profile_blob_v1_tests.sh` (`profile_blob_v1_test.c`, the find-domain test), `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`), `tests/host/run_profile_validator_v1_tests.sh` (`profile_validator_v1_test.c`).

## Action encoding

Wherever a domain stores an action, it is four bytes:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | action kind |
| 1 | 1 | flags, zero |
| 2 | 2 | operand, uint16 |

| Kind | Action | Operand | Native keycode |
| ---: | --- | --- | --- |
| 0 | none | zero | `KC_NO` |
| 1 | QMK keycode | any 16-bit keycode | the operand itself |
| 2 | layer, momentary | layer `0..15` | `MO(n)` |
| 3 | layer lock | layer `0..15` | `LOCK_LAYER(n)`, `0x7ec0 + n` |
| 4 | pointing slot, held | slot `0..31` | `PD_MODE_n`, `0x7e80 + n` |
| 5 | pointing slot, locked | slot `0..31` | `PD_MODE_n_LOCK`, `0x7ea0 + n` |
| 6 | macro | slot `0..127` | `QK_MACRO_n`, `0x7700 + n` |
| 7 | custom key | `0..127` | `CUSTOM_KEY_n`, `0x7f00 + n` |

An action is refused `RESERVED_FLAGS` with a nonzero flags byte,
`UNKNOWN_ACTION_KIND` with a kind above 7, and `INVALID_OPERAND` with an
operand at or above its kind's count or a nonzero operand on kind 0; any of
them refuses the whole candidate or edit. The validator also refuses a layer
at or above the build's layer count and a pointing slot the build does not
have. The userspace keycode blocks are fixed (D-F51,
[`0051`](../../adr/0051-userspace-keycodes-in-fixed-blocks.md)).

The firmware writes the semantic kind for every native keycode that has one:
`KC_NO` as kind 0, a custom key as 7, a macro as 6, a layer lock as 3, `MO()`
as 2, a pointing hold or lock as 4 or 5, and anything else as kind 1. Kind 1
decodes for any value; what a QMK keycode may do is decided by the action-ABI
digest and the placement rules below. A custom key as a behaviour step sends
its shortcut only (D-F26, [`0026`](../../adr/0026-custom-key-carries-a-shortcut.md);
[key-behavior-domain.md](key-behavior-domain.md)).

Held by: `tests/host/run_profile_blob_v1_tests.sh` (`profile_blob_v1_test.c`), `tests/fixtures/profile_blob_v1.fixture`, `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`, native mapping and keycode blocks), `tests/host/run_action_dispatch_tests.sh` (`action_dispatch_test.c`, native round trips).

## Where an action may be placed

A host's candidate and every live edit are held to the rules the authored
profile is held to at build time (`noah_action_supported_at`): where each
action may stand as a key (a keymap position, or the key a behaviour listens
on), a behaviour's tap, its press-and-hold, its other holds, and a combo's
output. A combo holds its output while it is held.

| Action | Key | Tap | Press-and-hold | Other hold | Combo output |
| --- | :-: | :-: | :-: | :-: | :-: |
| `MO()`, `TT()` | yes | no | yes | no | yes |
| `OSL()` | yes | yes | no | no | yes |
| `LM()` | yes | no | no | no | yes |
| `LT()` | yes | no | no | no | no |
| `TG()`, `TO()`, `LOCK_LAYER()` | yes | yes | yes | yes | yes |
| `DF()`, `PDF()`, and any layer keycode naming a layer at or above the layer count | no | no | no | no | no |
| Any other keycode in QMK's user range `0x7e40..0x7fff`: the inert `0x7e40..0x7e7f` block, layer locks past the layer count, `0x7ee0..0x7eff`, and `0x7f80..0x7fff`, which only the runtime sends (`CUSTOM_KEY_LAYER_TAP(n)`, D-F26) | no | no | no | no | no |

A refused behaviour action is `INVALID_REFERENCE` at its row, tap index and
field; a refused combo output refuses the combo domain at its row; a refused
keymap keycode names its layer (table) and position (row). The saved profile
being adopted at boot, the other half's saved profile arriving as its copy,
and the compiled defaults are not held to placement, so a profile saved
before a rule existed still loads and still copies between the halves.

Held by: `tests/host/run_action_dispatch_tests.sh` (`action_dispatch_test.c`, the placement table), `tests/host/run_profile_validator_v1_tests.sh` (`profile_validator_v1_test.c`, the placement hook).

## Digests and checksums

| Name | Algorithm | Covers |
| --- | --- | --- |
| CRC32 | reflected CRC-32, polynomial `0xEDB88320`, initial `0xFFFFFFFF`, final XOR `0xFFFFFFFF` | the exact canonical blob |
| Payload digest | FNV-1a 32-bit, offset basis `0x811C9DC5`, prime `16777619` | the exact canonical blob |
| Compiled-default digest | the payload digest | the build's compiled defaults as a blob |
| Action-ABI digest | FNV-1a 32-bit | the action vocabulary, below |

The CRC32 detects corruption; the payload digest names a profile for
comparison. They are separate fields everywhere they travel. A digest never
stands in for the CRC32, length, schema and validation checks before a
profile runs. Profile status page 0's source digest is the compiled-default
digest.

A candidate's BEGIN declares the blob's length, CRC32, payload digest and
action-ABI digest; a declared action-ABI digest other than the build's is
refused `INCOMPATIBLE_ACTION_ABI`. A saved profile's record header carries
its compiled-default and action-ABI digests, and the other half's copy names
both; a record or copy whose digests are not the build's is not used
([device-resident profile](../device-resident-profile.md),
[split protocol](../profile-split-v1.md)). Until the live-profile owner
reports, the capability page's digests are zero and profile status sets
*digests unavailable*: zero is never presented as a digest.

Held by: `tests/host/run_profile_blob_v1_tests.sh` (`profile_blob_v1_test.c`), `tests/fixtures/profile_blob_v1.fixture` (`blob.*.fnv1a32`, `blob.*.crc32`), `tests/host/run_profile_store_tests.sh` (`profile_store_test.c`), `tests/host/run_profile_wire_v1_tests.sh` (`qmk_profile_channel_test.c`).

### Action-ABI digest

The FNV-1a digest of this byte stream, integers little-endian:

| Order | Bytes | Value |
| ---: | --- | --- |
| 1 | 4 | ASCII `NLA1` |
| 2 | 1 + 1 | schema major and minor |
| 3 | 1 | the highest action kind, `7` |
| 4 | 1 | layer count |
| 5 | 1 | pointing slot count |
| 6 | 1 | macro slot count |
| 7 | 1 | custom-key count |
| 8 | 4 | `NOAH_FIRMWARE_VERSION` |
| 9 | 2 each | `KC_NO`, `KC_TRNS`, `MO(0)`, `LT(0, KC_NO)`, `OSM(0)`, `MT(0, KC_NO)`, `QK_MACRO_0`, `CUSTOM_KEY_0`, the first layer lock |
| 10 | 1 + 2 + 2 per layer | layer id, `MO(n)`, `LOCK_LAYER(n)` |
| 11 | 1 + 4 + 2 + 2 per slot | slot id, its 32-bit mode flag, its hold keycode, its lock keycode |
| 12 | 2 + 2 | the end of the userspace keycodes (`0x7ee0`), then `0xFFFF` |

It depends on no authored row, so an empty profile and a populated one share
it. Because it includes the firmware version and the layer count, a new
firmware version or layer bank has its own digest, and a profile saved under
another is refused.

Held by: `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`), `tests/fixtures/compiled_profile_pd_v2.fixture` (`profile.action_abi`).
