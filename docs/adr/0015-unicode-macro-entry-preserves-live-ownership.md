# Unicode macro entry preserves live ownership

Cited as D-F15.

Macro text is UTF-8. ASCII is always typed with ordinary keys and only the
characters that need it use the host's Unicode entry, so turning Unicode on
never makes an ASCII macro depend on the host's input source. Unicode entry
runs from the scan and holds its modifiers through the fork's report-only
modifier override, so the live modifier owners show again afterwards instead
of being released. The host OS and Unicode use are settings the user chooses;
the firmware does not detect the host's input source. Rules:
[`runtime-flow.md`](../architecture/runtime-flow.md).
