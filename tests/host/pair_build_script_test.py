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
assert sys.argv[4:6] == ['-j', '0'], 'compile on every core'
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
if 'NOAH_STACK_BUDGET_ENABLE=yes' in sys.argv:
    for extension in ('elf', 'map'):
        Path('.build/bastardkb_charybdis_4x6_noah.' + extension).write_text(half)
    for flags in ('cflags', 'ldflags'):
        Path('.build/obj_bastardkb_charybdis_4x6_noah/' + flags + '.txt').write_text(half)
''')
            driver.chmod(0o755)
            stale = qmk / '.build/obj_bastardkb_charybdis_4x6_noah'
            stale.mkdir(parents=True)
            (stale / 'runtime_init.d').write_text('runtime_init.o: /removed/worktree/runtime_init.c\n')
            (stale / 'runtime_init.o').write_bytes(b'stale')
            env = dict(os.environ, NOAH_RESOURCE_CHECKS="no", PATH=str(fakebin) + os.pathsep + os.environ['PATH'], QMK_ROOT=str(qmk),
                       QMK_HOME='/wrong/qmk', QMK_USERSPACE='/wrong/userspace', EXPECTED_USERSPACE=str(source),
                       BUILD_ROOT=str(base / 'builds'), PAIR_TEST_LOG=str(base / 'calls'), NOAH_IN_BUILD_IMAGE='1')
            subprocess.run(['sh', source / 'tools/build-firmware-pair.sh'], cwd=base, env=env, check=True)
            calls = [json.loads(line) for line in (base / 'calls').read_text().splitlines()]
            # Resource gates must check and preserve EACH half before QMK's
            # shared output is overwritten. A gate failure stops the pair.
            (source / 'tests/host').mkdir(parents=True)
            (source / 'tests/host/run_firmware_memory_budget_checks.sh').write_text(
                '#!/bin/sh\ncat "$QMK_ROOT/.build/bastardkb_charybdis_4x6_noah.elf"\n')
            (source / 'tools/check_firmware_stack_budget.py').write_text(
                "import argparse, os\nfrom pathlib import Path\n"
                "p=argparse.ArgumentParser();p.add_argument('--manifest');p.add_argument('--elf');p.add_argument('--map');a=p.parse_args()\n"
                "half=Path(a.elf).read_text();assert Path(a.map).read_text()==half\n"
                "print(half)\n"
                "raise SystemExit(1 if os.environ.get('PAIR_TEST_RESOURCE_FAIL')==half else 0)\n")
            resource_env = dict(env, NOAH_RESOURCE_CHECKS='yes')
            subprocess.run(['sh', source / 'tools/build-firmware-pair.sh'], cwd=base, env=resource_env,
                           capture_output=True, text=True, check=True)
            for half in ('right', 'left'):
                for extension in ('elf', 'map', 'cflags.txt', 'ldflags.txt', 'memory.txt',
                                  'firmware_stack_budget.txt', 'firmware_stack_budget_live_profile_owner.txt'):
                    content = (base / 'builds/test-pair' / f'1_charybdis_{half}_resources.{extension}').read_text()
                    self.assertEqual(content.strip(), half)
            failed = subprocess.run(['sh', source / 'tools/build-firmware-pair.sh'], cwd=base,
                                    env=dict(resource_env, PAIR_TEST_RESOURCE_FAIL='right'),
                                    capture_output=True, text=True)
            self.assertNotEqual(failed.returncode, 0)
            self.assertFalse((base / 'builds/test-pair/2_charybdis_left_resources.uf2').exists())
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
            self.assertIn('Not the pin: building against BK', trial.stderr)
            self.assertEqual(len(calls), 2)
            for half in ('right', 'left'):
                artifact = base / 'builds/test-pair' / f'1_charybdis_{half}.uf2'
                self.assertEqual(artifact.read_bytes(), half.encode())
            # The note beside the pair names its inputs, image and hashes.
            note = (base / 'builds/test-pair/1_charybdis.build.txt').read_text()
            self.assertIn('bk ' + pinned + '\n', note)
            # The unpinned pair's note says so, so it is never taken for a pinned one.
            trial_note = sorted((base / 'builds/test-pair').glob('*_charybdis.build.txt'))[-1].read_text()
            self.assertIn(' (not the pin ' + pinned + ')\n', trial_note)
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
            env = dict(os.environ, NOAH_RESOURCE_CHECKS="no", PATH=str(fakebin) + os.pathsep + os.environ['PATH'], QMK_ROOT=str(qmk),
                       BUILD_ROOT=str(builds), DOCKER_LOG=str(base / 'docker.json'), NOAH_IN_BUILD_IMAGE='0',
                       NOAH_SPLIT_CRC='no')
            subprocess.run(['sh', source / 'tools/build-firmware-pair.sh', '--no-owner'], cwd=base, env=env, check=True,
                           capture_output=True)
            args = json.loads((base / 'docker.json').read_text())
            image = (REPO / 'tools/build-image').read_text().strip()
            self.assertEqual(args[0], 'run')
            # The build cannot download anything (D-F05).
            self.assertEqual(args[args.index('--network') + 1], 'none')
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

    def test_vs_code_builds_the_checkouts_as_they_are(self):
        # VS Code's pair tasks are development builds: the BK checkout beside this
        # one need not be at the pin. verify, CI and releases build at the pin.
        tasks = json.loads((REPO / '.vscode/tasks.json').read_text())['tasks']
        pair = [t for t in tasks if 'tools/build-firmware-pair.sh' in ' '.join(t.get('args', []))]
        self.assertEqual(len(pair), 2)
        for task in pair:
            self.assertEqual(task['options'].get('env', {}).get('NOAH_ALLOW_UNPINNED_QMK'), '1', task['label'])

    def test_every_workflow_builds_in_the_build_image(self):
        image = (REPO / 'tools/build-image').read_text().strip()
        # Our own copy, pinned by its multi-arch index digest (D-F05): a tag can
        # be pushed again, and an image we do not own can be moved or deleted.
        self.assertRegex(image, r'^ghcr\.io/noahclr/charybdis-build:[A-Za-z0-9._-]+@sha256:[0-9a-f]{64}$')
        found = []
        for workflow in (REPO / '.github/workflows').glob('*.y*ml'):
            for line in workflow.read_text().splitlines():
                if line.strip().startswith('image:'):
                    found.append((workflow.name, line.split(':', 1)[1].strip()))
        self.assertTrue(found)
        for name, used in found:
            self.assertEqual(used, image, f'{name} uses {used}, but tools/build-image names {image}')


FAKE_DOCKER = """#!/usr/bin/env python3
import json, os, sys
from pathlib import Path
state = Path(os.environ['FAKE_REGISTRY'])
pushed = json.loads(state.read_text()) if state.exists() else {}
args = sys.argv[1:]
log = open(os.environ['FAKE_DOCKER_LOG'], 'a')
log.write(' '.join(args) + '\\n')
if args[:3] == ['buildx', 'imagetools', 'inspect']:
    digest = pushed.get(args[3])
    if not digest:
        sys.exit(1)
    print(f'Name:      {args[3]}')
    print(f'Digest:    {digest}')
elif args[:2] == ['buildx', 'build']:
    assert args[args.index('--platform') + 1] == 'linux/amd64,linux/arm64', args
    assert '--load' in args and '--push' not in args, 'nothing is pushed before the check'
    assert args[args.index('-f') + 1].endswith('tools/build-image.dockerfile'), args
elif args[0] == 'run':
    platform = args[args.index('--platform') + 1]
    print(os.environ.get('FAKE_TARGET_' + platform.split('/')[1].upper(), 'same') + '  -')
elif args[0] == 'push':
    pushed[args[1]] = 'sha256:' + 'd' * 64
    state.write_text(json.dumps(pushed))
else:
    sys.exit(f'unexpected docker call: {args}')
"""

TAG = 'ghcr.io/noahclr/charybdis-build:qmk-cli-1.2.0-gcc15.2.0-r1'


class MakeBuildImage(unittest.TestCase):
    def run_make(self, tmp, *args, **env):
        fakebin = Path(tmp) / 'bin'
        fakebin.mkdir(exist_ok=True)
        docker = fakebin / 'docker'
        docker.write_text(FAKE_DOCKER)
        docker.chmod(0o755)
        return subprocess.run(['sh', str(REPO / 'tools/make-build-image.sh'), *args],
                              env=dict(os.environ, NOAH_RESOURCE_CHECKS="no", PATH=f'{fakebin}:{os.environ["PATH"]}',
                                       FAKE_REGISTRY=str(Path(tmp) / 'registry.json'),
                                       FAKE_DOCKER_LOG=str(Path(tmp) / 'docker.log'), **env),
                              capture_output=True, text=True)

    def calls(self, tmp):
        return (Path(tmp) / 'docker.log').read_text().splitlines()

    def test_builds_checks_then_pushes_and_prints_the_pin(self):
        with tempfile.TemporaryDirectory(prefix='make-image-') as tmp:
            done = self.run_make(tmp, 'qmk-cli-1.2.0-gcc15.2.0-r1')
            self.assertEqual(done.returncode, 0, done.stderr)
            self.assertEqual(done.stdout.strip().splitlines()[-1], f'{TAG}@sha256:' + 'd' * 64)
            order = [call.split()[0] if call.split()[0] != 'buildx' else ' '.join(call.split()[:2])
                     for call in self.calls(tmp)]
            self.assertEqual(order[1:], ['buildx build', 'run', 'run', 'push', 'buildx imagetools'],
                             'both platforms are checked before anything is pushed')

    def test_never_reuses_a_tag(self):
        with tempfile.TemporaryDirectory(prefix='make-image-') as tmp:
            (Path(tmp) / 'registry.json').write_text(json.dumps({TAG: 'sha256:' + 'c' * 64}))
            refused = self.run_make(tmp, 'qmk-cli-1.2.0-gcc15.2.0-r1')
            self.assertNotEqual(refused.returncode, 0)
            self.assertIn('never reuse a tag', refused.stderr)
            self.assertFalse(any(call.startswith('buildx build') for call in self.calls(tmp)))

    def test_pushes_nothing_when_the_platforms_differ(self):
        with tempfile.TemporaryDirectory(prefix='make-image-') as tmp:
            failed = self.run_make(tmp, 'qmk-cli-1.2.0-gcc15.2.0-r1', FAKE_TARGET_ARM64='other')
            self.assertNotEqual(failed.returncode, 0)
            self.assertIn('nothing was pushed', failed.stderr)
            self.assertFalse(any(call.startswith('push') for call in self.calls(tmp)))

    def test_the_dockerfile_takes_target_files_from_the_official_amd64_variant(self):
        dockerfile = (REPO / 'tools/build-image.dockerfile').read_text()
        source = dockerfile.split('ARG QMK_CLI=', 1)[1].split()[0]
        self.assertRegex(source, r'^ghcr\.io/qmk/qmk_cli@sha256:[0-9a-f]{64}$')
        self.assertIn('FROM --platform=linux/amd64 ${QMK_CLI} AS target', dockerfile)
        script = (REPO / 'tools/make-build-image.sh').read_text()
        copied = sorted(line.split()[2] for line in dockerfile.splitlines() if line.startswith('COPY --from=target'))
        checked = sorted('/opt/qmk/' + d for d in script.split('TARGET_DIRS="', 1)[1].split('"', 1)[0].split())
        self.assertEqual(copied, checked, 'the script checks exactly the directories the image replaces')

if __name__ == '__main__':
    unittest.main()
