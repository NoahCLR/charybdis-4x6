"""Build-driver regression tests: no compiler or physical keyboard required."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import hashlib

REPO = Path(__file__).resolve().parents[2]


class PairBuild(unittest.TestCase):
    def test_explicit_source_selection_survives_saved_qmk_configuration(self):
        with tempfile.TemporaryDirectory(prefix='pair-build-') as tmp:
            base = Path(tmp)
            source, qmk, fakebin = (base / name for name in ('task-userspace', 'selected-qmk', 'bin'))
            for folder in (source / 'tools', qmk, fakebin):
                folder.mkdir(parents=True)
            for tool in ('build-firmware-pair.sh', 'build-image'):
                shutil.copyfile(REPO / 'tools' / tool, source / 'tools' / tool)
            # BK is a real checkout at the commit qmk-pin.json names.
            git = lambda root, *args: subprocess.run(['git', '-C', root, '-c', 'user.name=Test',
                                                      '-c', 'user.email=test@example.invalid', *args],
                                                     check=True, capture_output=True, text=True).stdout.strip()
            subprocess.run(['git', 'init', '-q', qmk], check=True)
            (qmk / '.gitignore').write_text('*.uf2\n.build/\n')
            git(qmk, 'add', '.gitignore')
            git(qmk, 'commit', '-qm', 'bk')
            pinned = git(qmk, 'rev-parse', 'HEAD')
            (source / 'qmk-pin.json').write_text(json.dumps({'repository': 'https://github.com/NoahCLR/bastardkb-qmk',
                                                            'commit': pinned}))
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
# Nothing from an earlier build may survive: a stale .d names another worktree's sources.
assert not Path('.build/obj_bastardkb_charybdis_4x6_noah').exists(), 'stale objects or dependency files survived'
Path('.build/obj_bastardkb_charybdis_4x6_noah').mkdir(parents=True)
Path('.build/obj_bastardkb_charybdis_4x6_noah/runtime_init.d').write_text('runtime_init.o: /removed/worktree/runtime_init.c')
assert Path(os.environ['QMK_USERSPACE']).resolve() == Path(os.environ['EXPECTED_USERSPACE']).resolve()
# Reproducible: no build-time version stamps, no machine-specific source paths.
assert 'SKIP_VERSION=yes' in sys.argv, 'QMK version.h would stamp the build time and git describe'
flags = next(arg for arg in sys.argv if arg.startswith('EXTRAFLAGS='))
assert f'-ffile-prefix-map={os.environ["QMK_USERSPACE"]}=/userspace' in flags, flags
assert f'-ffile-prefix-map={Path.cwd()}=/qmk' in flags or f'-ffile-prefix-map={os.environ["QMK_HOME"]}=/qmk' in flags, flags
with open(os.environ['PAIR_TEST_LOG'], 'a') as log:
    log.write(json.dumps(sys.argv) + '\\n')
half = next(arg.split('=', 1)[1] for arg in sys.argv if arg.startswith('NOAH_PHYSICAL_HALF='))
Path('bastardkb_charybdis_4x6_noah.uf2').write_bytes(half.encode())
''')
            driver.chmod(0o755)
            stale = qmk / '.build/obj_bastardkb_charybdis_4x6_noah'
            stale.mkdir(parents=True)
            (stale / 'runtime_init.d').write_text('runtime_init.o: /removed/worktree/runtime_init.c\n')
            (stale / 'runtime_init.o').write_bytes(b'stale')
            env = dict(os.environ, PATH=str(fakebin) + os.pathsep + os.environ['PATH'], QMK_ROOT=str(qmk),
                       QMK_HOME='/wrong/qmk', QMK_USERSPACE='/wrong/userspace', EXPECTED_USERSPACE=str(source),
                       BUILD_ROOT=str(base / 'builds'), PAIR_TEST_LOG=str(base / 'calls'), NOAH_IN_BUILD_IMAGE='1')
            subprocess.run(['sh', source / 'tools/build-firmware-pair.sh'], cwd=base, env=env, check=True)
            calls = [json.loads(line) for line in (base / 'calls').read_text().splitlines()]
            # A BK that is not the pin (moved, or locally changed) is refused,
            # unless explicitly allowed for a trial.
            (qmk / 'hook.c').write_text('void hook(void) {}\n')
            refused = subprocess.run(['sh', source / 'tools/build-firmware-pair.sh'], cwd=base, env=env,
                                     capture_output=True, text=True)
            self.assertNotEqual(refused.returncode, 0)
            self.assertIn('qmk-pin.json pins ' + pinned, refused.stderr)
            git(qmk, 'add', 'hook.c')
            git(qmk, 'commit', '-qm', 'moved on')
            refused = subprocess.run(['sh', source / 'tools/build-firmware-pair.sh'], cwd=base, env=env,
                                     capture_output=True, text=True)
            self.assertNotEqual(refused.returncode, 0)
            trial = subprocess.run(['sh', source / 'tools/build-firmware-pair.sh'], cwd=base,
                                   env=dict(env, NOAH_ALLOW_UNPINNED_QMK='1'), capture_output=True, text=True)
            self.assertEqual(trial.returncode, 0, trial.stderr)
            self.assertIn('TRIAL: building against BK', trial.stderr)
            self.assertEqual(len(calls), 2)
            for half in ('right', 'left'):
                artifact = base / 'builds/test-pair' / f'1_charybdis_{half}.uf2'
                self.assertEqual(artifact.read_bytes(), half.encode())
            # The note beside the pair names its inputs, image and hashes.
            note = (base / 'builds/test-pair/1_charybdis.build.txt').read_text()
            self.assertIn('bk ' + pinned + '\n', note)
            self.assertIn('image ' + (REPO / 'tools/build-image').read_text().strip(), note)
            for half in ('right', 'left'):
                digest = hashlib.sha256(half.encode()).hexdigest()
                self.assertIn(f'sha256 {digest} 1_charybdis_{half}.uf2', note)

    def test_outside_the_build_image_runs_itself_inside_it(self):
        with tempfile.TemporaryDirectory(prefix='pair-docker-') as tmp:
            base = Path(tmp).resolve()
            source, qmk, fakebin, builds = (base / name for name in ('userspace', 'qmk', 'bin', 'builds'))
            for folder in (source / 'tools', qmk, fakebin):
                folder.mkdir(parents=True)
            for tool in ('build-firmware-pair.sh', 'build-image'):
                shutil.copyfile(REPO / 'tools' / tool, source / 'tools' / tool)
            for root in (qmk, source):
                subprocess.run(['git', 'init', '-q', root], check=True)
                subprocess.run(['git', '-C', root, '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
                                'commit', '--allow-empty', '-qm', 'fixture'], check=True)
            pinned = subprocess.run(['git', '-C', qmk, 'rev-parse', 'HEAD'], check=True, capture_output=True, text=True).stdout.strip()
            (source / 'qmk-pin.json').write_text(json.dumps({'repository': 'https://github.com/NoahCLR/bastardkb-qmk', 'commit': pinned}))
            docker = fakebin / 'docker'
            docker.write_text('#!/bin/sh\n[ "$1" = info ] && exit "${DOCKER_INFO_STATUS:-0}"\n'
                              'python3 -c \'import json, sys; print(json.dumps(sys.argv[1:]))\' "$@" > "$DOCKER_LOG"\n')
            docker.chmod(0o755)
            env = dict(os.environ, PATH=str(fakebin) + os.pathsep + os.environ['PATH'], QMK_ROOT=str(qmk),
                       BUILD_ROOT=str(builds), DOCKER_LOG=str(base / 'docker.json'), NOAH_IN_BUILD_IMAGE='0',
                       NOAH_SPLIT_CRC='no')
            subprocess.run(['sh', source / 'tools/build-firmware-pair.sh', '--no-owner'], cwd=base, env=env, check=True,
                           capture_output=True)
            args = json.loads((base / 'docker.json').read_text())
            image = (REPO / 'tools/build-image').read_text().strip()
            self.assertEqual(args[0], 'run')
            self.assertEqual(args[args.index(image) + 1:],
                             ['sh', str(source / 'tools/build-firmware-pair.sh'), '--no-owner'])
            mounts = {args[i + 1] for i, arg in enumerate(args) if arg == '-v'}
            for folder in (source, source / '.git', qmk, qmk / '.git', builds):
                self.assertIn(f'{folder}:{folder}', mounts)
            self.assertIn('NOAH_IN_BUILD_IMAGE=1', args)
            self.assertIn('NOAH_SPLIT_CRC', args)
            self.assertNotIn('NOAH_IN_BUILD_IMAGE', args)
            # Without a running Docker it stops; it never builds with another compiler.
            stopped = subprocess.run(['sh', source / 'tools/build-firmware-pair.sh'], cwd=base,
                                     env=dict(env, DOCKER_INFO_STATUS='1'), capture_output=True, text=True)
            self.assertNotEqual(stopped.returncode, 0)
            self.assertIn('Docker is not running', stopped.stderr)

    def test_every_workflow_builds_in_the_build_image(self):
        image = (REPO / 'tools/build-image').read_text().strip()
        # Pinned by its multi-arch index digest (D-F05): a tag can be pushed again.
        self.assertRegex(image, r'^[a-z0-9./_-]+:[A-Za-z0-9._-]+@sha256:[0-9a-f]{64}$')
        found = []
        for workflow in (REPO / '.github/workflows').glob('*.y*ml'):
            for line in workflow.read_text().splitlines():
                if line.strip().startswith('image:'):
                    found.append((workflow.name, line.split(':', 1)[1].strip()))
        self.assertTrue(found)
        for name, used in found:
            self.assertEqual(used, image, f'{name} uses {used}, but tools/build-image names {image}')


if __name__ == '__main__':
    unittest.main()
