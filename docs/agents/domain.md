# Domain docs

How the engineering skills consume this repository's domain documentation.

## Before exploring, read these

- **`GLOSSARY.md`** at the repository root: the device and wire terms.
- **`docs/adr/`**: the decisions that touch the area you are about to work in.

If either does not exist yet, proceed silently. Don't flag its absence or
suggest creating it upfront; `/domain-modeling` creates them when a term or a
decision is actually settled.

## Layout

Single-context:

```
/
├── GLOSSARY.md
├── docs/adr/            firmware decisions: 0001-<slug>.md, …
├── docs/spec/           contracts: Profile Wire, the split protocol, storage, domains
└── users/noah/, keyboards/bastardkb/charybdis/4x6/keymaps/
```

## Decision citations

Code, tests and docs cite a firmware decision as **D-F\<n\>**: D-F07 is
`docs/adr/0007-*.md`. A new ADR takes the next number and is cited the same
way. An ADR first numbered as an Ark decision says so (`Was D-L08.`).

**D-L\<n\>** citations are Charybdis Ark's decisions, recorded in Ark's
repository. Leave them as they are; change such a decision there.

## Use the glossary's vocabulary

When your output names a domain concept (in an issue title, a refactor
proposal, a hypothesis, a test name), use the term as `GLOSSARY.md` defines
it. Don't drift to synonyms the glossary explicitly avoids.

If the concept you need isn't in the glossary yet, that's a signal: either
you're inventing language the project doesn't use (reconsider) or there's a
real gap (note it for `/domain-modeling`).

## Flag ADR conflicts

If your output contradicts an existing ADR, surface it explicitly rather than
silently overriding it:

> _Contradicts D-F27 (what a key's hold decides), but worth reopening because…_
