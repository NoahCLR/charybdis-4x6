# Saving and the two halves

Everything you can change without a reflash is one **profile**: the keymap,
behaviours, combos, macros, custom keys, pointing modes, lighting, settings
and the keyboard's name. Each half keeps its own copy, and the two keep each
other up to date over the cable. Ark's
[Review and save](https://github.com/NoahCLR/charybdis-ark/blob/dev/docs/guide/review-and-save.md)
page is the app's side of this; this page is the keyboard's.

## What the keyboard runs

The keyboard runs, in order of preference:

1. the **working copy**: your unsaved edits, held in memory;
2. the **saved profile**: what it last saved to flash;
3. the **compiled defaults**: the profile built into the firmware, used when
   nothing is saved or the saved profile can't be used.

Unplugging, or a restart, drops the working copy, so the keyboard always
starts from its saved profile. A saved profile is used only by firmware with
exactly the same compiled defaults and firmware version as the firmware that
saved it; any other firmware starts from its own compiled defaults (see
[Installing](installing.md#what-happens-to-your-profile)).

## Edits, Save and the edit mode

A **live edit** changes the working copy on both halves at once, at the next
**safe boundary**: a moment when no key, layer, lock, macro or tap is in
progress, so nothing changes under your fingers. **Save** makes the working
copy the saved profile on both halves; **Revert** drops it and runs the saved
profile again, without writing anything.

The **edit mode** decides how edits reach the keyboard. Each half stores it
outside the profile, so restoring a backup never changes it.

- **Live edit**: edits run at once, and the keyboard saves by itself once 3
  seconds have passed with no new edit and no key held. It does this even if
  Ark has closed. A held key puts the save off. A failed save leaves the
  edits running, unsaved, and says so; Save tries again.
- **Live preview + save**, the default: edits run at once and wait for your
  Save. The keyboard never saves them on its own.
- **Review and save**: the keyboard runs its saved profile until Ark saves a
  complete new one.

The mode changes only with nothing unsaved and both halves connected.

## Keyboard adjustments

The DPI keys and the QMK lighting keys change the setting at once and join the
working copy on both halves, like any edit. In Live preview + save they wait
for your Save. In Live edit, and in Review and save when the adjustment is the only unsaved
change, the keyboard saves them after the same 3 quiet seconds.

QMK's own settings storage is never written and never wins over the profile.
The combo on/off keys are different: they last until the keyboard is
unplugged and are never saved.

## How a save stays safe

Each half has two profile slots and a save always writes the one not in use,
finishing with the marker that makes it valid. A power cut at any moment
leaves a complete saved profile, the old one or the new one, never a mix.

Every save is a new **generation**. When the halves meet, at power-up or when
the cable is reconnected, they compare generations and the newer one is
copied to the other half; which half has the USB cable doesn't matter.
Halves whose firmware has different compiled defaults or a different firmware
version copy nothing, so flash both halves from the same release.

While the keyboard writes flash it pauses briefly between steps. It waits for
50 ms with no key held or changing before each erase, so a key pressed during
a save can make it take longer.

## If it stops saving

When a half meets a fault it can't safely carry on from, such as a flash write
that failed, it **stops**: typing goes on, but it refuses edits, Save and
Revert, and it says it has stopped. A restart clears most stops. Ark can
restart both halves over USB, which is exactly as safe as unplugging: unsaved
edits are lost on the keyboard, and Ark puts them back afterwards. The
keyboard never restarts on its own to clear a stop. If the halves still
disagree after a restart, unplug the keyboard and plug it back in.

A stop that comes straight back after a restart is in the flash or in the
firmware itself: forget the saved profile
([Installing](installing.md#recovering)), or flash both halves again.

## The keyboard's name

The keyboard's name is part of the profile: up to 32 bytes, the name your
computer, Chrome's device picker and Ark list it under. Left empty, it uses
QMK's built-in name. Computers read the name only when a device connects, so
after you rename it, the right half drops off USB for a moment and comes back
under the new name. It waits until both halves hold the new profile, nothing
is in progress and Ark has been quiet for a second.
