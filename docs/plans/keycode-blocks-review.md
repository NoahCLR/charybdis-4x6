# Keycode blocks and custom keys: review

Review of `17b602ce..780d7dbb` (compiled defaults authored inline, fixed
userspace keycode blocks, 64 named custom keys, stack manifest). This is a
working plan: when every item is closed, fold what still constrains the code
into the spec it governs and delete this file (D-L07).

## Hardware read

Firmware from this branch, flashed on both halves, read-only through the
device service:

- Action ABI `0x1d3fcacc`, feature bit 16, 64 custom-key slots; the peer is
  known and converged.
- The committed profile from the old ABI is ignored; the keyboard runs its
  compiled defaults (`activeKind` compiled, committed digest 0).
- Readback: 8 layer names, 11 named VIA macros, 37 behaviours, 10 combos (the
  two Cmd+N chords at 100 ms, the rest on the 50 ms default), custom keys 0–3
  named Right Thumb, Left Thumb, Click Spam, Drag Window.
- Every custom-key and pointing-slot key on the layers is named in the app.
- The backup `backups/pre-keycode-migration-2026-09-27T18-35-40-949Z` upgrades
  to the new ABI and passes the restore placement check. Against the running
  defaults it differs only as expected: thumbs were VIA macros 18/19 and are now
  custom keys 1/0, combos store an explicit 50 ms instead of following the
  default, and the backup has no custom-key names.
- Observation, not a code finding: Sniping DPI reads 100 where the backup has
  200. That value lives in QMK's keyboard EEPROM word (`eeconfig_kb`), not the
  profile; RGB and default-layer EEPROM values survived the flash, so why it
  reset is unknown. Restoring the backup sets it back.

## Findings

Each item was checked against the code; the reproducing scenario is given.

### 1. Discarding a custom-key name after importing an old backup throws — fixed

`core/model/profile-revert.js` in the independent Ark repository (formerly
`tools/charybdis-live/`). A backup's settings
are v4 or older, so after an import the review lists `customKey:n` (Right Thumb
→ no name), and discarding it fails with "This custom key name cannot be put
back on its own." because `mine` has no `customKeyNames`. The macro branch has
the same shape for v2/v3 settings.

Fixed: a discarded name upgrades the draft's settings to the version that
carries it (v4 for macros, v5 for custom keys) and the domain envelope follows.
Test: `custom-key-editor.test.js`, a v2 import taking back one of two names.

### 2. Unassigned userspace keycodes send a stray key — fixed

`users/noah/lib/action/action_kind.c:283`. The retired matcher treated every
code from the old keymap range up as inert; the custom-key matcher covers only
`0x7e40..0x7e7f`. Unused pointing slots (`0x7e88..0x7e9f`, `0x7ea8..0x7ebf`),
`LOCK_LAYER(n)` for n ≥ 8 (`0x7ec8..0x7edf`) and `0x7ee0..QK_USER_MAX` now fall
through to LITERAL, whose tap goes through `register_code16` and sends the
high byte as modifiers and the low byte as a key. The profile validator accepts
any `QMK_KEYCODE` operand, so a saved profile can carry one.

Fixed: action kind `UNASSIGNED_USER` (priority 10, no capabilities, no-op
dispatch) takes every `QK_USER..QK_USER_MAX` code no block claims; it is
refused at every placement, combo output included. The ABI digest does not
cover the kind registry, so the ABI is unchanged. Tests: `action_dispatch_test`
(each gap refused everywhere, the blocks' last codes keep their kinds) and
`action_lifecycle_test` (tap, press and release send nothing). The real-profile
harness stubbed pointing locks as absent, so they validated as plain keycodes;
it now resolves them from `pd_modes` as the firmware does.

### 3. Layer edits on old-ABI keyboards use the new numbering — fixed

`core/model/portable-profile.js:260` (`reorderLayers`), `core/model/layer-reach.js`
and `placementProblem` read `0x7ec0`/`0x7e40` blocks whatever the document's
ABI. On a keyboard still running the previous firmware, reordering leaves an
old `LOCK_LAYER` (`0x7e5c+n`) pointing at the wrong layer. The fixture
`tests/fixtures/portable-profile.js:16` was renumbered to `0x7ec4` while it
still claims ABI `0xeb80829c`, which hid this.

Fixed: a draft opens only on firmware with the known ABI, and `reorderLayers`
refuses a document with another ABI. The shared fixture is split:
`legacyDocument()` is the old backup with its old numbering (`0x7e60`), and
`document()` is that backup imported through the app's own upgrade chain.
Schema-1 restore and settings tests use the legacy one. Layer-reach still reads
an old keyboard's keys in the new numbering, but only for display, since
nothing on such a keyboard can be edited.

### 4. A custom key in a combo has two encodings — fixed

`upgradeKeycodeBlocks` turns a combo's custom key into action kind 7; capture
builds combos from the native readback as kind 1 (`0x7e42`), as seen on the
keyboard. The firmware runs both identically, but a restored backup never
matches the next read, and the review can show a change that is none.

Fixed as proposed. The real backup's combos now equal the keyboard's read
apart from the explicit 50 ms windows. Test: `portable-profile.test.js`.

### 5. Custom-key names are not checked for plain ASCII — fixed

`tests/host/real_profile_validation_test.c:225` checks macro and layer names;
`keymap_materialize.h` checks only the size. `KEY(CUSTOM_KEY_4, "Café")`
compiles, and with nothing stored the readback reports a v5 domain the
firmware's own validator rejects (`0x20..0x7e` from v4 on).

Fixed; a non-ASCII name in `keymap.c` fails the test (tried, then reverted).

### 6. A backup whose behaviour step sends a custom key imports, then fails — fixed

The old keymap allowed `TAP_SENDS(custom)`. The upgrade makes such a step
kind 7 and `validateSnapshot` accepts it; the restore then refuses it naming
the old keycode, because `profile-device-service.js:443` runs
`profilePlacementProblem` on the untranslated `document.profile`. The real
backup has no such step.

Fixed as proposed. Test: `portable-profile.test.js`.

### 7. Settings v5 has no negative validator tests — fixed

`tests/host/profile_settings_v1_test.c` now covers each case, plus a v5 domain
publishing and reading back.

## Minor

- **M1 — stale docs and comments** — fixed: `keymap.c`, `effective_combo_runtime.h`,
  `RGB_IMPLEMENTATION.md`, `field-classification.md` (and a custom-key names
  row), D-L25's note on D-L42, and the unassigned range (`0x7fff`) in
  `noah_keymap_ids.h` and `pd-mode-domain-v1.md`.
- **M2 — `profile_introspect.py` needed Python 3.12** — fixed; `--check` passes on
  3.9 and 3.14.
- **M4 — the layout read warned about keys the app names** — fixed:
  `unnamedKeyCount` counts the blocks as named on the known ABI.
- **M5 — saved LED groups counted toward the 32 stage-row limit** — fixed: saved
  groups are bounded by the group limit and stage rows by theirs. No boundary
  test; it would need a 32-stage-row keymap variant.
- **M6 — name readback with nothing stored was quadratic** — fixed: a cursor
  resumes from the last record; backward and repeated page reads are tested.
- **M7 — profile headroom** — fixed, and worse than estimated. The keyboard's
  real profile is 2,819 bytes with 618 in settings; all 128 names at 20
  characters would make it 5,201 bytes, over the 5,088-byte ceiling (about 120
  fit). A name edit that met the ceiling said "Profile blob is 5101 bytes"; both
  name editors now say the profile is full (`encodeNamedProfile`), and
  `portable-profile-v1.md` no longer claims every name fits at once.
- **M8 — fixture for an impossible keyboard** — fixed:
  `device-profile-readback.json` is back on the bridge ABI `0xdcb00959`; the one
  test that aliased `0x7E80` as pointing slot 0 now says it is on current
  firmware.
