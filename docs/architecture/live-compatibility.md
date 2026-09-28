# Live compatibility runner contract

The five cross-language host runners (portable editor, portable profile, macro
program size, compiled defaults v1, and PD v1) import app code from the canonical
`CHARYBDIS_LIVE_ROOT` exported by `tests/host/noah_host_live_env.sh`.
An explicit missing/empty path fails. When unset, the firmware suite temporarily
uses `tools/charybdis-live` until extraction cleanup; source remains in place.

The independent Live repository owns `scripts/check-compatibility.js`, invoked
through `npm run test:compat`. It requires explicit firmware, Live, QMK and
report paths, validates checkout roots and bridge-aware runners, and records
starting revisions/dirty states and sequential runner results in JSON. It sets
QMK_ROOT and QMK_HOME for the selected QMK tree and stops on first failure.
Before merging wire/schema or cross-language codec changes, run this bridge
against the working copies under review. Freeze checkouts during the run. For
reproducible CI, use clean, pinned revisions and retain the report and logs.
Ordinary UI changes require only Live's independent suite. Neither this bridge
nor its fixture tests replace full firmware tests or hardware acceptance.
