"""Fork-only and internal QMK symbols are used only in users/noah/lib/compat/.

Compat adapts each QMK or fork assumption once, with a contract check in
run_qmk_contract_checks.sh. A runtime module or keymap file that names one of
the symbols below has reached past that adapter. The few places that must name
one, such as a hook QMK calls by name, are allowlisted with their reason.
Comments are not uses. Stock QMK public API (register_code, layer_on, …) and
struct fields (combo_t's active, disabled and state bits) are not listed.

To add a symbol compat adapts, add it to SYMBOLS with what it is.
"""

import re
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
COMPAT = "users/noah/lib/compat/"
SCANNED = ("users/noah", "keyboards/bastardkb/charybdis/4x6/keymaps")

SYMBOLS = {
    # Fork hooks and queries (BK's docs/noah-fork-contracts.md).
    "process_record_admit_user": "fork hook: record admission",
    "is_tap_keycode_user": "fork hook: which keys QMK's tapping engine decides",
    "is_tap_record_user": "fork hook: which records QMK's tapping engine decides",
    "tapping_key_event_pending": "fork query: QMK's tapping queue",
    "combo_key_event_pending": "fork query: QMK's combo queue",
    "combo_key_record_allowed": "fork hook: combo member record filter",
    "COMBO_KEY_RECORD_FILTER": "fork switch: combo member record filter",
    "KEYRECORD_USER_DATA": "fork switch: the opaque byte in keyrecord_t",
    "keyboard_report_mods_override_user": "fork hook: report-only modifiers",
    "keyboard_report_keys_filter_user": "fork hook: report-only key filter",
    "nkro_report_keys_filter_user": "fork hook: report-only NKRO key filter",
    "QMK_REPORT_MODS_OVERRIDE": "fork feature test: report-only modifiers",
    "QMK_REPORT_KEYS_FILTER": "fork feature test: report-only key filter",
    "usb_product_string_user": "fork hook: runtime USB product string",
    "USB_PRODUCT_STRING_USER": "fork switch: runtime USB product string",
    "auto_mouse_get_time_elapsed_at": "fork query: auto-mouse elapsed time at a sampled clock",
    "split_activity_sync_should_send": "fork hook: split activity admission",
    "split_activity_sync_sent": "fork hook: split activity admission",
    "split_transaction_diagnostic": "fork hook: split transaction diagnostics",
    "QMK_SPLIT_ACTIVITY_POLICY_VERSION": "fork feature test: split activity admission",
    "QMK_SPLIT_TRANSPORT_CRC_VERSION": "fork feature test: split frame CRC",
    "charybdis_get_pointer_default_dpi": "BK keyboard pointer API",
    "charybdis_get_pointer_sniping_dpi": "BK keyboard pointer API",
    "charybdis_get_pointer_sniping_enabled": "BK keyboard pointer API",
    "charybdis_set_pointer_sniping_enabled": "BK keyboard pointer API",
    "charybdis_cycle_pointer_default_dpi_noeeprom": "BK keyboard pointer API",
    "charybdis_cycle_pointer_sniping_dpi_noeeprom": "BK keyboard pointer API",
    # QMK internals.
    "read_source_layers_cache": "QMK internal: the source-layer cache",
    "update_source_layers_cache": "QMK internal: the source-layer cache",
    "layer_switch_get_layer": "QMK internal: source-layer resolution",
    "keymap_config": "QMK internal: the keymap options in eeconfig",
    "combo_task": "QMK internal: the combo engine's scan",
    "protocol_keyboard_task": "QMK internal: the main loop's keyboard task",
    "pointing_device_driver": "QMK internal: the pointing driver table",
}

# (file, symbol): why that file names it outside compat.
ALLOWED = {
    ("users/noah/hooks.c", "process_record_admit_user"):
        "the weak default QMK calls by name; it hands the record to compat's admission",
    ("users/noah/lib/key/runtime/process.c", "is_tap_keycode_user"):
        "QMK's tapping engine calls it by name; the key runtime's answer is the decision (D-F27)",
    ("users/noah/lib/key/runtime/process.c", "is_tap_record_user"):
        "QMK's tapping engine calls it by name; the key runtime's answer is the decision (D-F27)",
    ("users/noah/lib/macro/macro_payload_run.c", "keyboard_report_mods_override_user"):
        "QMK calls it by name; macro playback owns what the report carries",
    ("users/noah/lib/macro/macro_payload_run.c", "keyboard_report_keys_filter_user"):
        "QMK calls it by name; macro playback owns what the report carries",
    ("users/noah/lib/macro/macro_payload_run.c", "nkro_report_keys_filter_user"):
        "QMK calls it by name; macro playback owns what the report carries",
    ("users/noah/lib/macro/macro_payload_run.c", "QMK_REPORT_MODS_OVERRIDE"):
        "refuses a build whose QMK lacks the hook defined beside it",
    ("users/noah/lib/macro/macro_payload_run.c", "QMK_REPORT_KEYS_FILTER"):
        "refuses a build whose QMK lacks the hooks defined beside it",
    ("users/noah/config.h", "KEYRECORD_USER_DATA"):
        "the userspace configuration switches the fork's record byte on",
    ("users/noah/lib/key/behavior/participation.c", "KEYRECORD_USER_DATA"):
        "the fork leaves the byte's meaning downstream; participation owns it",
}

COMMENT = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)
SYMBOL = re.compile(r"\b(" + "|".join(map(re.escape, SYMBOLS)) + r")\b")


def uses(source: str) -> list[tuple[int, str]]:
    # Comments become blanks of the same lines, so line numbers hold.
    code = COMMENT.sub(lambda match: "\n" * match.group().count("\n"), source)
    return [(code.count("\n", 0, match.start()) + 1, match.group(1)) for match in SYMBOL.finditer(code)]


def problems(root: Path, allowed: dict = ALLOWED) -> list[str]:
    tracked = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z", "--", *SCANNED],
        capture_output=True, check=True,
    ).stdout.decode().split("\0")
    found = []
    seen = set()
    for path in sorted(p for p in tracked if p.endswith((".c", ".h")) and not p.startswith(COMPAT)):
        for line, symbol in uses((root / path).read_text(errors="replace")):
            if (path, symbol) in allowed:
                seen.add((path, symbol))
            else:
                found.append(f"{path}:{line}: {symbol} ({SYMBOLS[symbol]}) belongs in {COMPAT}")
    for path, symbol in sorted(set(allowed) - seen):
        found.append(f"{path} is allowlisted for {symbol} but no longer uses it")
    return found


class CompatBoundaryTest(unittest.TestCase):
    def test_repository(self):
        self.assertEqual(problems(ROOT), [])

    def fixture(self, directory: str, files: dict[str, str]) -> Path:
        root = Path(directory)
        for path, text in files.items():
            (root / path).parent.mkdir(parents=True, exist_ok=True)
            (root / path).write_text(text)
        subprocess.run(["git", "init", "-q", str(root)], check=True)
        subprocess.run(["git", "-C", str(root), "add", "."], check=True)
        return root

    def test_use_outside_compat_is_named(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.fixture(directory, {
                "users/noah/lib/key/runtime/scan.c": "#include \"x.h\"\n\nbool f(void) {\n    return combo_key_event_pending(0, 0, true, 0, 0);\n}\n",
                "keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c": "uint8_t layer(keypos_t key) { return read_source_layers_cache(key); }\n",
            })
            self.assertEqual(problems(root, {}), [
                "keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c:1: read_source_layers_cache "
                "(QMK internal: the source-layer cache) belongs in users/noah/lib/compat/",
                "users/noah/lib/key/runtime/scan.c:4: combo_key_event_pending "
                "(fork query: QMK's combo queue) belongs in users/noah/lib/compat/",
            ])

    def test_compat_comments_and_allowlist_pass(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.fixture(directory, {
                "users/noah/lib/compat/qmk_gesture_timing.h": "bool combo_key_event_pending(uint8_t row);\n",
                "users/noah/lib/key/runtime/scan.c": "// asks combo_key_event_pending\n/* and\n tapping_key_event_pending */\nint x;\n",
                "users/noah/hooks.c": "bool process_record_admit_user(keyrecord_t *record) { return true; }\n",
            })
            self.assertEqual(problems(root, {("users/noah/hooks.c", "process_record_admit_user"): "test"}), [])
            self.assertEqual(problems(root, {}), [
                "users/noah/hooks.c:1: process_record_admit_user (fork hook: record admission) belongs in users/noah/lib/compat/",
            ])

    def test_stale_allowlist_entry_is_named(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.fixture(directory, {"users/noah/hooks.c": "int x;\n"})
            self.assertEqual(problems(root, {("users/noah/hooks.c", "process_record_admit_user"): "test"}), [
                "users/noah/hooks.c is allowlisted for process_record_admit_user but no longer uses it",
            ])

    def test_line_numbers_survive_comments(self):
        self.assertEqual(uses("/* one\ntwo */\n// three\ncombo_task();\n"), [(4, "combo_task")])


if __name__ == "__main__":
    unittest.main()
