"""Exercise configuration compilation and linked-proof rejection independently.

Synthetic headers/compiler macros isolate selectors; actual target proof uses the
recorded GCC flags/forced headers and ARM ELF in the post-link gate.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / 'tools/firmware_memory_configuration.py'
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    (work / 'keyboard.h').write_text('')
    (work / 'process_combo.h').write_text('')
    (work / 'transactions.h').write_text('')
    compat = work / 'lib/compat'
    compat.mkdir(parents=True)
    (compat / 'qmk_lighting_live.h').write_text(
        '#define NOAH_LIGHTING_LIVE_AVAILABLE 1\n'
        '#define NOAH_LIGHTING_FACT_SLICE_COUNT 17\n'
        'typedef struct { unsigned char bytes[621]; } noah_lighting_facts_t;\n')
    config = dict(MATRIX_ROWS=10, MATRIX_COLS=6, COMBO_BUFFER_LENGTH=4,
                  NOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1, NOAH_LIGHTING_LIVE_ENABLE=1,
                  NOAH_PD_PROFILE_ENABLE=1, NOAH_PORTABLE_PROFILE_ENABLE=1,
                  NOAH_LIVE_PROFILE_OWNER_ENABLE=1, RGB_MATRIX_ENABLE=1, VIA_ENABLE=1,
                  SPLIT_KEYBOARD=1, NOAH_LIGHTING_TARGET_KEYS_THAT=1,
                  NOAH_LIGHTING_PAINT_WHAT_IT_DOES=1, RGB_MATRIX_LED_COUNT=58,
                  RGB_LEFT_LED_COUNT=29, RPC_M2S_BUFFER_SIZE=128, RPC_S2M_BUFFER_SIZE=32)
    owned = {'live_sample': 664, 'live_frames': 348, 'live_control': 76,
             'live_split': 112, 'live_hid': 72}
    consumers = ['noah_lighting_live_task', 'noah_lighting_live_handle.constprop.0',
                 'noah_lighting_live_close', 'noah_lighting_live_colour',
                 'noah_rgb_runtime_live_release', 'live_rpc', 'live_receive_main',
                 'live_send_main', 'live_capture', 'noah_lighting_keymap_refresh']
    symbols = [f'20000000 {size:08x} b {name}' for name, size in owned.items()]
    symbols += [f'10000000 00000010 T {name}' for name in consumers]
    # This nm stand-in reads the named artifact: unrelated/partial artifacts do
    # not gain proof merely because they have a plausible target filename.
    nm = work / 'nm'
    nm.write_text('#!/usr/bin/env python3\nimport sys\nprint(open(sys.argv[-1]).read())\n')
    nm.chmod(0o755)
    artifact = work / 'bastardkb_charybdis_4x6_noah.elf'
    flags = work / 'cflags.txt'
    header = work / 'forced.h'

    def probe(changes=None, table=None, version=(15, 2, 0), expected=True):
        values = config | (changes or {})
        header.write_text(''.join(f'#define {k} {v}\n' for k, v in values.items() if v is not None))
        arguments = ['-I', str(work), '-include', str(header), '-DQMK_KEYBOARD_H="keyboard.h"']
        for name, value in zip(('__GNUC__', '__GNUC_MINOR__', '__GNUC_PATCHLEVEL__'), version):
            arguments += [f'-U{name}', f'-D{name}={value}']
        flags.write_text(shlex.join(arguments))
        artifact.write_text('\n'.join(symbols if table is None else table))
        result = subprocess.run(['python3', str(TOOL), str(flags), '--compiler', os.environ.get('CC', 'cc'),
                                 '--lighting-live-elf', str(artifact), '--nm', str(nm)], cwd=work)
        assert (result.returncode == 0) == expected, (changes, table, version, result.returncode)

    probe()
    for key in config:
        probe({key: None}, expected=False)
    for key in ('NOAH_LIGHTING_LIVE_ENABLE', 'NOAH_PD_PROFILE_ENABLE',
                'NOAH_PORTABLE_PROFILE_ENABLE', 'RGB_MATRIX_ENABLE', 'NOAH_LIGHTING_EVENT_CAPTURE_ENABLE'):
        probe({key: 0}, expected=False)
    probe({'NOAH_LIGHTING_TARGET_KEYS_THAT': 0, 'NOAH_LIGHTING_PAINT_WHAT_IT_DOES': 0}, expected=False)
    for key, value in [('MATRIX_ROWS', 8), ('COMBO_BUFFER_LENGTH', 8), ('RGB_MATRIX_LED_COUNT', 60),
                       ('RGB_LEFT_LED_COUNT', 28), ('RPC_M2S_BUFFER_SIZE', 256), ('RPC_S2M_BUFFER_SIZE', 64)]:
        probe({key: value}, expected=False)
    for version in ((14, 2, 0), (15, 1, 0), (15, 2, 1)):
        probe(version=version, expected=False)
    for index in range(len(symbols)):
        probe(table=symbols[:index] + symbols[index + 1:], expected=False)
    for name, size in owned.items():
        probe(table=[s.replace(f'{size:08x} b {name}', f'{size+1:08x} b {name}') for s in symbols], expected=False)
        probe(table=[s.replace(f'b {name}', f'T {name}') for s in symbols], expected=False)
    probe(table=[], expected=False)
    probe(table=['10000000 00000010 T unrelated'], expected=False)
print('live memory proof: forced configuration, compiler, geometry, every owned object and consumer; negatives passed')
