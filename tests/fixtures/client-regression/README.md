# Firmware-owned client regression vectors

These frozen inputs preserve firmware regression coverage without loading an
application implementation. `manifest.json` records the producing Ark revision
and checksums. The initial producer was the clean standalone Ark commit named
there; its cross-language runner bodies were moved unchanged into Ark's
`tests/integration/` for ongoing integration testing.

- `pd-corpus.bin.gz`: 8,635 PD byte-mutation, length, diagonal and UTF-8 cases.
  Each uncompressed record is expected acceptance (one byte), little-endian
  length (two bytes), then the domain bytes. The expected values were produced
  by the recorded Ark decoder and are now reviewed firmware regression data.
- `macro-corpus.json.gz`: 3,022 seeded macro inputs, each with `hex` input and
  `expected` C program length summed over the windows it plays in, or -1 for a
  macro the firmware refuses.
- `portable.bin.pd5`: a populated profile from eight-slot firmware (schema 2,
  RGB v2, PD v1). The firmware refuses it as it is; it is the one vector for
  refusing an earlier format. Its current-format counterpart, with the same
  worst-case macro and custom-key names, is `tests/fixtures/populated_profile_v4.fixture`.

The firmware suite decompresses/loads these local inputs using Python or shell,
then exercises the existing C tests in their normal and sanitizer variants.
No Ark checkout, npm installation or regeneration is required for these tests.
The provenance is a source reference, not an executable dependency.

Byte 87 of a directional PD record became the output setting (D-F07): the six
corpus cases that set it to `1` in an otherwise valid directional record are
now expected to be accepted, and only those six expectations changed. Byte 3
of a scrolling record became its scroll axes (D-F08): the four cases that set
it to `1` or `2` in an otherwise valid scrolling record are likewise accepted.

A stored macro plays in windows decoded from storage, so it has no program
limit of its own (D-F14, `macro-limits`). The two corpus cases that were refused only for
exceeding 512 program bytes, 600 letters and 171 taps, are now accepted at 606
and 513 bytes; only those two expectations changed.

Thirty-two sparse pointing slots (D-F09) made PD domain version 2 and RGB
version 3 the only ones the firmware accepts. The PD corpus is unchanged: it is
version 1, and its runner moves each frozen record into the current record
layout and checks it with the current record validator.

PD version 4 retired the mouse button records (bytes 52..69 reserved, D-F62).
Every frozen case stores Arrow's buttons, so each is now refused as it is; the
runner also checks that each case accepted before is accepted again with those
bytes cleared, so the corpus still checks every other record rule. The bytes
and the stored expectations are unchanged.

Do not regenerate expected results just to make a changed validator pass.
Review intentional contract changes and extend firmware-owned vectors alongside
the C tests. Ark can independently propose vectors from its integration corpus;
review and import those bytes, record provenance and update the manifest in the
same change. Only current formats are accepted, so a historical vector stays only
as evidence that the firmware refuses it.
The independence runner checks the manifest hashes. Running Ark's integration
suite verifies its current implementation rather than these frozen expectations.

Pointing macro triggers (D-F23) extend the tap vocabulary to `0x7700..0x777f`
with inherited modifiers only. The frozen PD corpus was reviewed: it contains
no newly accepted record, so its bytes and expectations remain unchanged.
New cases live in `pd_mode_domain_v4.json` and the cold validator host test.
