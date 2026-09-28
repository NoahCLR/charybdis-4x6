# Hardware measurements

Raw captures taken on the keyboard, with the build and conditions needed to
trust them and to repeat them. Specs and plans quote these numbers; this folder
holds the evidence.

## Layout

```
measurements/
  README.md                         this file: layout and rules for every subject
  <subject>/                        one kind of measurement, one capture procedure
    README.md                       how to capture it dependably, and the set index
    YYYY-MM-DD-<label>/             one set: one firmware build, one sitting
      README.md                     the set record: build, conditions, results, notes
      <workload>-<run>.json         raw tool output, unedited
```

Subjects:

| Subject | Measures | Procedure |
| --- | --- | --- |
| [`pointing-cadence/`](pointing-cadence/README.md) | Master loop: pointing polls, poll gaps, loop stages, split transactions | [capture procedure](pointing-cadence/README.md#capture-procedure) |

## Rules

- **A set is one firmware build, flashed once, captured in one sitting.** A new
  build, a reflash, a profile change or a different host starts a new set.
- **Raw output is kept as written.** Never edit, trim or merge capture files. A
  bad capture is deleted before the set is recorded, and the set record says
  so.
- **Every set names its source.** Build from a committed, clean tree so the set
  can name the userspace commit; name the QMK commit too. A set built from
  uncommitted source cannot be repeated and is not recorded.
- **The label says what the set is for**, such as `stage-timing` or
  `no-coalescing`, so two sets can be compared by name. The date is the capture
  date.
- **Compare sets that differ in one thing**, and say what it is. The subject's
  README says which differences are comparable.
- **Conclusions live elsewhere.** A set record states facts and anomalies;
  what a result decides goes in the plan or spec it governs, linking the set.

## Adding a subject

Create `<subject>/README.md` with the capture procedure (build, conditions,
workloads, validity checks, how to compare) and an empty set index, and add a
row to the table above.
