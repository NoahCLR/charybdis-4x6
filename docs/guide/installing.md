# Installing and recovering

## Flashing a release

Each release has two files to flash, one per half:

- `bastardkb_charybdis_4x6_noah_right.uf2` for the right half, the one with
  the USB cable;
- `bastardkb_charybdis_4x6_noah_left.uf2` for the left half.

Download both from the
[latest release](https://github.com/NoahCLR/charybdis-4x6/releases/latest),
then for each half:

1. Plug that half into the computer on its own.
2. Double-tap its reset button. A drive called `RPI-RP2` appears.
3. Copy that half's file onto the drive. The half restarts on the new
   firmware.

Then connect the halves and plug the right half in, and open
[Charybdis Ark](https://github.com/NoahCLR/charybdis-ark) to edit the
keyboard.

Always flash both halves from the same release. Each file knows which half it
is: with the left file on the cabled half the keyboard never shows up on the
computer, and with the right file on both halves they can't talk. Flash the
right file and it comes back. Halves from different releases type, but may
refuse to copy profiles between them.

Only these files carry everything Ark needs. A plain `qmk compile` of this
repository builds a generic image that can't store a profile; see
[Authoring in C](authoring.md#building-the-pair) for building your own pair.

## What happens to your profile

Flashing doesn't erase the saved profile, but a new release may not use it.
The keyboard keeps running a saved profile only when the new firmware has
exactly the same compiled defaults and firmware version as the firmware that
saved it. Otherwise it starts from the new firmware's compiled defaults, and
you restore your profile from a backup.

A backup restores onto any firmware of the same firmware version, whatever
its compiled defaults. A release that changes the firmware version, or the
actions a profile can hold, refuses older backups. So before you update:

1. Export a backup in Ark.
2. Read the release notes.
3. Keep the old release's files if you may want to go back; the backup goes
   back onto those.

The same applies to firmware you build yourself: any change to the compiled
defaults, even one colour, starts the keyboard from them again, and a backup
brings your profile back.

## Recovering

From the gentlest to the most thorough:

- **Unsaved edits went wrong.** Revert in Ark, or unplug the keyboard: it
  starts from its saved profile.
- **A saved profile went wrong.** Restore a backup in Ark, or use one of the
  recovery copies Ark keeps before every save
  ([Backups](https://github.com/NoahCLR/charybdis-ark/blob/dev/docs/guide/backups.md)).
- **The keyboard stopped saving.** Restart it from Ark, or unplug it and plug
  it back in ([Saving](saving.md#if-it-stops-saving)).
- **The halves disagree.** Unplug the keyboard and plug it back in; the halves
  compare their saved profiles and the newer one wins.
- **Back to the compiled defaults.** Forget the saved profile on both halves:
  unplug the keyboard, then for each half hold its top outer key (top left on
  the left half, top right on the right half) while plugging that half in on
  its own. It starts in the bootloader with its settings marked for reset;
  unplug it again. Then connect the halves and plug
  the right half in: each half forgets its saved profile as it starts, and
  both run the compiled defaults. Do both halves:
  if only one forgets, the other copies its saved profile back.
- **Nothing works.** Flash both halves again from the same release.
