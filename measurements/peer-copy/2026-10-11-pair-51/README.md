# Pair 51: unchanged committed profile

Both halves run the existing dev pair 51, userspace `00bc0f5c` and pinned BK
`173eae2b5e1b21c15a84ea90eb8fe84b434c63c5`. Captured on Noah's Mac with right
USB/master and the idle left peer. No flash occurred during the capture.
The current 3,760-byte custom profile was restored unchanged through the
client's ordinary portable restore, with a recovery copy outside the repo.
The scratch probe polled candidate peer status about every 3 ms between HID
requests and added retry count and BUSY streak to its original transition
log. Raw stdout is retained unchanged. Procedure: [peer copy](../README.md).

| Observation | Run 1 | Run 2 |
| --- | ---: | ---: |
| Whole Apply | 12,703 ms | 12,578 ms |
| Client's peer step | 844 ms | 850 ms |
| First observed SENDING → PREPARING | 76 ms | 80 ms |
| First observed PREPARING → PREPARED | 718 ms | 719 ms |
| New SENDING retry count | 9 | 9 |
| New PREPARING retry count | 4 | 4 |
| New transport failures | 0 | 0 |

In each run the observed transfer offset jumped from 0 to 3,760 without
intermediate acknowledged offsets. SENDING reported STORE_WORKING with a
RECEIVING store. On this build that combination is the page-1 mapping of
split COPYing BUSY replies. The nine busy collections plus one final reply
are consistent with one admitted reuse range, not 35 separately admitted
110-byte payload chunks. Each range itself still requires at least 35
receiver copy steps. Page 1 cannot report exact reused-byte or receiver-scan
counts, or retrospectively identify a source-unavailable reply. Page 2 did
not yet exist in pair 51.

PREPARING observations show exponential polling intervals around 100, 200
and 400 ms after its initial admission. The receiver's exact completion time
is not observable in these captures: 718–719 ms is time until the sender
collects readiness, not measured validator execution time. The previously
reported 1.23-second SENDING run was not reproduced in either capture.

The failure counter was already 6 before each Apply and remained 6; the
pre-existing failures are not failures of these runs.
