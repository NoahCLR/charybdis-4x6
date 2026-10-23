# A custom key can carry a shortcut, and that is its own action

Cited as D-F26.

Each custom key may carry a shortcut: one key plus any of the eight modifiers,
left and right mixed, or modifiers alone, or a key alone. It is the key's own
action, as an ordinary key's keycode is: without a behaviour row it is held
from press to release; with one, it is what unauthored cells tap, the
fallback hold, and what the key does when its row is bypassed. It is neither
a macro, which plays and finishes, nor a QMK modified keycode, whose
modifiers must sit on one side: it is held through the owned output a literal
modified keycode uses, which releases exactly what it took. A behaviour step
on another key may send a custom key and sends its shortcut only, never its
behaviour, so steps never chain (revising D-F51). Bytes:
[`profile-wire-v1/settings-domain.md`](../spec/profile-wire-v1/settings-domain.md#custom-key-shortcuts); behaviour:
[`key-press.md`](../architecture/key-press.md#from-an-action-to-the-report).
