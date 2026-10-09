# Behaviour and combo participation

Whether a key press uses its keycode's shared behaviour, and whether it can
join a combo, is decided once, at the press, by one firmware policy module.
This document is the contract for that decision (D-F14). The stored fields
live in the settings, key-behaviour and combo domains; their bytes are in
[Portable Profile V1](portable-profile-v1.md#settings-domain-0x40-version-6)
and [Profile Wire V1](profile-wire-v1.md).

## Scopes

| Scope | Behaviours | Combos | Stored in |
| --- | --- | --- | --- |
| Global | master enable | master enable (QMK's combo enable, scalar 20) | settings scalars 28 and 20 |
| Layer | all authored behaviours on/off | all combos on/off | settings scalars 29 and 30, one bit per layer |
| Definition | each behaviour row: enabled, allowed layers | each combo row: enabled, allowed layers | behaviour row and combo row |
| Placement | use the shared behaviour, or bypass to the normal action | participate in combos, or stay out | per-layer bitmaps in the settings domain |

Every applicable scope must allow participation. Any off choice vetoes it; a
lower scope cannot override an off master or its source layer's switch.
Behaviour participation and combo participation are independent of each
other.

Disabling keeps everything it disables: a disabled behaviour row keeps its
steps, timing and allowed layers; a disabled combo keeps its inputs, output
and window. Re-enabling restores them unchanged. Every control is stored,
read back, and carried through reboot, backup, restore and both-half commit
like the rest of the profile.

Fresh compiled defaults and translated older backups allow everything: both
masters on, every layer bit set, every definition enabled with every bank
layer allowed, and every placement bitmap zero (no bypass, no exclusion).
So a profile that never touches these controls behaves exactly as before.

## Source layer and placement

A press's **source layer** is the layer that supplied its resolved keycode:
QMK's `layer_switch_get_layer()` for that key position, which falls through
`KC_TRNS` entries to the highest active layer with a non-transparent key. It
is captured with the press (`read_source_layers_cache()` in the userspace hook,
the same cache QMK uses to release the key) and never inferred from keycode
identity, because one keycode can sit on several layers.

The **placement** is the pair (source layer, matrix position). Transparent
entries above the source layer are not placements and veto nothing.

With layers 1, 2 and 3 active, 3 highest, and behaviours and combos off on 2:

- a non-transparent key on 3 uses layer 3's permissions;
- a transparent key on 3 that falls to a non-transparent key on 2 uses layer
  2's permissions, so its behaviour and combo participation are disabled;
- transparent entries on 3 and 2 that fall to a key on 1 use layer 1's
  permissions; layer 2's switch does not veto it.

`KC_NO` stops the fallthrough and produces no action. `KC_TRNS` continues
below. On a layer with behaviours and combos off, mapping `KC_W` there sends a
plain W under that layer's policy, while `KC_NO` blocks the key.

Ark identifies the source layer of an inherited key and points the user to
that placement's controls.

## Behaviour decision

A physical press uses its keycode's behaviour row only if all of these hold:

1. the behaviour master is on (scalar 28);
2. the source layer's bit is set in scalar 29;
3. the row is enabled and its allowed-layer mask has the source layer's bit;
4. the placement's bypass bit is clear.

Otherwise the press takes its **normal action**: the keycode as if it had no
behaviour row, with the usual modifier, layer and pointing ownership. Native
`LT()` and `MT()` keep native QMK tap-hold timing. A custom key has no native
action, so a bypassed custom-key press does nothing; Ark explains this, and a
layer that needs output there maps a plain keycode. There is no separate
fallback-action field.

One `KC_W` row is shared by every `KC_W` placement. Bypassing W on one layer
leaves every other W placement eligible.

The decision is made at the press and held by the press's owner through its
release and any pending gesture completion. A layer change while the key is
held cannot move the press to another owner or lose its release.

## Combo decision

QMK calls BK's `combo_key_record_allowed()` for every member record of every combo that
contains the press's (reference-remapped) keycode. A press counts towards a
combo only if all of these hold:

1. combos are enabled (scalar 20, QMK's own enable);
2. the press's source layer has its bit in scalar 30;
3. the combo row is enabled and its allowed-layer mask has the source layer's
   bit;
4. the placement's combo-exclusion bit is clear.

Conditions 2 and 4 and the source layer are captured in the physical record
at the press and copied to its release. BK's record gate skips all member
state updates on a veto, including excluded presses and releases: an excluded
duplicate keycode cannot clear an eligible occurrence's held member bit.
The existing trigger hook alone cannot provide this guarantee. Members of one chord may come from different source
layers; each must pass on its own, and any member that does not prevents that
chord. A press that passes no combo is not buffered.

Combo reference remapping (the per-layer reference layer, settings v6) can
change the keycode a combo matches against. It never replaces the press's
source layer as the permission context.

A policy change while a chord is partly pressed settles ordinary keys once;
an active combo's output still receives its matching release.

## Generated outputs

Placement controls apply to physical presses only. A combo output and a
behaviour step's synthetic record keep their existing dispatch and ownership
contracts and have no physical placement, so no placement bit applies to
them.

A custom key emitted by a combo still observes the behaviour master and its
row's enable. Its allowed-layer context is the combo's **origin layer**: the
source layer captured from the chord's representative owner press, the same owner key
the combo-origin tracker already chooses for release matching and feedback.
A chord whose members came from different source layers therefore uses its
owner press's layer, never a union or intersection of the others. The origin
tracker retains that layer through delayed output delivery. Generated permission
is captured before native tapping and admission queues; origin normalization remains at dispatch
and does not consume a pending origin merely to classify tapping.

Origin reconstruction uses only occurrences eligible for that combo: the
physical mirror retains each press's captured combo permission and source
layer, and applies the combo row's allowed-layer rule before matching members.
An excluded duplicate held before completion contributes neither ambiguity nor
footprint. Multiple eligible occurrences still leave reconstruction ambiguous.
The last declared input is the representative owner regardless of press order;
its captured source layer supplies generated permission and remains paired with
the eligible footprint through output press and release.

## Where it runs

`users/noah/lib/key/behavior/participation.{c,h}` owns the decision. Callers
receive the decision and the press context; none rebuilds the policy. It reads
the effective settings cache and the effective behaviour and combo views, never
storage, and costs one masked bit test per scope on an ordinary press.

- **At the press.** The userspace pre-process hook reads the press's source
  layer from QMK's source-layer cache
  (`users/noah/lib/compat/qmk_source_layer_contract.h`), asks for both
  decisions, retaining the physical press's context for its release. The
  source layer and both decisions are copied into BK's optional opaque
  `keyrecord_t.user_data` byte (`KEYRECORD_USER_DATA`), along with the resolved
  native keycode. Native tapping, combo buffering and userspace admission copy
  that context with each record. A later press at the same position cannot
  rewrite earlier queued records. QMK's synthesized tapping releases preserve
  the tapping press's context too.
- **Behaviour lookup.** The key runtime admits a press through its keycode's
  behaviour row only when the stored decision says so; otherwise it looks the
  keycode up without the row and takes the normal action. A combo's generated
  record asks the generated rule with the owner key's source layer.
- **Tap-hold.** QMK decides whether a record is a tap-hold through BK's
  `is_tap_record_user()`, a per-record weak hook given QMK's own answer and
  `is_tap_keycode_user()`'s. The userspace override keeps the keycode hook's
  answer for a press that uses its row and QMK's for one that bypasses it, so an
  authored behaviour on an `LT()` or `MT()` key that a placement bypasses
  returns to native QMK tap-hold timing.
- **Combos.** `combo_key_record_allowed()` in
  `users/noah/lib/compat/qmk_combo_origin.c` vetoes a member press unless its
  stored combo decision and the combo row both allow it on its source layer.
  Combo readout does not report it as a user trigger hook (flag bit 2): its
  decisions are the profile's own controls.
