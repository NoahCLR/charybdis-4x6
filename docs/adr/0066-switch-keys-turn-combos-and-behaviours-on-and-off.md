# Switch keys turn combos and behaviours on and off

Cited as D-F66.

Combos and behaviours each have a runtime override beside their stored
master: QMK's combo flag, and the firmware's behaviours override (D-F32).
The **switch keys** set them: QMK's `CM_ON`, `CM_OFF` and `CM_TOGG` for
combos, and the firmware's `BH_ON`, `BH_OFF` and `BH_TOGG` for behaviours,
which until now no key could set. Each turns its override on, off or the
other way. They work until the keyboard restarts, never touch the stored
master and never save.

The firmware owns all six, as one action kind. QMK used to switch combos
for the `CM_*` keys itself, but only while pre-processing a physical press:
a behaviour's tap or hold, or a combo's output, reaches QMK as a record that
skips that step, so a combo key there did nothing, and a pressed one
switched before the firmware had decided whether the press was a tap or a
hold. BK's `COMBO_KEYCODES_DOWNSTREAM` now leaves them to the firmware, which
switches wherever a switch key is sent: on a key, in a behaviour's tap and
holds, and as a combo's output. A pressed switch key with a behaviour does
what its behaviour decides, and where the behaviour leaves the tap empty it
taps itself (D-F64), so it switches.

`BH_*` sit in their own aligned block, `0x7ee0..0x7eef` (D-F51), with 16
codes reserved and three used, so no other keycode moves; the rest of the
block and `0x7ef0..0x7eff` stay inert. They are a capability, so Profile
Wire goes to 2.1 (D-F28); the profile's format is unchanged and its schema
stays 4.3. The block moves the end of the action vocabulary, so the
action-ABI digest changes too, and a profile saved under the previous digest
is refused (D-F10).

With behaviours off a key is the standard key, and a behaviours key's
standard key is its switch, so `BH_ON` and `BH_TOGG` always turn behaviours
back on. Switching off decides no press again: a behaviour already holding
something keeps it until its key is let go. Contracts:
[`participation-policy.md`](../spec/participation-policy.md),
[`pd-mode-domain-v1.md`](../spec/pd-mode-domain-v1.md#keycodes).
