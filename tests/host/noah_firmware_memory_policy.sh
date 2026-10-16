# Select an artifact policy from its recorded flags, never its target filename.
# Argument4 additionally requires complete linked live state/consumer and compiler proof.
# Argument3 requires the compiler's separate GCC/geometry assertion to pass.
noah_firmware_static_ram_policy() {
    python3 - "$1" "${2:-no}" "${3:-no}" "${4:-no}" <<'PY'
import pathlib
import re
import shlex
import sys

path = pathlib.Path(sys.argv[1])
if not path.is_file():
    print(57344)
    raise SystemExit
# Follow command-line order, including repeated definitions and undefinitions.
# Only integer constants establish an enabled configuration; an unknown macro
# expression never grants a higher regression threshold.
definitions = {}
args = iter(shlex.split(path.read_text()))
for arg in args:
    if arg in ('-D', '-U'):
        arg += next(args, '')
    if arg.startswith('-U'):
        definitions[arg[2:]] = False
    elif arg.startswith('-D'):
        key, sep, value = arg[2:].partition('=')
        value = value.strip('()') if sep else '1'
        match = re.fullmatch(r'([+-]?(?:0[xX][0-9a-fA-F]+|[0-9]+))[uUlL]*', value)
        definitions[key] = bool(match and int(match[1], 16 if match[1].lstrip('+-').lower().startswith('0x') else 10))

def enabled(key, default=False):
    return definitions.get(key, default)

if not enabled('NOAH_PD_PROFILE_ENABLE'):
    policy = 57344
elif (sys.argv[2] == 'yes' and enabled('NOAH_PORTABLE_PROFILE_ENABLE')
      and enabled('RGB_MATRIX_ENABLE')
      and (enabled('NOAH_LIGHTING_TARGET_KEYS_THAT', True)
           or enabled('NOAH_LIGHTING_PAINT_WHAT_IT_DOES', True))):
    policy = 247808 if sys.argv[3] == 'yes' and enabled('NOAH_LIGHTING_EVENT_CAPTURE_ENABLE') else 229376
    if policy == 247808 and sys.argv[4] == 'yes' and enabled('NOAH_LIGHTING_LIVE_ENABLE'):
        policy = 249344
else:
    policy = 226304
print(policy)
PY
}
