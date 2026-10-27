# The pointer layer is QMK's auto-mouse, steered from outside

Cited as D-F60.

The pointer layer stays the layer QMK's auto-mouse turns on, with QMK's timer,
timeout and reset on a non-mouse key. The runtime only steers it: it tells QMK
which records are mouse records, holds one count on QMK's key tracker while
`layer_ownership` keeps the layer or a locked anchoring mode runs that does
not own QMK's toggle, lets a locked scrolling mode own QMK's auto-mouse
toggle, and corrects the layer state in `layer_state_set_user`: a
prefer-typing mode strips the layer, an auto-sniping layer or a mode its
layer started (D-F62) strips a separate pointer layer, and otherwise any anchor (QMK's toggle, its key tracker, an
anchoring mode) forces it on. QMK's own settings,
its lighting fade timer and its `MO()`/`TT()` accounting keep working
unchanged. The cost is coupling to QMK internals: QMK zeroes its tracker
whenever it resets auto-mouse (a live settings edit can), so either anchor
count is taken back only while the tracker holds one, and the
`layer_ownership` count is retaken when found gone; the locked mode's count is
not retaken, though the layer stays forced on while the mode runs. A mouse
button the runtime holds (`pointer_layer_policy_note_action`) is given back
without that check. A press resolved from the pointer layer must count as a
mouse record so QMK's two keycode lookups agree. Chosen over a userspace pointer-layer owner that would replace QMK's auto-mouse.
Path: [`pointer-report.md`](../architecture/pointer-report.md#the-pointer-layer);
contract: [`pd-mode-domain-v1.md`](../spec/pd-mode-domain-v1.md#pointer-layer-and-dpi).
