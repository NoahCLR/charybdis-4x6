# Issue tracker: Noah's notes vault

Issues, specs and tickets for this repository live in Noah's private notes
vault, `charybdis-notes`, beside the main checkout. From any worktree:

```sh
"$(dirname "$(git rev-parse --path-format=absolute --git-common-dir)")/../charybdis-notes"
```

Its `_agents/tracker.md` maps every tracker operation the skills name
(publishing an issue, a spec or tickets, fetching a ticket, claiming, closing,
wayfinding) onto its notes, tasks and plans. Read it before running one of
those skills; the vault's `AGENTS.md` holds the rules for working in it.

GitHub issues are not used, and pull requests here are Noah's own work, not a
request surface. Pull requests are public and the vault is private: write a
pull request so it stands on its own, without quoting or linking the vault.

Without the vault (a machine that lacks it), there is no tracker: work from
Noah's instructions and this repository's `AGENTS.md`.
