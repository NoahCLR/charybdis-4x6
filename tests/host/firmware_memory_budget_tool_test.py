#!/usr/bin/env python3

import importlib.util
import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("memory_budget", ROOT / "tools" / "check_firmware_memory_budget.py")
MEMORY_BUDGET = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MEMORY_BUDGET)


class FirmwareMemoryBudgetToolTest(unittest.TestCase):
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

    def test_parses_static_bss_and_heap_boundaries(self):
        output = "\n".join(
            (
                "20005dc8 B __bss_base__",
                "2000c17c B __bss_end__",
                "2000c180 B __heap_base__",
                "20040000 B __heap_end__",
            )
        )
        symbols = MEMORY_BUDGET.parse_layout_symbols(output)
        self.assertEqual(symbols["__bss_end__"] - symbols["__bss_base__"], 25524)
        self.assertEqual(symbols["__heap_end__"] - symbols["__heap_base__"], 212608)

    def test_rejects_missing_layout_boundary(self):
        with self.assertRaisesRegex(ValueError, "__heap_end__"):
            MEMORY_BUDGET.parse_layout_symbols("20005dc8 B __bss_base__")

    def test_rejects_unparseable_size_output(self):
        with self.assertRaisesRegex(ValueError, "GNU size"):
            MEMORY_BUDGET.parse_size_bss("not a size report")


if __name__ == "__main__":
    unittest.main()
