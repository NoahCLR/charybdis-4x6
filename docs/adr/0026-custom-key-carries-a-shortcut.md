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

Amended (Noah, 2026-10-25): a shortcut may also name any set of layers, each
part optional, so layers alone are a shortcut. They are part of the key's own
action, so everything that sends it brings them: the key, an empty tier of its
behaviour, another key's behaviour and a combo whose output it is.
Held, each layer is owned through layer ownership by the key holding it, as
`MO()` owns its layer, beside the locks, other keys and a pointing mode's key,
so on release a layer goes off only when no other owner keeps it; layer
ownership records the layers the key took, so a shortcut changed while held
releases exactly those. A shortcut holding the pointer layer keeps it on for
auto-mouse, as MO() of it does. Tapped, its layers are on only for the tap. They are ordinary layer-on
events: a layer that starts a pointing mode (D-F62) starts it, and a hold that
sends such a shortcut changes other keys, as a layer hold does (D-F27). The
base layer and the default layers are always on, so the settings domain refuses a
shortcut that names one; it states the default layers itself, so it can, where
the pointing domain can only leave such a link inert.

Amended (Noah, 2026-10-26): a shortcut's layers act as any one of the layer
keys, all of them together, its keys and modifiers working as before beside
them. Hold is `MO()`, as above. Lock is `TG()` of the set: a press, or a tap,
locks every layer, or releases every one when all are locked already. Move to
is `TO()` of the set: its layers lock, on before any other lock releases, and
every other lock and the one-shot go. Tap-toggle and One-shot make the key a
dual-role key, as `TT()` and `OSL()` are: its layers are on from every press,
the key's own momentary hold, and without an authored hold so are its keys
and modifiers, so a key rolled in while it is down goes out at once on them,
as `LM()`'s do; the key's own tap is then left only what it does to the
layers, sent as the runtime's own `CUSTOM_KEY_LAYER_TAP(n)` (`0x7f80..0x7fff`,
never stored, refused everywhere a profile places an action): One-shot arms
them for the next key, which uses them up, a quick second tap cancelling them,
and Tap-toggle's `TAPPING_TOGGLE`-th tap locks them as Lock does. A One-shot
key's own press does not use the one-shot up, as `OSL()`'s does not. Sent by
another key's behaviour as a tap or by a combo's tap, Tap-toggle's layers are on for the tap
as Hold's, and One-shot arms them after the keys, so the keys cannot use them
up; held there, both hold them. A key's own momentary hold of its layers and
a custom key its behaviour holds are separate owners at that key, so neither
lets go of the other's layers, and the key runtime leases each of the
momentary layers, so it counts as holding a layer as `TT()` and `OSL()` do. A
custom key with layers keeps the pointer layer up while it is held, as QMK
lets a layer key do. One kind for the whole set keeps one picker
choice and one byte; a kind per layer was weighed and rejected for six bytes
per record and a picker and reach model to match. `DF()` and `PDF()` stay
refused; `LT()` is what a behaviour is for.

Each record grew from two bytes to five (settings version 10, schema 4.2):
modifiers, key, a 16-bit layer mask, then the layer kind, Hold without
layers. A sparse table of only the keys with
layers was weighed and rejected: the active settings live in one RAM copy
sized for the settings domain's own maximum, and a sparse table at its
maximum (128 entries of slot, mask and kind, four bytes each, and a count) would
claim more of that copy than the 384 bytes the fixed records add, while
costing a variable offset for every name after it and a search on every
press. Layer ownership adds a table of the keys holding shortcut layers, one
entry per matrix position with a held and a momentary owner (360 bytes per
half), and the key runtime's intent table shrinks by 184 bytes now that layer
locks and one-shots are masks; measured, 608 bytes of static RAM per half
over the build before shortcut layers. The fixed record keeps each
shortcut at a known offset, read in constant time, and validated a byte at a
time like the rest (resource policy: a conservative design on its own merits,
not a claim of capacity).

Amended (Noah, 2026-10-27): a shortcut holds up to six keys in place of one,
all held together, beside its modifiers and its layers, which are unchanged.
A key is a basic keycode for a usage the host sees: a keyboard key, a system
or consumer (media) key, or a mouse key that moves, clicks or scrolls; mouse-key
acceleration sends nothing of its own, only changing how fast the other mouse
keys move, and stays out, as do macros, QMK functions,
pointing modes and dual-role keycodes, which are a behaviour's job. Held, the
modifiers go down first, then each key in order; they come back up in
reverse, the keys last first, then the modifiers. Tapped, all of them go down
and come back up inside the one tap. Six is the 6-key report's limit, whatever
the build: NKRO was weighed and rejected, so a profile means the same on every
build. The system and consumer reports carry one usage each, so a shortcut
holds at most one system key and one consumer key. The modifiers stay held, as
before; a One-shot or locked modifier was weighed and rejected as a behaviour's
job. QMK's Layer Lock key (`QK_LLCK`) is not a layer kind.

Each record grew from five bytes to ten (settings version 11, schema 4.3):
modifiers, six key bytes packed from the first, then the layer mask and kind.
A shared pool of keys was the fallback if fixed records did not fit; they do:
with the settings RAM copy grown by the longer records, and each held action's
lease grown to hold every key (a held custom key keeps the modifiers and keys
it pressed, so a shortcut changed while held releases exactly that), each
half's linked static RAM stays under the
[resource policy](../spec/resource-policy.md)'s tripwire, so there is no pool.
That tripwire is a regression line, not capacity, and no runtime high-water
was measured. Fixed records keep every key at a known offset, validated a byte
at a time like the rest, where a pool would add a count, a variable offset and
a second place a shortcut lives.
