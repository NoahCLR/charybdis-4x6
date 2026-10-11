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
- `portable.bin.fixture`, `.pd`, `.pd3`, `.pd4`, `.pd5`: historical and populated
  profile imports, including worst-case macro/custom-key names. The `.fixture`
  suffix on the first file avoids the repository's generated `.bin` ignore rule.

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
version 1, and its runner checks it with the retired version-1 validator, whose
record rules version 2 shares. The populated `.pd*` profiles are unchanged too:
their runner checks that the firmware refuses them as they are, and accepts
their documented translation (`tests/host/translate_eight_slot_profile.py`).

Do not regenerate expected results just to make a changed validator pass.
Review intentional contract changes and extend firmware-owned vectors alongside
the C tests. Ark can independently propose vectors from its integration corpus;
review and import those bytes, record provenance and update the manifest in the
same change. Retain historical vectors when backward compatibility is required.
The independence runner checks the manifest hashes. Running Ark's integration
suite verifies its current implementation rather than these frozen expectations.
