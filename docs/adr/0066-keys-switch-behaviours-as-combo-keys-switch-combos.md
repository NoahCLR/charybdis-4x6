# Keys switch behaviours as QMK's combo keys switch combos

Cited as D-F66.

Behaviours already had a runtime override beside their stored master (D-F32),
but no key could flip it. `BH_ON`, `BH_OFF` and `BH_TOGG` now switch it the
same way QMK's `CM_ON`, `CM_OFF` and `CM_TOGG` switch its combo flag: on, off
or the other way, when pressed. Like the combo keys, they work until the
keyboard restarts, never touch the stored master and never save. They go
wherever the combo keys go: on a key, in every behaviour step, and as a
combo output.

The keys sit in their own aligned block, `0x7ee0..0x7eef` (D-F51), with
16 codes reserved and three used, so no other keycode moves. The rest of the
block and `0x7ef0..0x7eff` stay inert. They are a capability, so Profile Wire
goes to 2.1 (D-F28); the profile's format is unchanged and its schema stays
4.3. The block moves the end of the action vocabulary, so the action-ABI
digest changes too, and a profile saved under the previous digest is refused
(D-F10).

Two consequences follow from the participation rules rather than from new
ones. With behaviours off, a key takes its normal action, and a behaviours
key's normal action is its switch, so `BH_ON` and `BH_TOGG` always turn
behaviours back on, even with a row of their own. Switching off decides no
press again: a behaviour already holding something keeps it until its key is
let go. Contracts: [`participation-policy.md`](../spec/participation-policy.md),
[`pd-mode-domain-v1.md`](../spec/pd-mode-domain-v1.md#keycodes).
