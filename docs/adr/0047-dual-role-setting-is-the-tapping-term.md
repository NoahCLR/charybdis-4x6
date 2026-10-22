# The dual-role setting is the tapping term

Cited as D-F47. Was D-L38.

Editing the dual-role tap/hold setting reached only the runtime while QMK's
tapping engine kept the compiled `TAPPING_TERM`, so it changed almost nothing
a person could feel. The setting is now the one tapping term: QMK's per-key
tapping and quick-tap hooks answer with it, the compiled `TAPPING_TERM` is
only its default, and there is no per-key term (a behaviour row's own timing
still governs that row). Since D-F27 the runtime decides every dual-role key
and reads the setting itself; the QMK hooks remain only for `TT()` and `OSL()`
to layers 16–31, which no accepted keymap holds.
