# Live edits: a working copy in RAM, Save, and the edit mode

Cited as D-F32.

Edits take effect on both halves at once, without a save. Each half keeps a
static, slot-sized buffer for a working copy of its profile. An edit (up to
eight splices across domains, one revision) is checked domain by domain with
the code whole-profile validation runs and published at a safe boundary; the
other half follows over a split RPC of its own. Save is the ordinary candidate
transaction with a candidate made entirely of REUSE from the working copy, so
every durability and power-loss guarantee is the same code. A replug boots the
saved profile: unsaved RAM never has authority at boot.

Edits are not prepared on both halves first: both run one build and the same
checks, so a refusal on the other half is a fault (answered by sending the
whole working copy), and a second round trip would slow every edit.

The edit mode (Apply, Live, Live + review) is stored on each half in QMK's
EEPROM user datablock, outside the profile. Live saves after 3 s with no edit
and no held key, with at most one automatic attempt per revision so a failure
cannot loop. The profile is the only durable owner of the settings QMK holds:
they reach QMK through its no-EEPROM setters, and the DPI and lighting keys
become settings edits. Contracts: [`profile-wire-v1.md`](../architecture/profile-wire-v1.md),
"Live edits"; [`profile-split-v1.md`](../architecture/profile-split-v1.md).
