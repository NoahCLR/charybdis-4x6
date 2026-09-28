"""Exercise the QMK flag and paired artifact contract without flashing hardware."""

import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class SplitTransportBuildTest(unittest.TestCase):
    def test_make_selection(self):
        with tempfile.TemporaryDirectory() as directory:
            probe = Path(directory) / "probe.mk"
            probe.write_text("all:\n\t@echo $(OPT_DEFS)\n")
            for baud in ("", "230400", "460800", "115200", "230400 460800", "bad"):
                with self.subTest(baud=baud):
                    result = subprocess.run(
                        ["make", "--no-print-directory", "-f",
                         str(ROOT / "users/noah/lib/compat/qmk_split_transport.mk"),
                         "-f", str(probe), f"NOAH_SPLIT_BAUD={baud}"],
                        capture_output=True, text=True,
                    )
                    if baud in ("", "230400", "460800"):
                        self.assertEqual(result.returncode, 0, result.stderr)
                        expected = f"-DSERIAL_USART_SPEED={baud}" if baud else ""
                        self.assertEqual(result.stdout.strip(), expected)
                    else:
                        self.assertNotEqual(result.returncode, 0)

    def test_feature_selection(self):
        with tempfile.TemporaryDirectory() as directory:
            probe = Path(directory) / "probe.mk"
            probe.write_text("all:\n\t@echo $(OPT_DEFS)\n")
            for name, flag in (("NOAH_SPLIT_ACTIVITY_COALESCE", "NOAH_SPLIT_ACTIVITY_COALESCE_ENABLE"),
                               ("NOAH_SPLIT_DIAGNOSTICS", "SPLIT_TRANSACTION_DIAGNOSTICS")):
                for value in ("", "no", "yes", "invalid"):
                    result = subprocess.run(
                        ["make", "--no-print-directory", "-f",
                         str(ROOT / "users/noah/lib/compat/qmk_split_transport.mk"),
                         "-f", str(probe), f"{name}={value}"],
                        capture_output=True, text=True,
                    )
                    if value == "invalid":
                        self.assertNotEqual(result.returncode, 0)
                    else:
                        self.assertEqual(result.returncode, 0, result.stderr)
                        self.assertEqual(result.stdout.strip(), f"-D{flag}" if value == "yes" else "")

    def test_pair_flags_and_artifact_names(self):
        for baud, owner, activity, diagnostic in (("", True, "", ""), ("230400", True, "yes", ""), ("230400", True, "", "yes"), ("230400", True, "yes", "yes"), ("460800", False, "yes", "yes")):
            with self.subTest(baud=baud, owner=owner), tempfile.TemporaryDirectory() as directory:
                workspace = Path(directory)
                qmk = workspace / "qmk checkout"
                qmk.mkdir()
                bin_dir = workspace / "bin"
                bin_dir.mkdir()
                mock = bin_dir / "qmk"
                mock.write_text(
                    "#!/usr/bin/env python3\n"
                    "import json, os, sys\n"
                    "from pathlib import Path\n"
                    "with open(os.environ['PAIR_TEST_LOG'], 'a') as log:\n"
                    "    log.write(json.dumps(sys.argv[1:]) + '\\n')\n"
                    "Path('bastardkb_charybdis_4x6_noah.uf2').write_text('test artifact')\n"
                )
                mock.chmod(0o755)
                log = workspace / "calls.jsonl"
                env = dict(os.environ, QMK_ROOT=str(qmk), BUILD_ROOT=str(workspace / "output"),
                           NOAH_SPLIT_BAUD=baud, NOAH_SPLIT_ACTIVITY_COALESCE=activity, NOAH_SPLIT_DIAGNOSTICS=diagnostic, PAIR_TEST_LOG=str(log),
                           PATH=f"{bin_dir}{os.pathsep}{os.environ['PATH']}")
                command = ["sh", str(ROOT / "tools/build-firmware-pair.sh")]
                if not owner:
                    command.append("--no-owner")
                result = subprocess.run(command, env=env, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                calls = [json.loads(line) for line in log.read_text().splitlines()]
                self.assertEqual(len(calls), 2)
                for call, half, role in zip(calls, ("right", "left"), ("MASTER", "SLAVE")):
                    self.assertIn(f"NOAH_PHYSICAL_HALF={half}", call)
                    self.assertIn(f"FORCE_{role}=yes", call)
                    self.assertEqual(f"NOAH_SPLIT_BAUD={baud}" in call, bool(baud))
                    self.assertEqual("NOAH_LIVE_PROFILE_OWNER=no" in call, not owner)
                    self.assertEqual("NOAH_SPLIT_ACTIVITY_COALESCE=yes" in call, activity == "yes")
                    self.assertEqual("NOAH_SPLIT_DIAGNOSTICS=yes" in call, diagnostic == "yes")
                suffix = ("" if owner else "_no_owner") + (f"_baud{baud}" if baud else "")
                suffix += ("_activity" if activity else "") + ("_diagnostic" if diagnostic else "")
                artifacts = sorted(path.name for path in (workspace / "output").rglob("*.uf2"))
                self.assertEqual(artifacts, [f"1_charybdis_{half}{suffix}.uf2" for half in ("left", "right")])
                self.assertEqual("WITHOUT the live-profile owner" in result.stdout, not owner)


if __name__ == "__main__":
    unittest.main()
