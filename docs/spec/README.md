# Firmware specs

The contracts the firmware keeps with clients, with itself across the two
halves, and with its own flash. Each section states the contract and names the
test or fixture that holds it; a change to a contract changes its spec in the
same pass, and the decisions behind them are in [`docs/adr/`](../adr/).
Words are the [glossary](../../GLOSSARY.md)'s.

Read them in layers, from the report a client sends up to the save both halves
make:

| Layer | File | What it holds |
| --- | --- | --- |
| HID channel | [profile-wire-v1/](profile-wire-v1/README.md) | Profile Wire: the Raw HID reports, every value id, results, candidate transfer and live edits |
| Blob | [profile-wire-v1/blob-and-digests.md](profile-wire-v1/blob-and-digests.md) | The canonical profile blob, the domain registry, action encoding, digests |
| Domains | [key behaviours](profile-wire-v1/key-behavior-domain.md), [combos](profile-wire-v1/combo-domain.md), [settings](profile-wire-v1/settings-domain.md), [keymap](profile-wire-v1/keymap-domain.md), [macros](profile-wire-v1/macro-domain.md) | Each domain's payload and readback |
| | [pd-mode-domain-v1.md](pd-mode-domain-v1.md) | The pointing domain and how pointing modes run |
| | [rgb-domain-v1.md](rgb-domain-v1.md) | The lighting domain and how it is drawn |
| | [participation-policy.md](participation-policy.md) | When a press uses its behaviour or joins a combo |
| | [host-layouts-v1.md](host-layouts-v1.md) | The host layout catalogue macro text is typed through |
| | [field-classification.md](field-classification.md) | Which authored values are profile data and which only a build changes |
| Split | [profile-split-v1.md](profile-split-v1.md) | How the halves compare and copy saved profiles, candidates and the working copy |
| | [split-activity-sync.md](split-activity-sync.md) | The split link, activity and runtime state, and its diagnostic recorders |
| Transaction | [logical-profile-transaction-v1.md](logical-profile-transaction-v1.md) | How an Apply or Save becomes the next generation on both halves |
| | [authority-state-table.md](authority-state-table.md) | Which profile each half holds and runs, as states |
| Storage | [device-resident-profile.md](device-resident-profile.md) | Which profile a half runs, admitted formats, compiled defaults, where the profile is stored, ceilings |
| | [portable-profile-v1.md](portable-profile-v1.md) | The complete backup and how it is read and restored |
| Resources | [resource-policy.md](resource-policy.md) | Physical memory per half, the RAM and stack policies, how to report them |
</content>
</invoke>
