# Behaviour and combo participation

Whether a press uses its keycode's behaviour and whether it can join a combo
is decided by one firmware policy (D-F14,
[ADR](../adr/0014-bigger-profiles.md)): the combo decision at the physical
press, the behaviour decision when the press is delivered. The stored
switches are in the [settings domain](profile-wire-v1/settings-domain.md)
(scalars and layer records), the [key-behaviour domain](profile-wire-v1/key-behavior-domain.md)
(row flags and allowed layers) and the [combo domain](profile-wire-v1/combo-domain.md)
(row flags and allowed layers); the bits of the combo-output switches are
[that domain's](profile-wire-v1/combo-domain.md#combo-output-behaviour-permission-encoding).

## Scopes

| Scope | Behaviours | Combos | Stored in |
| --- | --- | --- | --- |
| Keyboard | behaviour master, scalar 28, and the behaviours override | combo master, scalar 20, and QMK's combo flag | settings |
| Layer | scalar 29, one bit per layer | scalar 30, one bit per layer | settings |
| Definition | the behaviour row: enabled, allowed layers | the combo row: enabled, allowed layers | key-behaviour and combo domains |
| Placement | the layer record's bypass bit for the position | the layer record's exclusion bit for the position | settings layer records |

Every scope must allow it; any one that does not vetoes it, and a lower scope
cannot overrule a higher one. Behaviour and combo participation are
independent. Each override is a runtime toggle beside its stored master,
starts allowed at boot and is never saved
([toggles](profile-wire-v1/live-edits.md#toggles-and-the-settings-qmk-holds)).

Switching something off keeps it: a disabled behaviour row keeps its branches,
timing and layers, a disabled combo its inputs, output and window, and
switching it on again restores them unchanged.

The compiled defaults allow everything: both masters on, every layer bit set,
every definition enabled on every bank layer, every placement bit clear.

Held by: `tests/host/run_participation_tests.sh` (`participation_test.c`, `test_defaults_allow_everything`, `test_master_and_definition`, `test_overrides_beside_the_masters`, `test_placement`).

## Source layer and placement

A press's source layer is the layer that supplied its keycode: QMK's
`layer_switch_get_layer()` for that position, which falls through `KC_TRNS`
to the highest active layer with a key that is not transparent. It is read
from QMK's source-layer cache (`users/noah/lib/compat/qmk_source_layer_contract.h`),
the one QMK releases the key from, and never inferred from the keycode, which
can sit on several layers. A placement is the pair (source layer, matrix
position); transparent entries above the source layer are not placements and
veto nothing. `KC_NO` stops the fall-through and sends nothing.

With layers 1, 2 and 3 active, 3 highest, and behaviours and combos off on 2:
a key on 3 uses layer 3's permissions; a transparent key on 3 over a key on 2
uses layer 2's, so it has neither; transparent keys on 3 and 2 over a key on 1
use layer 1's.

Held by: `tests/host/run_participation_tests.sh` (`participation_test.c`, `test_source_layer_decides`).

## Behaviour decision

A physical press uses its keycode's behaviour row only if:

1. the behaviour master and the behaviours override allow it;
2. its source layer's bit is set in scalar 29;
3. the row is enabled and its allowed layers include the source layer;
4. the placement's bypass bit is clear.

Otherwise the press takes its normal action: the keycode as if it had no
row, with the usual modifier, layer and pointing ownership. An `LT()`, `MT()`
or `OSM()` key stays the key runtime's and keeps its default, as without a row
(D-F27, [ADR](../adr/0027-a-keys-hold-decides-how-later-keys-meet-it.md)). A
custom key's normal action is its shortcut, held from press to release (D-F26,
[ADR](../adr/0026-custom-key-carries-a-shortcut.md)); without one it does
nothing. One row serves every placement of its keycode, so bypassing it at one
placement leaves the others as they were.

The decision is held by the press's owner through its release and any pending
gesture. A layer change while the key is held cannot move it to another owner
or lose its release.

Held by: `tests/host/run_participation_tests.sh` (`participation_test.c`), `tests/host/run_pd_mode_key_runtime_integration_tests.sh` (`pd_mode_key_runtime_integration_test.c`, `test_qmk_bypassed_authored_layer_tap_keeps_default`, `test_qmk_bypassed_custom_key_does_nothing`, `test_qmk_tapping_engine_classifies_no_supported_key`).

## A press that waited

A press can wait in QMK's tapping queue, its combo buffer or the firmware's
record admission while a tap/hold key decides. QMK resolves it again when it
delivers it, on the layers active then. If the press now resolves from
another layer, its behaviour decision is made again on that layer with the
same four scopes; its combo decision stays the physical press's.

Holding `LT(Nav, F)` past its term with `J` pressed meanwhile types Nav's key
at J's position under Nav's settings; releasing the LT first makes it a tap,
and J resolves from the base layer under the base layer's settings.

A press is settled once, the first time it is delivered, and its release takes
that decision, even when the position is pressed again before the release is
delivered. A delivered press that record admission holds back settles again
when it is replayed.

Held by: `tests/host/run_pd_mode_key_runtime_integration_tests.sh` (`test_qmk_queued_participation_is_per_press`, `test_qmk_waiting_key_participates_on_hold_layer`, `test_qmk_waiting_key_uses_hold_layer`, `test_qmk_buffered_member_uses_hold_layer`), `tests/host/run_participation_tests.sh` (`test_record_context_is_immutable`).

## Combo decision

QMK asks `combo_key_record_allowed()` for every member record of every combo
holding the press's keycode (after reference remapping). The press counts
toward a combo only if:

1. the combo master (scalar 20) allows it, and QMK's combo flag, which QMK
   checks itself;
2. its source layer's bit is set in scalar 30;
3. the combo row is enabled and its allowed layers include the source layer;
4. the placement's exclusion bit is clear.

Conditions 2 and 4 and the source layer are taken at the press and copied to
its release. A veto skips every member-state update, releases included, so an
excluded duplicate of a keycode cannot clear an eligible press's member bit.
The members of one chord may come from different source layers; each must
pass on its own. A press that passes no combo is not buffered. Reference
remapping can change the keycode a combo matches; it never replaces the
source layer as the permission context. A policy change while a chord is
partly pressed settles ordinary keys once, and an active combo's output still
gets its release.

Held by: `tests/host/run_participation_tests.sh` (`participation_test.c`, `test_combo_rows`), `tests/host/run_pd_mode_key_runtime_integration_tests.sh` (`test_qmk_excluded_duplicate_preserves_chord`, `test_qmk_excluded_member_keeps_chord_from_forming`), `tests/host/run_qmk_combo_origin_tests.sh` (`qmk_combo_origin_test.c`).

## Combo outputs

Placement applies to physical presses only; a combo's output and a
behaviour's generated records have no placement. Whether a combo fires and
whether its output uses its keycode's behaviour are independent (D-F24,
[ADR](../adr/0024-combo-output-behaviour-permission.md)). The output uses its
behaviour only if:

1. the behaviour master and override allow it;
2. the combo-output switch (settings header byte 5) allows it;
3. the combo's own output switch (its row flag) allows it;
4. the output's behaviour row is enabled and its allowed layers include the
   combo's origin layer.

The layer behaviour switch and placement bypass bits do not apply. A veto
sends the keycode's normal action, a custom key its shortcut or nothing. Other
combos with the same output, and physical placements of it, keep their own
permissions.

The origin layer is the source layer of the chord's last declared input,
whatever the press order, never a union of its members' layers. Members on
other layers decide only whether the chord fires; remapping to a reference
layer does not change the origin. Only presses eligible for the combo count
as its members, so an excluded duplicate adds neither ambiguity nor footprint.

The permission is taken when the chord completes, before QMK can queue its
output behind native tapping, and travels with the output's record through
tapping and admission to its release. Each delivered output takes the oldest
fired completion not yet delivered, so two chords with one output that wait
together keep their own decisions
([pairing](../../users/noah/lib/compat/README.md#combo-origin-pairing)). A change
to a switch, a definition or the layers cannot reclassify a completed output
or lose its release; a suppressed completion drops its permission. When the
origin cannot be reconstructed, the output bypasses its behaviour if any
combo with that output bypasses it.

Held by: `tests/host/run_participation_tests.sh` (`participation_test.c`, `test_generated_outputs`, `test_combo_output_permissions`), `tests/host/run_pd_mode_key_runtime_integration_tests.sh` (`test_qmk_combo_output_permissions`, `test_qmk_same_output_chords_wait_together`, `test_qmk_generated_custom_uses_owner_layer`, `test_qmk_combo_output_gui_behaviour`), `tests/host/run_qmk_combo_origin_tests.sh` (`qmk_combo_origin_test.c`, `test_output_permission_survives_queues_and_release`), `tests/fixtures/combo_output_behaviors_v1.fixture`.

## Where it runs

`users/noah/lib/key/behavior/participation.{c,h}` owns the decision; callers
pass the press and act on the answer. It reads the effective settings,
behaviour and combo views, never storage.

- **At the press**, the pre-process hook reads the source layer, asks both
  decisions and writes them into BK's opaque `keyrecord_t.user_data` byte
  (`KEYRECORD_USER_DATA`). QMK's tapping, its combo buffer and record
  admission copy the byte with each record; a release copies its press's, and
  a later press at the same position cannot rewrite earlier queued records.
  The keycode is not frozen in the record.
- **At delivery**, the press settles the first time it leaves the queues:
  in `is_tap_record_user()`, with the keycode and layer QMK resolves now, or
  in the process hook, whichever comes first.
- **Behaviour lookup** goes through the row only when the record says so.
- **Tap-hold**: `is_tap_record_user()` keeps `is_tap_keycode_user()`'s answer
  for every record, so a bypassed `LT()`, `MT()` or `OSM()` key answers as it
  does with its row, and QMK's tapping engine classifies none of them.
- **Combos**: `combo_key_record_allowed()` in
  `users/noah/lib/compat/qmk_combo_origin.c` applies the combo decision.
  Combo readback does not report it as a trigger hook of the user's own
  ([GET `0x06`](profile-wire-v1/combo-domain.md#native-combo-readback--get-0x06)).

The `user_data` byte:

| Bits | Physical record | Combo-output record |
| --- | --- | --- |
| 7 | valid | valid |
| 6 | settled at delivery | generated |
| 5 | may join combos | origin normalised |
| 4 | uses its behaviour | uses its behaviour |
| 0–3 | source layer | origin layer |

Held by: `tests/host/run_participation_tests.sh` (`participation_test.c`, `test_record_context_is_immutable`), `tests/host/run_pd_mode_key_runtime_integration_tests.sh` (`test_qmk_queued_participation_is_per_press`, `test_qmk_combo_origin_survives_delivery_after_deactivation`).
