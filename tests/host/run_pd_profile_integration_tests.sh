#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
python3 - "$ROOT" "$BUILD_DIR" <<'PY'
import json, sys
from pathlib import Path
cases = {case['name']: case for case in json.loads(Path(sys.argv[1], 'tests/fixtures/pd_mode_domain_v3.json').read_text())['valid']}
for name in ('presets', 'full'):
    Path(sys.argv[2], name + '.bin').write_bytes(bytes.fromhex(cases[name]['hex']))
PY
for sanitizer in normal sanitized; do
    flags=""
    if [ "$sanitizer" = sanitized ]; then flags="-fsanitize=address,undefined -fno-omit-frame-pointer"; fi
    cc -std=c11 -Wall -Wextra -Werror -pedantic $flags -DNOAH_PD_PROFILE_ENABLE \
        -I"$ROOT/users/noah" \
        "$ROOT/tests/host/pd_profile_integration_test.c" \
        "$ROOT/users/noah/lib/profile/protocol/profile_candidate_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_validator_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_pd_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_combo_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_settings_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/key_behavior_domain_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_domain_registry.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_blob_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
        "$ROOT/users/noah/lib/profile/runtime/effective_pd_runtime.c" \
        -o "$BUILD_DIR/test"
    "$BUILD_DIR/test" "$BUILD_DIR/presets.bin" "$BUILD_DIR/full.bin"
done
arm-none-eabi-gcc -std=c11 -Wall -Wextra -Werror -pedantic -DNOAH_PD_PROFILE_ENABLE \
    -mcpu=cortex-m0plus -mthumb -I"$ROOT/users/noah" \
    -c "$ROOT/users/noah/lib/profile/schema/profile_validator_v1.c" -o "$BUILD_DIR/validator.o"
