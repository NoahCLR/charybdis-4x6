"""Build-driver regression tests: no compiler or physical keyboard required."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class PairBuild(unittest.TestCase):
    def test_explicit_source_selection_survives_saved_qmk_configuration(self):
        with tempfile.TemporaryDirectory(prefix='pair-build-') as tmp:
            base = Path(tmp)
            source, qmk, fakebin = (base / name for name in ('task-userspace', 'selected-qmk', 'bin'))
            for folder in (source / 'tools', qmk, fakebin):
                folder.mkdir(parents=True)
            shutil.copyfile(Path(__file__).resolve().parents[2] / 'tools/build-firmware-pair.sh', source / 'tools/build-firmware-pair.sh')
            subprocess.run(['git', 'init', '-q', '-b', 'test-pair', source], check=True)
            subprocess.run(['git', '-C', source, '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
                            'commit', '--allow-empty', '-qm', 'fixture'], check=True)
            driver = fakebin / 'qmk'
            driver.write_text('''#!/usr/bin/env python3
import json, os, sys
from pathlib import Path
# Simulate the installed CLI: saved config would win without this isolation.
assert sys.argv[1:4] == ['--config-file', '/dev/null', 'compile'], sys.argv
assert 'QMK_BIN=qmk --config-file /dev/null' in sys.argv, 'make generators must also ignore saved config'
keymap = Path(os.environ['QMK_USERSPACE']) / 'keyboards/bastardkb/charybdis/4x6/keymaps/noah'
for number in range(1, 6):
    assert f'MAIN_KEYMAP_PATH_{number}={keymap}' in sys.argv, 'a QMK keymap symlink must not select other source'
assert Path.cwd().resolve() == Path(os.environ['QMK_HOME']).resolve()
assert Path(os.environ['QMK_USERSPACE']).resolve() == Path(os.environ['EXPECTED_USERSPACE']).resolve()
with open(os.environ['PAIR_TEST_LOG'], 'a') as log:
    log.write(json.dumps(sys.argv) + '\\n')
half = next(arg.split('=', 1)[1] for arg in sys.argv if arg.startswith('NOAH_PHYSICAL_HALF='))
Path('bastardkb_charybdis_4x6_noah.uf2').write_bytes(half.encode())
''')
            driver.chmod(0o755)
            env = dict(os.environ, PATH=str(fakebin) + os.pathsep + os.environ['PATH'], QMK_ROOT=str(qmk),
                       QMK_HOME='/wrong/qmk', QMK_USERSPACE='/wrong/userspace', EXPECTED_USERSPACE=str(source),
                       BUILD_ROOT=str(base / 'builds'), PAIR_TEST_LOG=str(base / 'calls'))
            subprocess.run(['sh', source / 'tools/build-firmware-pair.sh'], cwd=base, env=env, check=True)
            calls = [json.loads(line) for line in (base / 'calls').read_text().splitlines()]
            self.assertEqual(len(calls), 2)
            for half in ('right', 'left'):
                artifact = base / 'builds/test-pair' / f'1_charybdis_{half}.uf2'
                self.assertEqual(artifact.read_bytes(), half.encode())


if __name__ == '__main__':
    unittest.main()
