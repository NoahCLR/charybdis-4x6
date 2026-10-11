# Unchanged candidate upload — baseline

| Source | Value |
| --- | --- |
| Firmware on keyboard | `00bc0f5c489fa2158a8164fdb7f276883e902acd`, dev pair 51 |
| BK pin | `173eae2b5e1b21c15a84ea90eb8fe84b434c63c5` |
| Client | Charybdis Ark `2abdaffda9b87888959ad54f44a0da163ffc310b` |
| Workload | Re-Apply the keyboard's coherent current document without edits |
| Profile | 3,760 bytes; CRC32 `824083ce`; FNV-1a `ffa224ac` |
| Conditions | macOS, both halves connected, keyboard idle, shared lock held; no independent page-1 poller |
| Hardware changes | No flash; authorized unchanged re-Apply only |
| Raw hardware trace | [upload-trace.json](upload-trace.json) |
| Replay output | [host-timing.txt](host-timing.txt), ordinary then ASan/UBSan |

The trace was written directly by a local copy of the Apply probe. The wrapper
records the client's existing completed requests during its upload step. Times
are milliseconds from the start of the probe, including the initial complete
profile read. The host replay uses the diagnostic mode added with this set;
the firmware runtime sources are identical to the flashed baseline. The exact
replay blob was read locally and matched to the trace's length and both
checksums. It is not published because it contains personal configuration.

## Observations

The upload step ran from 6,281 to 8,732 ms: **2,451 ms**. It sent 217 requests:
166 candidate-status polls, four REUSE requests, one BEGIN, one VALIDATE and
45 identity/read requests. Total request time was 723 ms; the remainder
includes polling waits and device progress between requests. The next
validation-labelled client step completed immediately because the upload step
already waited for VALIDATED.

| Boundary | Time, ms | Interval, ms |
| --- | ---: | ---: |
| BEGIN completed | 6,289 | |
| RECEIVING first observed | 6,294 | |
| First REUSE completed | 6,439 | |
| COMPLETE first observed | 7,294 | 855 from first REUSE |
| VALIDATE completed | 7,298 | 859 from first REUSE |
| VALIDATING first observed | 7,304 | |
| VALIDATED first observed | 8,732 | 1,434 from VALIDATE |

The four ranges contain 1,024, 1,024, 1,024 and 688 bytes. They require
`52 + 52 + 52 + 35 = 191` pieces, hence **382 copy grants**. The real host
replay counts 384 transfer/setup grants with BEGIN and VALIDATE included.
Validation takes **622 grants**: 188 for checksums, seven for blob/envelope
steps and 427 for semantic decoding:

| Domain | Grants |
| --- | ---: |
| RGB | 62 |
| Key behaviors | 230 |
| Combos | 31 |
| Settings | 47 |
| Pointing modes | 57 |

With the outer scheduler's VIA worker idle, the replay takes **1,006 matrix
scans**. With a continuously ready injected VIA worker, it takes **2,011**
scans while candidate work stays identical. The trace's copy and validation
intervals each imply roughly **2.3 ms per candidate grant**. This is consistent
with the current bounded work explaining the observed delay; it is not a
hardware scan-period measurement.

VIA staging starts at 8,732 ms, after candidate validation. It took 3,132 ms;
later local VIA roll-forward took 5,112 ms. These are separate phases. The
candidate upload begins from ready VIA storage and makes no VIA mutations, so
its delay is not the full-bank digest triggered during those later phases.

## Limits

No cadence recorder was active: the trace cannot count actual matrix scans or
separate individual key, pointing, RGB, flash and transport costs. The replay
shows the required work without those costs. Status polls include client waits
and HID completion latency. This set does not establish changed-upload,
interruption or input-cadence acceptance, nor measure a faster firmware build.
