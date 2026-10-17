# Retired VIA-to-C bridge

[`tools/via_to_qmk_layout.py`](../../tools/via_to_qmk_layout.py) is retained for
historical inspection. It is deprecated, unmaintained and outside the required
checks. Its 64-macro model predates the current 128-slot bank, named
`VIA_MACROS` rows and `COMBO_WINDOW` declarations. Its write mode is not a
supported way to update this profile.

For live editing and portable backups, use
[Charybdis Ark](https://github.com/NoahCLR/charybdis-ark). For compiled defaults,
edit the authored files described in the [guide](../GUIDE.md#main-files), then
follow [Development](../DEVELOPMENT.md) for validation and the flashable pair.
The [profile introspector](PROFILE_INTROSPECT.md) generates documentation from
those authored inputs; it does not read the keyboard.

For a read-only look at an old VIA export, the legacy preview command is:

```sh
python3 tools/via_to_qmk_layout.py --print --via-json /path/to/export.json
```

Treat its output as historical conversion output requiring manual review, not
as a current-format profile or a lossless backup.
