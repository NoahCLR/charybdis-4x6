# A stopped keyboard says so, and a client restarts it

Cited as D-F34.

After a storage, durability, authority, conflict, integration or
compiled-defaults error the profile owner stops, because only a fresh read of
flash is then known safe. A replug was the only way out, and it lost unsaved
edits. Now each half reports that it stopped in edit status, and a client can
restart both halves over Raw HID (SET `0x28`, with a confirmation word), after
keeping the unsaved edits itself to send back. A restart is a reset between
main-loop passes, exactly as safe as unplugging, which the marker-last rule
already makes safe anywhere in a save. It is never automatic, since only a
client can keep what is unsaved. A USB half restarted alone tells the other
half it holds no prepared push, so an abandoned prepared copy is released and
the halves rejoin.

Considered: a lease on prepared copies (a sender that is there pauses
silently at the barrier), restarting the owner in place (a stop can come from
state it shares; only a reset starts all of it from flash), and a feature bit
(D-F28). Contracts: [`profile-wire-v1.md`](../architecture/profile-wire-v1.md), "A stopped
keyboard, and Restart"; [`profile-split-v1.md`](../architecture/profile-split-v1.md).
