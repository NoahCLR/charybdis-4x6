# A macro holds at most six keys besides its modifiers

Cited as D-F65.

The keyboard report has six key slots, and this build has no NKRO. A macro
may therefore never have more than six keyboard keys down at once. That
counts its presses still held, a chord's keys and a character it types, all
together; a key a chord names that is already held, or names twice, counts
once. Its modifiers have the report's modifier byte, and system, consumer and
mouse keys have their own reports, so none of them count. A character is
typed as a key, so text, and a character the host layout types, needs a slot
the held keys leave free, even when its key is one of them: the rule stays one
a client can check without knowing each layout's keys. Preflight checks
this over every window before any output (D-F59), and a macro that breaks it
plays nothing. A stored macro is never refused when a profile is applied,
so a macro saved before this rule still loads and simply doesn't play.

Before this, a macro could hold up to 16 keys, the size of the engine's
lease tables. While a macro plays, the report filter fills the six slots
with the lowest usages it holds, not the first pressed. So a seventh key
pushed another key out, and the computer saw it let go, and letting go of
the seventh brought that key back as a new press. A 14-key chord typed
stray characters instead of a chord. Six applies whatever the build, as it
does for a custom key's shortcut (D-F26). That was chosen over a limit that
grows with NKRO, which no shipped build enables. The 16 stays as the
engine's table size and the limit on every key held, modifiers, system,
consumer and mouse keys included.
Path: [`macro-domain.md`](../spec/profile-wire-v1/macro-domain.md#validation-and-playback-checks),
[`macro-playback.md`](../architecture/macro-playback.md#finding-the-macro).
