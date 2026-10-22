# CI runs for releases; the release gate may run Ark's agreement check

Cited as D-F06.

Development is verified locally, so CI does not run on `dev` or on pull
requests into it. A release's `dev` → `main` pull request runs every check
`main` requires (`Promotion from dev`, `Host suite (GCC)`, `Pair build`,
`Agreement with Ark main`), nightly runs watch `dev` and BK drift without
blocking, and a release publishes exactly the pair `Pair build` made.
`Agreement with Ark main` is the one place firmware runs Ark's code: it runs
Ark's agreement check against this firmware, so `main` cannot move to a
contract the released Ark does not speak, not even through a merge on GitHub.
The independence test exempts that one workflow (D-F53).
