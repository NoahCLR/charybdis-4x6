---
status: superseded by D-F31
---

# VIA checksum work is independent of split frame size

Cited as D-F20.

Every Apply that touched VIA re-digested the whole VIA bank on both halves,
and the 14-byte split payload had also limited checksum work per scan, so the
expanded macro bank needed thousands of grants. The worker hashed up to 224
bytes per scan from QMK's RAM mirror instead, keeping full re-digests rather
than cached or partial ones. D-F31 removed VIA: there is no bank to checksum.
