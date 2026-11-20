# A release publishes the starter keymap

Cited as D-F78.

There was one keymap folder, `keymaps/noah`, and every pair, the one CI
builds for a release included, compiled it. So a downloaded release started
from the author's own profile: Noah's layer set and thumb behaviours, combos,
custom keys for the apps Noah uses, macros for Mac shortcuts, a Dutch macOS
host and the keyboard name "Charybdis - Home". Someone else had to undo most
of it in Ark before the keyboard was theirs. Noah asked for the published firmware to
leave that keymap out and ship a plainer one (2026-11-19).

**Two keymap folders.** `keymaps/noah` stays: it is the pair Noah flashes.
`keymaps/starter` is the keymap a release publishes. It is the Bastard
Keyboards VIA layout Noah supplied: a QWERTY Base, Lower (shifted number row,
numpad, RGB keys), Raise (function keys, arrows, navigation, media) and Pointer
(mouse buttons, DPI steps, sniping, drag-scroll, `QK_BOOT` and `EE_CLR`), with
Z and / holding Pointer, and twelve transparent, unnamed layers. It has no
custom keys, macros, combos, behaviours or autocorrect entries, and one
pointing slot, drag-scroll. The host is Auto with the US layout, and the
keyboard keeps QMK's own name. Auto-sniping is available but starts with no
layer (`CHARYBDIS_AUTO_SNIPING_LAYER` unset, so the compiled auto-sniping
layers are none); auto-mouse raises Pointer. Its lighting is the noah keymap's
without the author's LED groups or the extra layers' colours. The two folders
share the userspace and every capability: the same feature set, layer bank,
slot counts and action ABI. Only the compiled defaults differ, and so does the
compiled-default digest.

**Every ordinary pair includes the starter pair.** `tools/build-firmware-pair.sh`
builds the noah pair and then the starter pair beside it, as
`N_charybdis_{right,left}_starter.uf2` with its own note, so verify builds it,
`land` can file it, and a release finds a local pair equal to CI's, as it
requires (D-F05). `NOAH_KEYMAP=noah` or `starter` builds one alone; comparison
and resource builds build noah unless `NOAH_KEYMAP` says otherwise. Each
keymap compiles under its own name (`-km starter`) with `USER_NAME=noah`: QMK
finds a keymap folder by name, so the explicit keymap paths alone built noah
twice.

**What a release publishes.** CI's `Pair build` runs
`tools/build-release-firmware-pair.sh`, which builds the starter pair and
exports it under the unchanged asset names
(`bastardkb_charybdis_4x6_noah_{right,left}.uf2`), and states the contract with
`NOAH_KEYMAP=starter`, so `firmware-contract.json` carries the starter
keymap's compiled-default digest. Ark's agreement check still probes the noah
keymap; the two contracts differ only in that digest. `run_starter_keymap_tests.sh`
validates the starter keymap as a real profile in the host suite, since
nothing else there compiles it.

**What changes for a user.** A saved profile is bound to the compiled-default
digest (D-F12), so the first release built from the starter keymap starts a
keyboard that saved a profile under an earlier release from the starter
defaults. Importing the backup in Ark brings that profile back, as after any
release that changes the defaults.

**Weighed and left.** Moving Noah's keymap out of the repository, leaving one
bare keymap and keeping Noah's layout only as an Ark backup, would have taken
that layout out of the public source but left Noah no pair of that profile to
build and test. Swapping a minimal `keymap.c` into the release build alone
would have published a pair no local build produced, which `release` refuses.
Renaming the assets to drop `noah` would have changed every download link and
the release tool for no change in what they hold.

Contracts:
[field classification](../spec/field-classification.md),
[device-resident profile](../spec/device-resident-profile.md).
