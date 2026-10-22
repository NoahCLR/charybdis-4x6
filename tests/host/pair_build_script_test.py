"""Build-driver regression tests: no compiler or physical keyboard required."""
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import hashlib

REPO = Path(__file__).resolve().parents[2]
# tools/make-build-image.sh runs this suite with NOAH_BUILD_IMAGE naming the new
# image; the scripts under test would take it in place of tools/build-image.
os.environ.pop('NOAH_BUILD_IMAGE', None)


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
            # shared output is overwritten. PAIR_TEST_FAIL names the gates
            # that fail, as half:gate.
            (source / 'tests/host').mkdir(parents=True)
            (source / 'tests/host/run_firmware_memory_budget_checks.sh').write_text(
                '#!/bin/sh\nhalf="$(cat "$QMK_ROOT/.build/bastardkb_charybdis_4x6_noah.elf")"\necho "$half"\n'
                'case " ${PAIR_TEST_FAIL:-} " in *" $half:memory "*) exit 1 ;; esac\n')
            (source / 'tools/check_firmware_stack_budget.py').write_text(
                "import argparse, os\nfrom pathlib import Path\n"
                "p=argparse.ArgumentParser();p.add_argument('--manifest');p.add_argument('--elf');p.add_argument('--map');a=p.parse_args()\n"
                "half=Path(a.elf).read_text();assert Path(a.map).read_text()==half\n"
                "print(half)\n"
                "gate=half+':'+Path(a.manifest).stem\n"
                "raise SystemExit(1 if gate in os.environ.get('PAIR_TEST_FAIL','').split() else 0)\n")
            resource_env = dict(env, NOAH_RESOURCE_CHECKS='yes')
            subprocess.run(['sh', source / 'tools/build-firmware-pair.sh'], cwd=base, env=resource_env,
                           capture_output=True, text=True, check=True)
            for half in ('right', 'left'):
                for extension in ('elf', 'map', 'cflags.txt', 'ldflags.txt', 'memory.txt',
                                  'firmware_stack_budget.txt', 'firmware_stack_budget_live_profile_owner.txt'):
                    content = (base / 'builds/test-pair' / f'1_charybdis_{half}_resources.{extension}').read_text()
                    self.assertEqual(content.strip(), half)
            # A failing gate hides no other: every gate runs on both halves and
            # reports, and the build fails at the end naming each failed report.
            failed = subprocess.run(['sh', source / 'tools/build-firmware-pair.sh'], cwd=base,
                                    env=dict(resource_env, PAIR_TEST_FAIL='right:firmware_stack_budget left:memory'),
                                    capture_output=True, text=True)
            self.assertNotEqual(failed.returncode, 0)
            self.assertNotIn('Built the', failed.stdout)
            self.assertTrue((base / 'builds/test-pair/2_charybdis_left_resources.uf2').exists())
            reports = {'memory.txt', 'firmware_stack_budget.txt', 'firmware_stack_budget_live_profile_owner.txt'}
            for half in ('right', 'left'):
                for report in reports:
                    content = (base / 'builds/test-pair' / f'2_charybdis_{half}_resources.{report}').read_text()
                    self.assertEqual(content.strip(), half)
            named = {line.strip() for line in failed.stderr.splitlines() if line.startswith('  ')}
            self.assertEqual(named, {str(base / 'builds/test-pair' / f'2_charybdis_{gate}')
                                     for gate in ('right_resources.firmware_stack_budget.txt',
                                                  'left_resources.memory.txt')})
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

    def test_pinned_build_refuses_changed_or_uninitialized_submodules(self):
        with tempfile.TemporaryDirectory(prefix='pair-submodules-') as tmp:
            base = Path(tmp)
            source, qmk, vendor, fakebin = (base / name for name in ('firmware', 'qmk', 'vendor', 'bin'))
            for folder in (source / 'tools', qmk, vendor, fakebin):
                folder.mkdir(parents=True)
            def git(root, *args):
                return subprocess.check_output(['git', '-C', str(root), '-c', 'user.name=Test',
                                                '-c', 'user.email=test@example.invalid',
                                                '-c', 'protocol.file.allow=always', *args],
                                               stderr=subprocess.DEVNULL, text=True).strip()
            for root in (source, qmk, vendor):
                git(root, 'init', '-q')
                git(root, 'commit', '--allow-empty', '-qm', 'initial')
            (vendor / 'runtime.c').write_text('int runtime = 1;\n')
            git(vendor, 'add', '.')
            git(vendor, 'commit', '-qm', 'runtime')
            nested_vendor = base / 'hal'
            nested_vendor.mkdir()
            git(nested_vendor, 'init', '-q')
            (nested_vendor / 'runtime.c').write_text('int hal = 1;\n')
            git(nested_vendor, 'add', '.')
            git(nested_vendor, 'commit', '-qm', 'hal')
            git(vendor, 'submodule', 'add', '-q', str(nested_vendor), 'hal')
            git(vendor, 'commit', '-qam', 'nested vendor')
            git(qmk, 'submodule', 'add', '-q', str(vendor), 'lib/chibios')
            git(qmk, 'commit', '-qam', 'vendor')
            git(qmk, 'submodule', 'update', '--init', '--recursive')
            pin = git(qmk, 'rev-parse', 'HEAD')
            for tool in ('build-firmware-pair.sh', 'build-image', 'qmk-copies.sh'):
                shutil.copyfile(REPO / 'tools' / tool, source / 'tools' / tool)
            (source / 'qmk-pin.json').write_text(json.dumps({'commit': pin}))
            docker = fakebin / 'docker'
            docker.write_text('#!/bin/sh\n[ "$1" = info ] && exit 0\nexit 0\n')
            docker.chmod(0o755)
            env = dict(os.environ, PATH=str(fakebin) + os.pathsep + os.environ['PATH'],
                       QMK_ROOT=str(qmk), BUILD_ROOT=str(base / 'builds'), NOAH_IN_BUILD_IMAGE='0')
            env.pop('NOAH_ALLOW_UNPINNED_QMK', None)
            def build():
                return subprocess.run(['sh', source / 'tools/build-firmware-pair.sh'], env=env,
                                      capture_output=True, text=True)
            self.assertEqual(build().returncode, 0)
            sub = qmk / 'lib/chibios'
            (sub / 'hal/runtime.c').write_text('int hal = 2;\n')
            self.assertNotEqual(build().returncode, 0, 'nested tracked dependency edits must be refused')
            git(sub / 'hal', 'checkout', '--', 'runtime.c')
            (sub / 'runtime.c').write_text('int runtime = 2;\n')
            result = build()
            self.assertNotEqual(result.returncode, 0, 'tracked ChibiOS edits must invalidate a pinned build')
            self.assertIn('with local changes', result.stderr)
            git(sub, 'add', '.')
            git(sub, 'commit', '-qm', 'different vendor commit')
            self.assertNotEqual(build().returncode, 0, 'a different submodule commit must be refused')
            git(qmk, 'submodule', 'update', '--init', '--recursive')
            # Finder metadata does not alter tracked compiler inputs.
            (sub / '.DS_Store').write_bytes(b'finder metadata')
            self.assertEqual(build().returncode, 0)
            git(qmk, 'submodule', 'deinit', '-f', '--all')
            self.assertNotEqual(build().returncode, 0, 'an uninitialized dependency must be refused')

    def docker_fixture(self, base):
        """A userspace and a BK checkout at its pin, with a stand-in docker that logs every call."""
        source, qmk, fakebin, builds = (base / name for name in ('userspace', 'qmk', 'bin', 'builds'))
        for folder in (source / 'tools', qmk, fakebin):
            folder.mkdir(parents=True)
        for tool in ('build-firmware-pair.sh', 'build-image', 'qmk-copies.sh'):
            shutil.copyfile(REPO / 'tools' / tool, source / 'tools' / tool)
        for root in (qmk, source):
            subprocess.run(['git', 'init', '-q', root], check=True)
            subprocess.run(['git', '-C', root, '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
                            'commit', '--allow-empty', '-qm', 'fixture'], check=True)
        pinned = subprocess.run(['git', '-C', qmk, 'rev-parse', 'HEAD'], check=True, capture_output=True, text=True).stdout.strip()
        (source / 'qmk-pin.json').write_text(json.dumps({'repository': 'https://github.com/NoahCLR/bastardkb-qmk', 'commit': pinned}))
        docker = fakebin / 'docker'
        docker.write_text('#!/bin/sh\n[ "$1" = info ] && exit "${DOCKER_INFO_STATUS:-0}"\n'
                          'python3 -c \'import json, sys; print(json.dumps(sys.argv[1:]))\' "$@" >> "$DOCKER_LOG"\n')
        docker.chmod(0o755)
        env = dict(os.environ, NOAH_RESOURCE_CHECKS="no", PATH=str(fakebin) + os.pathsep + os.environ['PATH'], QMK_ROOT=str(qmk),
                   BUILD_ROOT=str(builds), DOCKER_LOG=str(base / 'docker.json'), NOAH_IN_BUILD_IMAGE='0',
                   NOAH_SPLIT_CRC='no')
        return source, qmk, builds, pinned, env

    def build_call(self, base):
        calls = [json.loads(line) for line in (base / 'docker.json').read_text().splitlines()]
        return calls, [call for call in calls if call[0] == 'run' and 'flock' not in call][-1]

    def test_outside_the_build_image_runs_itself_inside_it(self):
        with tempfile.TemporaryDirectory(prefix='pair-docker-') as tmp:
            base = Path(tmp).resolve()
            source, qmk, builds, pinned, env = self.docker_fixture(base)
            subprocess.run(['sh', source / 'tools/build-firmware-pair.sh', '--no-owner'], cwd=base, env=env, check=True,
                           capture_output=True)
            calls, args = self.build_call(base)
            image = (REPO / 'tools/build-image').read_text().strip()
            self.assertEqual(args[0], 'run')
            # The build cannot download anything (D-F05).
            self.assertEqual(args[args.index('--network') + 1], 'none')
            # It compiles copies held in memory at the checkouts' own paths: the
            # userspace, and BK from the volume of its commit, made under a lock.
            command = args[args.index(image) + 1:]
            self.assertEqual(command[:2], ['sh', '-c'])
            self.assertIn(f"cd '{source}' && exec sh tools/build-firmware-pair.sh", command[2])
            self.assertEqual(command[3:], ['pair', '--no-owner'])
            tmpfs = {args[i + 1].split(':')[0] for i, arg in enumerate(args) if arg == '--tmpfs'}
            self.assertEqual(tmpfs, {str(source), str(qmk)})
            mounts = {args[i + 1] for i, arg in enumerate(args) if arg == '-v'}
            self.assertIn(f'{source}:/mnt/noah/repo:ro', mounts)
            self.assertIn(f'charybdis-host-qmk-{pinned}:/mnt/noah/qmk:ro', mounts)
            self.assertTrue(any('flock' in call for call in calls), 'the volume is made under a lock')
            for folder in (source / '.git', qmk / '.git'):
                self.assertIn(f'{folder}:{folder}:ro', mounts)
            # The pair is all it writes: nothing in BK, so builds need no lock.
            self.assertIn(f'{builds}:{builds}', mounts)
            self.assertFalse([m for m in mounts if m.split(':')[0] in (str(source), str(qmk)) and not m.endswith(':ro')])
            self.assertIn('GIT_OPTIONAL_LOCKS=0', args)
            self.assertIn('NOAH_IN_BUILD_IMAGE=1', args)
            self.assertIn('NOAH_SPLIT_CRC', args)
            self.assertNotIn('NOAH_IN_BUILD_IMAGE', args)
            # The vault's verify reads this sentence to know it may skip the build lock.
            self.assertIn('Outside the build image it writes nothing into BK',
                          ' '.join(line.lstrip('# ') for line in (REPO / 'tools/build-firmware-pair.sh').read_text().splitlines()))
            # Without a running Docker it stops; it never builds with another compiler.
            stopped = subprocess.run(['sh', source / 'tools/build-firmware-pair.sh'], cwd=base,
                                     env=dict(env, DOCKER_INFO_STATUS='1'), capture_output=True, text=True)
            self.assertNotEqual(stopped.returncode, 0)
            self.assertIn('Docker is not running', stopped.stderr)

    def test_a_changed_bk_is_copied_from_the_checkout_for_that_build(self):
        # A volume is named by the commit alone, so it cannot hold a local change.
        with tempfile.TemporaryDirectory(prefix='pair-docker-') as tmp:
            base = Path(tmp).resolve()
            source, qmk, builds, pinned, env = self.docker_fixture(base)
            (qmk / 'local_change.h').write_text('')
            subprocess.run(['sh', source / 'tools/build-firmware-pair.sh'], cwd=base, check=True, capture_output=True,
                           env=dict(env, NOAH_ALLOW_UNPINNED_QMK='1'))
            calls, args = self.build_call(base)
            mounts = {args[i + 1] for i, arg in enumerate(args) if arg == '-v'}
            self.assertIn(f'{qmk}:/mnt/noah/qmk:ro', mounts)
            self.assertFalse([m for m in mounts if m.startswith('charybdis-host-qmk-')])
            self.assertFalse(any('flock' in call for call in calls), 'no volume is made for a changed BK')
            self.assertIn('--exclude=./.build', ' '.join(args))
            self.assertIn(f'--tmpfs', args)

    def test_bk_inside_the_checkout_is_refused_outside_the_image(self):
        # Its copy would land on its own mount; CI builds such a layout in the image.
        with tempfile.TemporaryDirectory(prefix='pair-docker-') as tmp:
            base = Path(tmp).resolve()
            source, qmk, builds, pinned, env = self.docker_fixture(base)
            nested = source / 'qmk_firmware'
            qmk.rename(nested)
            refused = subprocess.run(['sh', source / 'tools/build-firmware-pair.sh'], cwd=base, capture_output=True, text=True,
                                     env=dict(env, QMK_ROOT=str(nested)))
            self.assertNotEqual(refused.returncode, 0)
            self.assertIn('inside this checkout', refused.stderr)
            self.assertFalse((base / 'docker.json').exists(), 'nothing ran in Docker')

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
        # In a container job the runners and the pair build run in place; saying
        # so does not depend on how the runner's Docker marks a container.
        for workflow in (REPO / '.github/workflows').glob('*.y*ml'):
            text = workflow.read_text()
            self.assertEqual(text.count('\n      image: '), text.count('\n      NOAH_IN_BUILD_IMAGE: 1\n'), workflow.name)


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
    platform = args[args.index('--platform') + 1].split('/')[1].upper()
    if 'find' in args[-1]:
        print(os.environ.get('FAKE_TARGET_' + platform, 'same') + '  -')
    else:
        sys.exit(int(os.environ.get('FAKE_TOOLS_FAIL_' + platform, '0')))
elif args[0] == 'push':
    pushed[args[1]] = 'sha256:' + 'd' * 64
    state.write_text(json.dumps(pushed))
else:
    sys.exit(f'unexpected docker call: {args}')
"""

FAKE_SUITE = """#!/bin/sh
echo "suite $NOAH_BUILD_IMAGE" >>"$FAKE_DOCKER_LOG"
exit "${FAKE_SUITE_FAIL:-0}"
"""

FAKE_PAIR = """#!/bin/sh
echo "pair $NOAH_BUILD_IMAGE resources=$NOAH_RESOURCE_CHECKS" >>"$FAKE_DOCKER_LOG"
[ "$NOAH_RESOURCE_CHECKS" = yes ] && [ -n "${FAKE_GATE_FAILS:-}" ] && exit 1
mkdir -p "$BUILD_ROOT/branch"
bytes=same; [ "$NOAH_BUILD_IMAGE" = "${FAKE_CHANGED_IMAGE:-}" ] && bytes=changed
printf %s "$bytes" > "$BUILD_ROOT/branch/1_charybdis_right.uf2"
printf left > "$BUILD_ROOT/branch/1_charybdis_left.uf2"
"""

TAG = 'ghcr.io/noahclr/charybdis-build:qmk-cli-1.2.0-gcc15.2.0-r1'
OLD = 'ghcr.io/noahclr/charybdis-build:qmk-cli-1.2.0-gcc15.2.0-r0@sha256:' + 'a' * 64
WORKFLOW = f"""jobs:
  host:
    container:
      image: {OLD}
  other:
    container:
      image: ghcr.io/example/other:1
"""


class MakeBuildImage(unittest.TestCase):
    # The script runs in a scratch copy of the repository, so pinning writes
    # there, and the host suite it runs is a stand-in.
    def run_make(self, tmp, *args, **env):
        repo, fakebin = Path(tmp) / 'repo', Path(tmp) / 'bin'
        for folder in (repo / 'tools', repo / 'tests/host', repo / '.github/workflows', fakebin):
            folder.mkdir(parents=True, exist_ok=True)
        for tool in ('make-build-image.sh', 'build-image.dockerfile'):
            shutil.copyfile(REPO / 'tools' / tool, repo / 'tools' / tool)
        (repo / 'tools/build-image').write_text(OLD + '\n')
        (repo / '.github/workflows/ci.yml').write_text(WORKFLOW)
        (repo / 'tests/host/run_all_host_tests.sh').write_text(FAKE_SUITE)
        (repo / 'tools/build-firmware-pair.sh').write_text(FAKE_PAIR)
        docker = fakebin / 'docker'
        docker.write_text(FAKE_DOCKER)
        docker.chmod(0o755)
        return subprocess.run(['sh', str(repo / 'tools/make-build-image.sh'), *args],
                              env=dict(os.environ, NOAH_RESOURCE_CHECKS="no", PATH=f'{fakebin}:{os.environ["PATH"]}',
                                       FAKE_REGISTRY=str(Path(tmp) / 'registry.json'),
                                       FAKE_DOCKER_LOG=str(Path(tmp) / 'docker.log'), **env),
                              capture_output=True, text=True)

    def calls(self, tmp):
        return (Path(tmp) / 'docker.log').read_text().splitlines()

    def pinned(self, tmp):
        repo = Path(tmp) / 'repo'
        return (repo / 'tools/build-image').read_text(), (repo / '.github/workflows/ci.yml').read_text()

    def test_builds_checks_tests_then_pushes_and_pins(self):
        with tempfile.TemporaryDirectory(prefix='make-image-') as tmp:
            done = self.run_make(tmp, 'qmk-cli-1.2.0-gcc15.2.0-r1')
            self.assertEqual(done.returncode, 0, done.stderr)
            pin = f'{TAG}@sha256:' + 'd' * 64
            self.assertEqual(done.stdout.strip().splitlines()[-1], pin)
            order = [call.split()[0] if call.split()[0] != 'buildx' else ' '.join(call.split()[:2])
                     for call in self.calls(tmp)]
            self.assertEqual(order[1:], ['buildx build', 'run', 'run', 'run', 'run', 'suite', 'pair', 'pair', 'pair', 'push',
                                         'buildx imagetools'],
                             'both platforms, the host suite and the pair are checked before anything is pushed')
            self.assertIn(f'suite {TAG}', self.calls(tmp), 'the suite runs in the new image')
            # The pair with every resource gate, then the ordinary pair in each image.
            self.assertEqual([call for call in self.calls(tmp) if call.startswith('pair')],
                             [f'pair {TAG} resources=yes', f'pair {TAG} resources=no', f'pair {OLD} resources=no'])
            self.assertIn('byte-identical', done.stderr)
            image, workflow = self.pinned(tmp)
            self.assertEqual(image, pin + '\n')
            self.assertEqual(workflow, WORKFLOW.replace(OLD, pin), 'every workflow line that used the pin moves with it')

    def test_never_reuses_a_tag(self):
        with tempfile.TemporaryDirectory(prefix='make-image-') as tmp:
            (Path(tmp) / 'registry.json').write_text(json.dumps({TAG: 'sha256:' + 'c' * 64}))
            refused = self.run_make(tmp, 'qmk-cli-1.2.0-gcc15.2.0-r1')
            self.assertNotEqual(refused.returncode, 0)
            self.assertIn('never reuse a tag', refused.stderr)
            self.assertFalse(any(call.startswith('buildx build') for call in self.calls(tmp)))

    def assert_nothing_pushed(self, tmp, failed, reason):
        self.assertNotEqual(failed.returncode, 0)
        self.assertIn(reason, failed.stderr)
        self.assertIn('nothing was pushed', failed.stderr)
        self.assertFalse(any(call.startswith('push') for call in self.calls(tmp)))
        self.assertEqual(self.pinned(tmp), (OLD + '\n', WORKFLOW), 'nothing was pinned')

    def test_pushes_nothing_when_the_platforms_differ(self):
        with tempfile.TemporaryDirectory(prefix='make-image-') as tmp:
            failed = self.run_make(tmp, 'qmk-cli-1.2.0-gcc15.2.0-r1', FAKE_TARGET_ARM64='other')
            self.assert_nothing_pushed(tmp, failed, 'target files differ')

    def test_pushes_nothing_when_a_tool_does_not_run(self):
        with tempfile.TemporaryDirectory(prefix='make-image-') as tmp:
            failed = self.run_make(tmp, 'qmk-cli-1.2.0-gcc15.2.0-r1', FAKE_TOOLS_FAIL_AMD64='1')
            self.assert_nothing_pushed(tmp, failed, 'linux/amd64')

    def test_pushes_nothing_when_the_host_suite_fails(self):
        with tempfile.TemporaryDirectory(prefix='make-image-') as tmp:
            failed = self.run_make(tmp, 'qmk-cli-1.2.0-gcc15.2.0-r1', FAKE_SUITE_FAIL='1')
            self.assert_nothing_pushed(tmp, failed, 'host suite failed')

    def test_pushes_nothing_when_a_resource_gate_fails_in_the_new_image(self):
        with tempfile.TemporaryDirectory(prefix='make-image-') as tmp:
            failed = self.run_make(tmp, 'qmk-cli-1.2.0-gcc15.2.0-r1', FAKE_GATE_FAILS='1')
            self.assert_nothing_pushed(tmp, failed, 'resource gate failed')

    def test_says_when_the_new_compiler_changes_the_pair(self):
        with tempfile.TemporaryDirectory(prefix='make-image-') as tmp:
            done = self.run_make(tmp, 'qmk-cli-1.2.0-gcc15.2.0-r1', FAKE_CHANGED_IMAGE=TAG)
            self.assertEqual(done.returncode, 0, done.stderr)
            self.assertIn('the compiler changed', done.stderr)
            self.assertIn('memory-budgets.md', done.stderr)

    def test_the_dockerfile_takes_target_files_from_the_official_amd64_variant(self):
        dockerfile = (REPO / 'tools/build-image.dockerfile').read_text()
        source = dockerfile.split('ARG QMK_CLI=', 1)[1].split()[0]
        self.assertRegex(source, r'^ghcr\.io/qmk/qmk_cli@sha256:[0-9a-f]{64}$')
        self.assertIn('FROM --platform=linux/amd64 ${QMK_CLI} AS target', dockerfile)
        script = (REPO / 'tools/make-build-image.sh').read_text()
        copied = sorted(line.split()[2] for line in dockerfile.splitlines() if line.startswith('COPY --from=target'))
        checked = sorted('/opt/qmk/' + d for d in script.split('TARGET_DIRS="', 1)[1].split('"', 1)[0].split())
        self.assertEqual(copied, checked, 'the script checks exactly the directories the image replaces')

    def test_the_tools_are_pinned_by_checksum_and_need_no_package_manager(self):
        dockerfile = (REPO / 'tools/build-image.dockerfile').read_text()
        sums = dict(re.findall(r'^ARG (\w+_SHA256_\w+)=(\S*)$', dockerfile, re.M))
        self.assertEqual(sorted(sums), ['NODE_SHA256_AMD64', 'NODE_SHA256_ARM64',
                                        'RIPGREP_SHA256_AMD64', 'RIPGREP_SHA256_ARM64'])
        for name, value in sums.items():
            self.assertRegex(value, r'^[0-9a-f]{64}$', name)
        self.assertIn('sha256sum -c -', dockerfile)
        for manager in ('apt-get', 'apt ', 'dpkg', 'npm '):
            self.assertNotIn(manager, dockerfile, 'what the image holds comes from this file alone')
        final = dockerfile.rsplit('\nFROM ', 1)[1]
        self.assertEqual([line for line in final.splitlines() if line and not line.startswith(('#', 'COPY --from=target'))][1:],
                         ['COPY --from=tools --chown=0:0 /out/node /out/rg /usr/local/bin/'],
                         'the build image adds only the two tools to the official one')


FAKE_GUARD_DOCKER = """#!/usr/bin/env python3
import json, os, sys
args = sys.argv[1:]
open(os.environ['FAKE_DOCKER_LOG'], 'a').write(json.dumps(args) + '\\n')
volumes = json.loads(os.environ.get('FAKE_VOLUMES', '{}'))
if args[:2] == ['volume', 'inspect'] and '--format' not in args:
    sys.exit(0 if args[2] in volumes else 1)
if args[:2] == ['volume', 'ls']:
    print('\\n'.join(volumes))
elif args[:2] == ['volume', 'inspect']:
    for name in args[4:]:
        print(volumes.get(name, '2026-10-22T12:00:00Z'), name)
elif args[0] == 'run' and 'flock' not in args and 'run_contract_probe.sh' in ' '.join(args):
    assert '--output' not in args, 'the container cannot see the output file'
    print('{"contract": true}')
"""

PROBE_RUNNER = """#!/bin/sh
set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"
"""


class InBuildImage(unittest.TestCase):
    """The Docker path of tests/host/in_build_image.sh, with a stand-in docker."""

    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix='in-image-'))
        self.addCleanup(shutil.rmtree, self.tmp)
        self.repo, self.qmk, fakebin = self.tmp / 'repo', self.tmp / 'qmk', self.tmp / 'bin'
        for folder in (self.repo / 'tests/host', self.repo / 'tools', self.qmk / 'quantum', fakebin):
            folder.mkdir(parents=True)
        for name in ('in_build_image.sh', 'noah_host_qmk_env.sh', 'run_contract_probe.sh'):
            shutil.copyfile(REPO / 'tests/host' / name, self.repo / 'tests/host' / name)
        shutil.copyfile(REPO / 'tools/qmk-copies.sh', self.repo / 'tools/qmk-copies.sh')
        (self.repo / 'tests/host/run_example_tests.sh').write_text(PROBE_RUNNER)
        (self.repo / 'tools/build-image').write_text(OLD + '\n')
        for name in ('quantum_keycodes.h', 'keycodes.h'):
            (self.qmk / 'quantum' / name).write_text('')
        git = lambda *args: subprocess.run(['git', '-C', self.qmk, '-c', 'user.name=Test',
                                            '-c', 'user.email=test@example.invalid', *args],
                                           check=True, capture_output=True, text=True).stdout.strip()
        git('init', '-q')
        git('add', '.')
        git('commit', '-qm', 'qmk')
        self.head = git('rev-parse', 'HEAD')
        docker = fakebin / 'docker'
        docker.write_text(FAKE_GUARD_DOCKER)
        docker.chmod(0o755)
        self.env = dict(os.environ, NOAH_IN_BUILD_IMAGE='0', QMK_ROOT=str(self.qmk),
                        PATH=f'{fakebin}:{os.environ["PATH"]}', FAKE_DOCKER_LOG=str(self.tmp / 'docker.log'))

    def run_runner(self, name, *args, **env):
        done = subprocess.run(['sh', str(self.repo / 'tests/host' / name), *args], env=dict(self.env, **env),
                              capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stderr)
        return done

    def calls(self):
        return [json.loads(line) for line in (self.tmp / 'docker.log').read_text().splitlines()]

    def runner_call(self):
        return [call for call in self.calls() if call[0] == 'run' and 'flock' not in call][-1]

    def test_every_runner_runs_in_the_build_image(self):
        # A runner without the guard would compile with whatever the Mac has,
        # and pass where CI fails (D-F05).
        line = '. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"'
        missing = [runner.name for runner in sorted((REPO / 'tests/host').glob('run_*.sh'))
                   if line not in runner.read_text().splitlines()]
        self.assertEqual(missing, [], 'each tests/host runner sources in_build_image.sh at top level')

    def test_a_clean_qmk_is_read_from_its_commit_volume(self):
        self.run_runner('run_example_tests.sh', FAKE_VOLUMES=json.dumps({f'charybdis-host-qmk-{self.head}': 'x'}))
        self.assertIn(f'charybdis-host-qmk-{self.head}:{self.qmk}', self.runner_call())
        self.assertTrue(any('flock' in call for call in self.calls()), 'the copy is made under a lock')

    def test_a_qmk_inside_the_checkout_is_used_in_place(self):
        # Copying the checkout would write over QMK's own mount, or its volume.
        nested = self.repo / 'qmk_firmware'
        shutil.copytree(self.qmk, nested)
        self.run_runner('run_example_tests.sh', QMK_ROOT=str(nested),
                        FAKE_VOLUMES=json.dumps({f'charybdis-host-qmk-{self.head}': 'x'}))
        call = ' '.join(self.runner_call())
        self.assertNotIn('--tmpfs', call)
        self.assertIn(f'-v {self.repo}:{self.repo}', call)
        self.assertIn(f'-v {nested}:{nested}', call)

    def test_untracked_files_in_qmk_are_copied_every_run(self):
        # A volume is named by the commit only, so it cannot hold a new file.
        (self.qmk / 'quantum/new_header.h').write_text('')
        self.run_runner('run_example_tests.sh')
        call = ' '.join(self.runner_call())
        self.assertIn(f'--tmpfs {self.qmk}:', call)
        self.assertNotIn('charybdis-host-qmk-', call)

    def test_a_new_volume_keeps_the_three_newest(self):
        volumes = {f'charybdis-host-qmk-{n}': f'2026-10-0{n}T00:00:00Z' for n in range(1, 6)}
        self.run_runner('run_example_tests.sh', FAKE_VOLUMES=json.dumps(volumes))
        removed = [name for call in self.calls() if call[:2] == ['volume', 'rm'] for name in call[2:]]
        self.assertEqual(sorted(removed), ['charybdis-host-qmk-1', 'charybdis-host-qmk-2'])

    def test_the_contract_is_written_wherever_the_client_asks(self):
        # A client's temporary directory is not visible inside the container.
        output = self.tmp / 'client-temp/contract.json'
        output.parent.mkdir()
        self.run_runner('run_contract_probe.sh', '--output', str(output))
        self.assertEqual(output.read_text(), '{"contract": true}\n')
        self.assertFalse(output.with_name('contract.json.partial').exists())

if __name__ == '__main__':
    unittest.main()
