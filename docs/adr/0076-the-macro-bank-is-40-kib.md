# The macro bank is 40 KiB

Cited as D-F76.

The macro bank was 34,903 bytes: what VIA's dynamic macro region held once
the 36 KiB VIA partition of D-F14 had taken its settings and the 1,920-byte
keymap. VIA is gone (D-F31) and profiles have their own flash region, so the
number described a layout that no longer exists, and it read as arbitrary
beside the autocorrect list's round 20,480 bytes of text. Noah asked for a
round bank, twice the list's text (2026-11-16). Contract:
[`macro-domain.md`](../spec/profile-wire-v1/macro-domain.md).

**The ceiling.** The bank is 40,960 bytes, 6,057 more than before, still
shared by the 128 slots, and one macro can still use all of it. The
encoding is unchanged; only the counted ceiling (D-F14) and what capability
page 2 reports for it (bytes 14–17) move, so no domain or schema version
changes. Every bank the old ceiling held still fits. A profile with a bank
past 34,903 bytes is refused by older firmware, by the same ceiling check as
any other over-limit profile.

**The reserve.** The maximum profile grows from 116,783 to 122,840 bytes,
leaving 7,208 below the 130,048-byte payload ceiling. The reserve kept for
later additions, such as per-key lighting, shrinks from 12,288 bytes to
7,168 (7 KiB), so 40 bytes remain above it. Raising another ceiling later
takes from the reserve, as this one would have; the storage layout and the
payload ceiling are unchanged.

**Cost.** The whole-profile validator reads 20 bytes a step, so each pass
over a full bank takes about 303 more steps. Validating a received profile
takes up to 12 steps a scan (`NOAH_PROFILE_CANDIDATE_VALIDATION_CALLS`), so
a few dozen more scans; boot selection takes one step a scan, so a few
hundred more; the working copy takes up to 64
(`NOAH_PROFILE_WORKING_VALIDATION_STEPS`), so a full bank in about 32 scans.
A split prepare's longest stretch without transfer progress grows with the
first and stays inside the owner's no-progress window
(`profile_domain_owner_round_trip_test.c`, `test_maximum_profile`). Playback
keeps its one 512-byte window (D-F59), so RAM is unchanged; finding the last
slot of a full bank walks 6,057 more bytes. Every 16-bit macro position and
length stays below 65,536.

Three other numbers were weighed and left. Keeping 34,903 kept the reserve
but not the round number. 32,768 was round and freed room, but went below
what VIA could hold. 40,960 with the reserve at 12,288 does not fit the
payload.

**Ark follow-up:** re-pin, and raise the macro codec's ceiling to 40,960;
Ark reads the bank's size from capability page 2 and shows it as Macro
memory.
