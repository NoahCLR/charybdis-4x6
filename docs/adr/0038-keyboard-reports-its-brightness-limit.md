# The keyboard reports its brightness limit

Cited as D-F38. Was D-L17.

QMK clamps brightness to a compiled maximum, which readback did not show. The
keyboard now reports it (GET `0x08` page 1), so a client never infers it from
source or by writing test values, and can refuse a restore above the
destination's limit. Both halves refuse a profile whose auto-mouse fade delay
is not shorter than its timeout. Pages:
[`profile-wire-v1/settings-domain.md`](../spec/profile-wire-v1/settings-domain.md#editor-metadata--get-0x08).
