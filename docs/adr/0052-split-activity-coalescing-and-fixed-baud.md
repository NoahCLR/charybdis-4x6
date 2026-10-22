# Split activity coalescing and a fixed split baud

Cited as D-F52. Was D-L43.

The default build coalesces split activity timestamps: it sends the latest
snapshot as often as the shortest lighting idle timeout needs, while keys are
still read every scan and every split protocol is kept
(`NOAH_SPLIT_ACTIVITY_COALESCE=no` builds without it). Both halves run a fixed
460,800-baud link with frame CRC on by default; the removed `NOAH_SPLIT_BAUD`
selector fails the build. Without CRC, 460,800 baud logged 20–30 transport
failures per Apply and made the other half's lighting flicker (2026-09-28),
so CRC came first; it rejects corrupted frames but cannot prevent them, and
halving frame time is not a measured gain in pointing rate. Contract:
[`split-activity-sync.md`](../architecture/split-activity-sync.md).
