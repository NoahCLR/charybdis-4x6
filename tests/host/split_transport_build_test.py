"""Exercise the QMK flag and paired artifact contract without flashing hardware."""

import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class SplitTransportBuildTest(unittest.TestCase):
    def test_split_speed_is_not_selectable(self):
        # The link runs at QMK's default speed; 460,800 baud garbled it (D-L43).
        with tempfile.TemporaryDirectory() as directory:
            probe = Path(directory) / "probe.mk"
            probe.write_text("all:\n\t@echo $(OPT_DEFS)\n")
            for baud in ("", "230400", "460800"):
                with self.subTest(baud=baud):
                    result = subprocess.run(
                        ["make", "--no-print-directory", "-f",
                         str(ROOT / "users/noah/lib/compat/qmk_split_transport.mk"),
                         "-f", str(probe), f"NOAH_SPLIT_BAUD={baud}"],
                        capture_output=True, text=True,
                    )
                    if baud:
                        self.assertNotEqual(result.returncode, 0)
                        self.assertIn("NOAH_SPLIT_BAUD was removed", result.stderr)
                    else:
                        self.assertEqual(result.returncode, 0, result.stderr)
                        self.assertNotIn("SERIAL_USART_SPEED", result.stdout)

    def test_feature_selection(self):
        # Coalescing is on unless turned off; diagnostics are off unless on.
        with tempfile.TemporaryDirectory() as directory:
            probe = Path(directory) / "probe.mk"
            probe.write_text("all:\n\t@echo $(OPT_DEFS)\n")
            for name, flag, default in (("NOAH_SPLIT_ACTIVITY_COALESCE", "NOAH_SPLIT_ACTIVITY_COALESCE_ENABLE", True),
                                        ("NOAH_SPLIT_DIAGNOSTICS", "SPLIT_TRANSACTION_DIAGNOSTICS", False)):
                for value in (None, "", "no", "yes", "invalid"):
                    with self.subTest(name=name, value=value):
                        command = ["make", "--no-print-directory", "-f",
                                   str(ROOT / "users/noah/lib/compat/qmk_split_transport.mk"), "-f", str(probe)]
                        if value is not None:
                            command.append(f"{name}={value}")
                        env = {key: val for key, val in os.environ.items() if key != name}
                        result = subprocess.run(command, capture_output=True, text=True, env=env)
                        if value == "invalid":
                            self.assertNotEqual(result.returncode, 0)
                            continue
                        self.assertEqual(result.returncode, 0, result.stderr)
                        enabled = value == "yes" or (default and value in (None, ""))
                        self.assertEqual(f"-D{flag}" in result.stdout.split(), enabled)

    def test_pair_flags_and_artifact_names(self):
        for baud, owner, activity, diagnostic in (("", True, "", ""), ("", True, "yes", ""), ("", True, "no", ""), ("", True, "", "yes"), ("", False, "no", "yes"), ("460800", True, "yes", "")):
            with self.subTest(baud=baud, owner=owner, activity=activity, diagnostic=diagnostic), tempfile.TemporaryDirectory() as directory:
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
                if baud:
                    # A leftover speed setting fails before anything is built.
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("NOAH_SPLIT_BAUD was removed", result.stderr)
                    self.assertFalse(log.exists())
                    continue
                self.assertEqual(result.returncode, 0, result.stderr)
                calls = [json.loads(line) for line in log.read_text().splitlines()]
                self.assertEqual(len(calls), 2)
                for call, half, role in zip(calls, ("right", "left"), ("MASTER", "SLAVE")):
                    self.assertIn(f"NOAH_PHYSICAL_HALF={half}", call)
                    self.assertIn(f"FORCE_{role}=yes", call)
                    self.assertFalse(any("NOAH_SPLIT_BAUD" in argument for argument in call))
                    self.assertEqual("NOAH_LIVE_PROFILE_OWNER=no" in call, not owner)
                    # The default build coalesces; only the opt-out is passed on.
                    self.assertFalse("NOAH_SPLIT_ACTIVITY_COALESCE=yes" in call)
                    self.assertEqual("NOAH_SPLIT_ACTIVITY_COALESCE=no" in call, activity == "no")
                    self.assertEqual("NOAH_SPLIT_DIAGNOSTICS=yes" in call, diagnostic == "yes")
                suffix = "" if owner else "_no_owner"
                suffix += ("_no_activity" if activity == "no" else "") + ("_diagnostic" if diagnostic else "")
                artifacts = sorted(path.name for path in (workspace / "output").rglob("*.uf2"))
                self.assertEqual(artifacts, [f"1_charybdis_{half}{suffix}.uf2" for half in ("left", "right")])
                self.assertEqual("WITHOUT the live-profile owner" in result.stdout, not owner)


if __name__ == "__main__":
    unittest.main()
