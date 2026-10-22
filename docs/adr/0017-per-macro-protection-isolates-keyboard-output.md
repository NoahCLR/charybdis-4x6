# Per-macro protection isolates keyboard output

Cited as D-F17.

Each macro may override automatic input protection (Automatic, On or Off),
stored with the macro. Automatic protects a macro that needs Unicode entry
under the layout latched when it starts, since a stray key would corrupt the
entry; natively typed text stays interruptible. While a macro is protected,
new physical presses are ignored until released and never replayed, keyboard
output carries only the macro's keys, and held modifiers resume afterwards.
Rules: [`runtime-flow.md`](../architecture/runtime-flow.md).
