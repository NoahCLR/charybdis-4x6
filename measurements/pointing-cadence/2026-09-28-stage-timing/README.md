# 2026-09-28-stage-timing

The first captures with loop stage timing: where the master's loop spends its
time, with activity coalescing on.

| | |
| --- | --- |
| Captured | 2026-09-28, Noah |
| Userspace | `ac9e9508` Time each stage of the master's loop in the cadence recorder |
| QMK | `6889960271` Add split activity admission hooks and optional transaction diagnostics; the ChibiOS and ChibiOS-Contrib submodules report local changes and `modules/drashna/` is untracked; both predate this work and were not inspected |
| Build | `NOAH_SPLIT_DIAGNOSTICS=yes NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS=yes sh tools/build-firmware-pair.sh` → `3_charybdis_{right,left}_diagnostic_cadence.uf2` |
| Recorder | format 2 |
| Split link | 230,400 baud; activity coalescing on |
| Profile | the compiled profile, unchanged after flashing |
| Host | Mac, macOS; port and cable not recorded |
| Lighting | profile default, awake (the lighting render runs in every capture) |
| Runs | one per workload: a spot check, taken before the three-run procedure |

## Results

|  | idle-1 | motion-1 | motion-typing-1 |
| --- | ---: | ---: | ---: |
| Pointing polls/s | 497 (496–498) | 448 (444–450) | 406 (396–421) |
| Matrix scans/s | 497 | 448 | 406 |
| Cadence windows | 10 | 10 | 9 |
| Split transaction share | 19.4% | 19.8% | 19.3% |
| Failed transactions | 1 | 6 | 7 |
| Longest poll gap | 9.3 ms | 9.2 ms | 10.2 ms |
| Poll gaps ≥ 5 ms per second | 3.1 | 3.5 | 8.8 |
| Poll gaps <1000us | 0.0% | 0.0% | 0.0% |
| Poll gaps <1250us | 0.0% | 0.0% | 0.0% |
| Poll gaps <1500us | 0.0% | 0.0% | 0.0% |
| Poll gaps <2000us | 73.0% | 61.4% | 38.8% |
| Poll gaps <5000us | 26.4% | 37.8% | 59.1% |
| Poll gaps >=5000us | 0.6% | 0.8% | 2.2% |

Stage: share of wall time · mean per loop · longest loop

| Stage | idle-1 | motion-1 | motion-typing-1 |
| --- | ---: | ---: | ---: |
| `matrixScan` | 35.1% · 706 µs · 2.2 ms | 35.7% · 798 µs · 7.0 ms | 34.0% · 837 µs · 2.7 ms |
| `durableIo` | 12.4% · 249 µs · 4.5 ms | 12.5% · 279 µs · 4.6 ms | 11.8% · 290 µs · 4.7 ms |
| `keyRuntime` | 0.9% · 19 µs · 78 µs | 0.9% · 21 µs · 60 µs | 2.6% · 63 µs · 531 µs |
| `splitSync` | 14.7% · 295 µs · 6.6 ms | 13.1% · 292 µs · 6.5 ms | 12.6% · 310 µs · 6.6 ms |
| `qmkTasks` | 8.0% · 162 µs · 274 µs | 7.8% · 175 µs · 295 µs | 9.2% · 226 µs · 2.4 ms |
| `processRecord` | 0.0% · 0 µs · 0 µs | 0.0% · 0 µs · 0 µs | 1.2% · 29 µs · 2.3 ms |
| `rgbRender` | 6.2% · 124 µs · 1.0 ms | 6.9% · 153 µs · 1.1 ms | 6.7% · 166 µs · 1.3 ms |
| `sensorRead` | 11.1% · 223 µs · 308 µs | 10.3% · 229 µs · 312 µs | 9.8% · 241 µs · 467 µs |
| `pointingTask` | 0.6% · 12 µs · 23 µs | 1.0% · 22 µs · 35 µs | 1.0% · 24 µs · 107 µs |
| `pointingReport` | 5.3% · 107 µs · 145 µs | 7.8% · 174 µs · 281 µs | 7.5% · 184 µs · 329 µs |
| `outsideKeyboardTask` | 5.7% · 115 µs · 310 µs | 4.1% · 90 µs · 191 µs | 3.8% · 92 µs · 206 µs |
| Coverage | 1.000 | 1.000 | 1.000 |

Split transaction: attempts/s · mean · longest

| Id | idle-1 | motion-1 | motion-typing-1 |
| --- | ---: | ---: | ---: |
| 0 | 497.3/s · 288 µs · 365 µs · 1 failed | 448.3/s · 299 µs · 395 µs · 2 failed | 407.7/s · 303 µs · 414 µs · 4 failed |
| 1 | 9.9/s · 379 µs · 435 µs | 9.9/s · 381 µs · 420 µs | 16.8/s · 378 µs · 427 µs · 3 failed |
| 2 | 9.9/s · 391 µs · 443 µs | 10.1/s · 448 µs · 5.1 ms · 2 failed | 9.9/s · 403 µs · 453 µs |
| 3 | 9.9/s · 354 µs · 435 µs | 9.8/s · 362 µs · 413 µs | 9.9/s · 364 µs · 424 µs |
| 4 | 9.9/s · 225 µs · 249 µs | 9.9/s · 215 µs · 290 µs | 10.1/s · 224 µs · 271 µs |
| 5 | 9.9/s · 208 µs · 304 µs | 9.9/s · 232 µs · 286 µs | 9.9/s · 219 µs · 275 µs |
| 6 | 9.8/s · 876 µs · 914 µs | 12.1/s · 910 µs · 5.1 ms · 2 failed | 11.6/s · 880 µs · 918 µs |
| 10 | 9.9/s · 737 µs · 802 µs | 29.8/s · 741 µs · 811 µs | 29.7/s · 742 µs · 807 µs |
| 11 | 9.0/s · 412 µs · 482 µs | 6.0/s · 402 µs · 459 µs | 8.1/s · 421 µs · 477 µs |
| 12 | 9.0/s · 1102 µs · 1.6 ms | 6.0/s · 1273 µs · 1.6 ms | 8.1/s · 1143 µs · 1.6 ms |
| 13 | 9.0/s · 197 µs · 210 µs | 6.0/s · 196 µs · 207 µs | 8.1/s · 197 µs · 214 µs |
| 14 | 9.0/s · 499 µs · 1.7 ms | 6.0/s · 645 µs · 1.7 ms | 8.1/s · 533 µs · 1.7 ms |

## Notes

- Stage coverage is 1.000 in every capture: the stages account for the whole
  loop.
- The motion capture's longest `matrixScan` loop (7.0 ms) coincides with two
  failed attempts each of ids 2 and 6, whose longest spans are 5.1 ms, the
  5 ms transaction timeout: a retried scan.
- The build was committed after the pair was built; the firmware sources of
  `ac9e9508` are the ones the pair was built from.
- Decisions from this set are in the *Split transport optimization* plan in
  the work-queue vault (`charybdis-notes`).
