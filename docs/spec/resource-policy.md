# Resource policy

What each half's RP2040 physically has, the policies the firmware's RAM and
stack use are checked against, and how to report either. Every figure is per
half: each half runs its own copy of the firmware on its own RP2040, and the
halves share no memory. A policy is a regression line the project chose, never
the chip's capacity; a policy's remaining margin is not hardware headroom.
Where the profile lives in flash, the slot record and the profile's ceilings
are the [device-resident profile](device-resident-profile.md)'s.

## Physical memory per half

Each RP2040 has 270,336 bytes (264 KiB) of SRAM in six banks. The linker
script is BK's `platforms/chibios/boards/common/ld/RP2040_FLASH_TIMECRIT.ld`,
for the target `keyboards/bastardkb/charybdis/4x6/keyboard.json`.

| Linker region | Physical SRAM | Bytes | Holds |
| --- | --- | ---: | --- |
| `ram0` | SRAM0–3, word-striped | 262,144 | `.data`, `.bss`, small RAM-resident sections, then the core-memory span |
| `ram4` | SRAM4 | 4,096 | the interrupt stack, the process stack, core 0's RTOS state |
| `ram5` | SRAM5 | 4,096 | core 1's reserved interrupt and process stacks |
| `ram7` | the last 256 bytes of SRAM5 | (256) | the boot window; part of SRAM5, not extra memory |

The memory gate refuses an ELF whose banks are not these sizes, whose
core-memory span does not end at the end of SRAM0–3, or whose `ram7` does not
end with SRAM5, and its accounting counts `ram7` once, inside SRAM5.

Held by: `tests/host/run_firmware_memory_budget_tool_tests.sh`
(`firmware_memory_budget_tool_test.py`,
`test_accounts_for_unique_rp2040_banks_without_double_counting_ram7`,
`test_rejects_non_rp2040_physical_bank_capacity`).

### SRAM0–3

The fixed linked prefix runs from the bank's start to `__heap_base__`: all of
`.data` and `.bss`. The largest static claims in it are the working copy,
`runtime_working_bytes`, one slot payload (130,048 bytes,
`NOAH_PROFILE_PAYLOAD_MAX`) of `.bss` (D-F32,
[`0032`](../adr/0032-live-edits-working-copy-and-save.md)); QMK's RAM mirror
of the 4 KiB logical EEPROM (D-F31,
[`0031`](../adr/0031-via-is-not-supported.md)); and the split slave thread's
1,200-byte working area. The working copy is static, never allocated, so the
core-memory span says exactly what is left. No RAM copy of a profile slot
exists: slots are read in place through XIP.

### SRAM4 and SRAM5

SRAM4 holds, in order, the interrupt stack (QMK's ChibiOS default
`USE_EXCEPTIONS_STACKSIZE`, 1,024 bytes) and the process stack `main` runs on
(`USE_PROCESS_STACKSIZE = 0xA00`, 2,560 bytes, set in `users/noah/rules.mk`),
then core 0's RTOS state; the memory gate reports what remains unassigned. The
process stack grows down toward the interrupt stack, so a process-stack
overflow lands where the next interrupt pushes its frames. SRAM5 holds the
core-1 stacks the startup code reserves at the same sizes; `ram7`, its last
256 bytes, is reserved for the boot window.

## The core-memory span

The core-memory span is `__heap_end__ - __heap_base__`: the rest of SRAM0–3
after the fixed linked prefix, at boot. ChibiOS initialises its core allocator
from it, and the linked newlib `_sbrk_r` takes memory from that allocator. It
is the allocator's range at boot, not memory guaranteed unused and not a
runtime high-water measurement. The only runtime allocation an ELF analysis
of this firmware found is newlib `rand()`'s one-time state, from the lighting
task; ChibiOS heap allocation is not linked in.

Held by: no test. The allocation claim is an ELF analysis recorded in
`tools/check_firmware_memory_budget.py`'s docstring; allocator high-water on
both halves is hardware evidence no tool measures.

## Static RAM policies

`tests/host/run_firmware_memory_budget_checks.sh` runs
`tools/check_firmware_memory_budget.py` against a freshly linked ELF and fails
on any of these:

| Policy | Value | What it is |
| --- | ---: | --- |
| SRAM0–3 `.data + .bss` | at most 203,776 bytes | a regression tripwire just above the largest build; crossing it means justify the growth and raise the line deliberately, not that the chip is full |
| Core-memory span at boot | at least 4,096 bytes | the safety floor for the newlib arena, about two orders of magnitude above its observed use; raise it only with evidence of a new allocator |
| Named macro storage | at most 8,192 bytes | the playback symbols `profile_macro_slots` and `macro_slot_active_ir` (both required) plus the provider's bookkeeping symbols |
| Reclaimed macro storage | at least 32,768 bytes below a 41,440-byte baseline | the VIA-era bank's footprint; the 8,192-byte line above always fails first |

Only the sum of `.data` and `.bss` is gated. The split between them is an
initialisation artifact: one nonzero member moves a whole object to `.data`,
where it also costs flash. `.bss` alone is gated only when
`--max-static-bss` is passed, and nothing passes it; the report prints both
sections. The script states how the 203,776 bytes are composed: a 56 KiB base
plus each reviewed increment and decrement since, each named by its decision.
A change that moves the tripwire edits that sum and its comment and says why
in its commit.

The gate refuses an ELF older than any firmware source file. It prints, beside
the policies, the fixed linked prefix and occupancy of every bank, SRAM4's and
SRAM5's unassigned tails, the "true static RAM headroom" (the core-memory span
less the arena floor), and GNU `size`'s BSS column, which includes NOLOAD
reservations and is informational only.

Held by: `tests/host/run_firmware_memory_budget_tool_tests.sh`
(`firmware_memory_budget_tool_test.py`,
`test_new_policy_flags_fail_at_exact_data_and_free_span_thresholds`,
`test_section_split_is_not_gated_unless_requested`,
`test_rejects_missing_required_symbol`,
`test_pass_report_distinguishes_hardware_accounting_from_policy_spans`) for
the checker; the thresholds against the real ELF only in a resource build
([Running the gates](#running-the-gates)).

### Module state policies

Each long-lived module's state type has a size policy for the 32-bit target,
a `_Static_assert` in its source that fails the firmware compile. These are
engineering regression policies, not SRAM limits:

| State | Policy (bytes) | Constant |
| --- | ---: | --- |
| Live-profile owner | 16,384 | `NOAH_PROFILE_OWNER_STATE_BUDGET_32BIT` |
| Split reconciler | 1,024 | `NOAH_PROFILE_SPLIT_RECONCILER_STATE_BUDGET_32BIT` |
| Active-profile provider | 784 | `NOAH_ACTIVE_PROFILE_PROVIDER_STATE_BUDGET_32BIT` |
| Active-profile key-behaviour runtime | 768 | `NOAH_ACTIVE_PROFILE_KEY_BEHAVIOR_RUNTIME_STATE_BUDGET_32BIT` |
| Whole-profile validator | 400 | `NOAH_PROFILE_VALIDATOR_V1_EMBEDDED_STATE_BUDGET` |
| Profile store | 384 | `NOAH_PROFILE_STORE_STATE_BUDGET_32BIT` |
| Other-half store | 256 | `NOAH_PROFILE_OTHER_HALF_STORE_STATE_BUDGET_32BIT` |
| Active-profile lighting runtime / frame | 192 / 80 | `NOAH_ACTIVE_PROFILE_RGB_RUNTIME_STATE_BUDGET_32BIT`, `NOAH_ACTIVE_PROFILE_RGB_FRAME_BUDGET_32BIT` |
| Split authority | 128 | `NOAH_PROFILE_SPLIT_AUTHORITY_STATE_BUDGET_32BIT` |
| Key-behaviour domain handle / its validation | 128 / 128 | `key_behavior_domain_v1.c`, `NOAH_KEY_BEHAVIOR_DOMAIN_V1_EMBEDDED_VALIDATION_BUDGET` |
| Lighting domain view / its validation | 40 / 72 | `NOAH_PROFILE_RGB_V1_EMBEDDED_VIEW_BUDGET`, `NOAH_PROFILE_RGB_V1_EMBEDDED_VALIDATION_BUDGET` |
| Lighting render snapshot | 96 | `rgb_runtime.c` |
| Compiled-profile handle | 76 | `profile_compiled_defaults_v1.c` |
| Durable-I/O grant | 28 | `NOAH_QMK_DURABLE_IO_STATE_BUDGET_32BIT` |

Held by: the firmware compile (`sh tools/build-firmware-pair.sh`). The host
suite compiles these sources for a 64-bit host, where the asserts guarded by
`UINTPTR_MAX == UINT32_MAX` are skipped.

## Stack policies

Two stack contexts are checked, each against a reviewed-path budget well
inside its physical stack:

| Context | Root | Physical stack | Reserve | Reviewed-path budget |
| --- | --- | ---: | --- | ---: |
| Main process | `main` | the process stack, `__process_stack_size__`, at least 2,560 bytes | 25%, at least 512 bytes | 1,920 bytes |
| Split slave thread | `SlaveThread` | 1,024 usable bytes in the 1,200-byte `waSlaveThread` working area in SRAM0–3 | 25%, at least 256 bytes | 768 bytes |

The split slave thread's stack is QMK's serial-protocol thread, not core 1's.
`tools/check_firmware_stack_budget.py` reads the linked disassembly for each
function's frame and its direct calls, and walks the paths a manifest names:
`tools/firmware_stack_budget.json` (key runtime, pointing, macros, storage
reads) and `tools/firmware_stack_budget_live_profile_owner.json` (the
live-profile owner, Profile Wire, the split protocol, saving). Every adjacency
on a path that is not a direct call must be a declared indirect edge; a path
whose adjacency the linker no longer makes fails the gate.

A PASS covers the named paths only. It is not a global or interrupt-stack
maximum and not a measured high-water: the sums start at `main` and leave out
the frame an interrupt pushes onto the process stack; paths stop at driver
functions (the checker does not follow the linker veneer into pico-sdk's
RAM-resident `flash_range_erase` and `flash_range_program`); and calls through
a function pointer that no manifest declares, such as the host driver's report
send, are not reviewed. Keep reviewed-path margin (to 1,920 or 768 bytes)
separate from the margin to the physical stack.

The paths are measured on the call shape the flashed firmware has, so code
keeps that shape. Anything `main` reaches only at boot or from housekeeping
with more than a few words of locals is `noinline`, so its frame never sits
beneath a key path; so are functions LTO would otherwise fold into a frame
every storage or Raw HID path carries. The key runtime's effect plans live on
the process stack and are bounded by `_Static_assert`s: an effect plan at most
196 bytes (`key_runtime_core_effect_plan_t`, its release plan likewise), an
effect at most 12 bytes, a deferred-release drain batch at most 64 bytes.

Held by: `tests/host/run_firmware_stack_budget_tool_tests.sh`
(`firmware_stack_budget_tool_test.py`: the checker, both repository manifests
loading as schema 2, and
`test_ordinary_manifest_reviews_every_process_stage_the_table_dispatches`);
the budgets against the real ELF only in a resource build.
`tests/host/run_firmware_stack_budget_checks.sh` and
`tests/host/run_live_profile_owner_stack_budget_checks.sh` check a plain
`qmk compile` image, which leaves out the live-profile owner (D-F35,
[`0035`](../adr/0035-live-profile-owner-on-by-default.md)) and is not the
firmware anyone flashes.

## Flash

The firmware image must end below 1 MiB: the link fails if it reaches
`0x10100000`, where the profile slot region starts
(`users/noah/lib/compat/qmk_profile_flash.ld`). The two 128 KiB slots, QMK's
8 KiB wear-leveling backing at the top of the assumed 2 MiB, the slot record,
the flash pauses and every profile ceiling (D-F14,
[`0014`](../adr/0014-bigger-profiles.md)) are in
[the device-resident profile](device-resident-profile.md#where-the-profile-is-stored).

Held by: `tests/host/run_profile_storage_layout_tests.sh` (the linker guard
check and the layout constants).

## Running the gates

```sh
NOAH_RESOURCE_CHECKS=yes sh tools/build-firmware-pair.sh
```

From a task worktree set `QMK_ROOT` and `BUILD_ROOT`. This builds the
instrumented pair, `*_resources.uf2`, in the pinned build image with the
live-profile owner: `NOAH_STACK_BUDGET_ENABLE=yes` adds a linker map, turns
off shrink wrapping so each frame is allocated in its prologue, and keeps the
Raw HID dispatcher (`noah_profile_channel_dispatch`) out of line, so its
figures can differ slightly from the ordinary pair's. For each half it keeps the ELF, map, compiler and
linker flags beside the pair and writes three reports: `<name>.memory.txt`,
`<name>.firmware_stack_budget.txt` and
`<name>.firmware_stack_budget_live_profile_owner.txt`. Every gate runs on both
halves whatever an earlier one found; the build then fails, naming each
failed report, if any did. Flash the ordinary pair, not this one.

The resource build is not part of the host suite, the ordinary pair build or
CI; run it for any change to static RAM, the stack configuration or call
shapes on a reviewed path, and when the compiler changes. The host suite runs
only the two tools' own tests.

## Reporting resources

Every resource figure names:

- the half and the physical bank or linker span it measures;
- whether it is a link-time value, a reviewed-path estimate or a measured
  runtime high-water;
- the policy separately from the physical capacity;
- the exact build and command that produced it.

Describe the `.data + .bss` margin as policy margin, the core-memory span as
the allocator's range at boot, and a stack PASS as covering the named paths.
GNU `size`'s aggregate BSS includes NOLOAD reservations: never add it to the
`.data`, `.bss`, core-memory-span or stack figures. A claim that a RAM
representation is required or impossible names the bank, fresh linked
accounting, the applicable policy and runtime high-water evidence; a
conservative design may stand on its own merits, never on a policy value
presented as capacity. Measurements belong in the commit message of the change
they measure; this file changes only when a policy, a bank or a gate does.
