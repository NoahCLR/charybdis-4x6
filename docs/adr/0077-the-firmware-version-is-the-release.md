# The firmware version is the release

Cited as D-F77.

The firmware version on capability page 0 was a constant, `0x00010000`,
carried over from VIA's `VIA_FIRMWARE_VERSION` when VIA went (D-F31). Nothing
changed it, so every build reported 1.0.0 and a client could not tell one
build from another. It also fed the action-ABI digest, the way VIA used its
version to wipe EEPROM on a new firmware. While it stayed there, changing the
number would discard the saved profile at reflash, refuse every older backup
and change the digest clients expect, for no change in what a profile means.
Noah asked for the number to say which release is on the keyboard
(2026-11-17).

**What it names.** The firmware version is the firmware release the build
includes, `vYYYY.MM.DD[.N]`: the year in bits 31–16, the month in bits 15–8,
then `N` (0–7, 0 for a release without a suffix) above a five-bit day. So
`v2026.10.27` is `0x07EA0A1B` and `v2026.10.27.1` is `0x07EA0A3B`. It is
never zero. A build from `dev` or a task branch reports the last release it
includes, with no marker of its own.

**Who writes it.** The vault's `release` writes it when it prepares a firmware
release, through a pull request of its own into `dev`, so the prepared head,
the pair verify built and the pair CI builds all carry it. It is never changed
by hand. It comes from the committed tree, never from the build's clock, so a
pair stays reproducible.

**The action-ABI digest.** The digest no longer includes the firmware
version. It covers what a stored action means (the schema, the action
vocabulary, the layer and slot counts and the userspace keycode blocks), and a
release changes none of that on its own. A backup restores onto any build of
the same schema and action ABI, whatever its release, and a saved profile
survives a reflash that changes only the release. Taking the version out
changes the digest once, from `0xF22F93D3` to `0xA144ECCE`: the first flash of
a build with this change runs its compiled defaults, and backups taken before
it are refused, as with any new action ABI.

The capability page keeps the field at bytes 17–20 with the same width, so the
capability layout and the Profile Wire version are unchanged (D-F28: no new
capability).

**Weighed and left.** Keeping the version in the digest and bumping it at each
release would break every saved profile and backup at every release, for no
change in what a profile means. A stamp taken at build time (the date or
`git describe`) would make the pair differ between the local build and CI's,
and `release` publishes CI's pair only when it equals one built locally. A
marker for development builds was left out: a pair from `dev` reports the
last release it includes.

**Ark follow-up:** re-pin, expect the new action-ABI digest, and show the
firmware version as the release it names (`v2026.10.27`).

Contracts:
[Profile Wire](../spec/profile-wire-v1/README.md#version-identity),
[action-ABI digest](../spec/profile-wire-v1/blob-and-digests.md#action-abi-digest),
[portable profile](../spec/portable-profile-v1.md).
