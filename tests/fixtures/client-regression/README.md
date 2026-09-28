# Firmware-owned client regression vectors

These frozen inputs preserve firmware regression coverage without loading an
application implementation. `manifest.json` records the producing Live revision
and checksums. The initial producer was the clean standalone Live commit named
there; its cross-language runner bodies were moved unchanged into Live's
`tests/integration/` for ongoing integration testing.

- `pd-corpus.bin.gz`: 8,635 PD byte-mutation, length, diagonal and UTF-8 cases.
  Each uncompressed record is expected acceptance (one byte), little-endian
  length (two bytes), then the domain bytes. The expected values were produced
  by the recorded Live decoder and are now reviewed firmware regression data.
- `macro-corpus.json.gz`: 3,022 seeded macro inputs, each with `hex` input and
  `expected` C program length, or -1 for a rejected over-cap macro.
- `portable.bin.fixture`, `.pd`, `.pd3`, `.pd4`, `.pd5`: historical and populated
  profile imports, including worst-case macro/custom-key names. The `.fixture`
  suffix on the first file avoids the repository's generated `.bin` ignore rule.

The firmware suite decompresses/loads these local inputs using Python or shell,
then exercises the existing C tests in their normal and sanitizer variants.
No Live checkout, npm installation or regeneration is required for these tests.
The provenance is a source reference, not an executable dependency.

Do not regenerate expected results just to make a changed validator pass.
Review intentional contract changes and extend firmware-owned vectors alongside
the C tests. Live can independently propose vectors from its integration corpus;
review and import those bytes, record provenance and update the manifest in the
same change. Retain historical vectors when backward compatibility is required.
The independence runner checks the manifest hashes. Running Live's integration
suite verifies its current implementation rather than these frozen expectations.
