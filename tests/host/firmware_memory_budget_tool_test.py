#!/usr/bin/env python3

import importlib.util
import io
import pathlib
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from unittest import mock


ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("memory_budget", ROOT / "tools" / "check_firmware_memory_budget.py")
MEMORY_BUDGET = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MEMORY_BUDGET)


LAYOUT_OUTPUT = "\n".join(
    (
        "20000000 T __data_base__",
        "20005eec T __data_end__",
        "20005ef0 B __bss_base__",
        "2000c2a4 B __bss_end__",
        "2000c2a8 B __heap_base__",
        "20040000 B __heap_end__",
        "20000000 B __ram0_base__",
        "20040000 A __ram0_end__",
        "20040000 B __ram4_base__",
        "20040f20 B __ram4_free__",
        "20041000 A __ram4_end__",
        "20041000 B __ram5_base__",
        "20041e00 B __ram5_free__",
        "20042000 A __ram5_end__",
        "20041f00 B __ram7_base__",
        "20041f00 B __ram7_free__",
        "20042000 A __ram7_end__",
    )
)

MACRO_OUTPUT = "\n".join(
    (
        "20000000 00000010 b hardcoded_macro_slots",
        "20000010 00000040 B via_macro_slots.lto_priv.7",
        "20000050 00000202 b macro_slot_active_ir.lto_priv.8",
        "20000254 00000004 b macro_slot_active_metadata.lto_priv.9",
    )
)

SIZE_OUTPUT = "text data bss dec hex filename\n151000 0 220000 371000 5a938 firmware.elf\n"


class FirmwareMemoryBudgetToolTest(unittest.TestCase):
    def run_main(self, *extra_args, layout_output=LAYOUT_OUTPUT):
        with tempfile.TemporaryDirectory() as temp_dir:
            elf = pathlib.Path(temp_dir) / "firmware.elf"
            elf.touch()

            def fake_run(command):
                if "--size-sort" in command:
                    return MACRO_OUTPUT
                if "--version" in command:
                    return "GNU nm test-toolchain\n"
                if command[0] == "arm-none-eabi-size":
                    return SIZE_OUTPUT
                return layout_output

            stdout = io.StringIO()
            stderr = io.StringIO()
            with mock.patch.object(MEMORY_BUDGET, "run", side_effect=fake_run):
                with redirect_stdout(stdout), redirect_stderr(stderr):
                    result = MEMORY_BUDGET.main(["--elf", str(elf), *extra_args])
            return result, stdout.getvalue(), stderr.getvalue()

    def test_parses_lto_and_plain_data_symbols(self):
        symbols = MEMORY_BUDGET.parse_nm_symbols(
            "\n".join(
                (
                    "20000000 00000010 b hardcoded_macro_slots",
                    "20000010 00000040 B via_macro_slots.lto_priv.7",
                    "20000050 00000202 b macro_slot_active_ir.lto_priv.8",
                    "20000254 00000004 b macro_slot_active_metadata.lto_priv.9",
                    "00000100 00000020 T macro_slot_active_ir_helper",
                )
            )
        )
        self.assertEqual(symbols["hardcoded_macro_slots"], 16)
        self.assertEqual(symbols["via_macro_slots"], 64)
        self.assertEqual(symbols["macro_slot_active_ir"], 514)
        self.assertEqual(symbols["macro_slot_active_metadata"], 4)

    def test_rejects_missing_required_symbol(self):
        with self.assertRaisesRegex(ValueError, "via_macro_slots"):
            MEMORY_BUDGET.parse_nm_symbols(
                "20000000 00000010 b hardcoded_macro_slots\n"
                "20000010 00000202 b macro_slot_active_ir\n"
            )

    def test_rejects_duplicate_canonical_symbol(self):
        with self.assertRaisesRegex(ValueError, "duplicate"):
            MEMORY_BUDGET.parse_nm_symbols(
                "20000000 00000010 b hardcoded_macro_slots\n"
                "20000010 00000010 b hardcoded_macro_slots.lto_priv.1\n"
                "20000020 00000040 b via_macro_slots\n"
                "20000060 00000202 b macro_slot_active_ir\n"
            )

    def test_parses_gnu_size_bss(self):
        output = "text data bss dec hex filename\n151000 0 204744 355744 56da0 firmware.elf\n"
        self.assertEqual(MEMORY_BUDGET.parse_size_bss(output), 204744)

    def test_parses_static_bss_and_linker_managed_free_boundaries(self):
        symbols = MEMORY_BUDGET.parse_layout_symbols(LAYOUT_OUTPUT)
        self.assertEqual(symbols["__bss_end__"] - symbols["__bss_base__"], 25524)
        self.assertEqual(symbols["__heap_end__"] - symbols["__heap_base__"], 212312)

    def test_parses_static_data_span(self):
        # noah_runtime_singleton carries non-zero initializers and therefore
        # lands in .data, so a gate that measured only .bss missed the single
        # largest static object.
        symbols = MEMORY_BUDGET.parse_layout_symbols(LAYOUT_OUTPUT)
        self.assertEqual(symbols["__data_end__"] - symbols["__data_base__"], 24300)

    def test_rejects_missing_data_boundary(self):
        partial = "\n".join(
            line for line in LAYOUT_OUTPUT.splitlines() if "__data_end__" not in line
        )
        with self.assertRaisesRegex(ValueError, "__data_end__"):
            MEMORY_BUDGET.parse_layout_symbols(partial)

    def test_rejects_missing_layout_boundary(self):
        with self.assertRaisesRegex(ValueError, "__heap_end__"):
            MEMORY_BUDGET.parse_layout_symbols("20005dc8 B __bss_base__")

    def test_rejects_unparseable_size_output(self):
        with self.assertRaisesRegex(ValueError, "GNU size"):
            MEMORY_BUDGET.parse_size_bss("not a size report")

    def test_accounts_for_unique_rp2040_banks_without_double_counting_ram7(self):
        memory = MEMORY_BUDGET.calculate_memory_accounting(
            MEMORY_BUDGET.parse_layout_symbols(LAYOUT_OUTPUT)
        )
        self.assertEqual(memory["physical_sram"], 270336)
        self.assertEqual(memory["data_bss"], 49824)
        self.assertEqual(memory["ram0_fixed_prefix"], 49832)
        self.assertEqual(memory["ram0_free"], 212312)
        self.assertEqual(memory["ram4_fixed_prefix"], 3872)
        self.assertEqual(memory["ram4_unassigned_tail"], 224)
        self.assertEqual(memory["ram5_fixed_prefix"], 3584)
        self.assertEqual(memory["ram5_unassigned_pre_boot"], 256)
        self.assertEqual(memory["ram7_reserved"], 256)
        self.assertEqual(memory["fixed_linked"], 57288)

    def test_pass_report_distinguishes_hardware_accounting_from_policy_spans(self):
        result, stdout, stderr = self.run_main()
        self.assertEqual(result, 0)
        self.assertEqual(stderr, "")
        self.assertIn("firmware memory policy: PASS", stdout)
        self.assertIn("RP2040 physical SRAM per MCU: 270336 B", stdout)
        self.assertIn(
            ".data + .bss static RAM: 49824 B (regression tripwire 57344 B, 7520 B below it)",
            stdout,
        )
        # True headroom must be reported separately from policy distance so
        # policy slack cannot read as a hardware limit (R-07).
        self.assertIn("true static RAM headroom: 208216 B", stdout)
        self.assertIn("fixed linked occupancy across unique SRAM banks: 57288 B", stdout)
        self.assertIn("linker-managed free/core-memory span at boot: 212312 B", stdout)
        # The section split is an initialisation artifact, so it is reported
        # without a limit unless one is passed explicitly.
        self.assertIn("SRAM0-3 .bss span: 25524 B (informational)", stdout)
        self.assertNotIn("linker heap", stdout)

    def test_new_policy_flags_fail_at_exact_data_and_free_span_thresholds(self):
        result, stdout, stderr = self.run_main(
            "--max-data-bss",
            "49823",
            "--min-sram0-free",
            "212313",
        )
        self.assertEqual(result, 1)
        self.assertIn("firmware memory policy: FAIL", stdout)
        self.assertIn(".data + .bss policy span 49824 exceeds 49823 B", stderr)
        self.assertIn(
            "SRAM0-3 newlib arena 212312 B is below the 212313 B safety floor",
            stderr,
        )

    def test_legacy_policy_flag_aliases_remain_supported(self):
        result, _, stderr = self.run_main(
            "--max-static-ram",
            "49823",
            "--min-heap",
            "212313",
        )
        self.assertEqual(result, 1)
        self.assertIn(".data + .bss policy span 49824 exceeds 49823 B", stderr)
        self.assertIn("newlib arena 212312 B is below the 212313 B safety floor", stderr)

    def test_section_split_is_not_gated_unless_requested(self):
        # 25524 B of .bss would fail the retired 26000 B default only if the
        # split were still enforced by default.
        result, _, stderr = self.run_main()
        self.assertEqual(result, 0)
        self.assertEqual(stderr, "")

        result, _, stderr = self.run_main("--max-static-bss", "25523")
        self.assertEqual(result, 1)
        self.assertIn("static BSS 25524 exceeds 25523 B", stderr)

    def test_arena_floor_defaults_well_below_the_measured_span(self):
        # The default floor guards the newlib arena rather than implicitly
        # capping static RAM, so a 212312 B span passes comfortably.
        result, stdout, _ = self.run_main()
        self.assertEqual(result, 0)
        self.assertIn("policy minimum 4096 B", stdout)

    def test_rejects_non_rp2040_physical_bank_capacity(self):
        wrong_layout = LAYOUT_OUTPUT.replace("20042000 A __ram5_end__", "20043000 A __ram5_end__")
        result, stdout, stderr = self.run_main(layout_output=wrong_layout)
        self.assertEqual(result, 1)
        self.assertEqual(stdout, "")
        self.assertIn("firmware memory policy: FAIL", stderr)
        self.assertIn("unexpected RP2040 SRAM bank layout", stderr)


if __name__ == "__main__":
    unittest.main()
