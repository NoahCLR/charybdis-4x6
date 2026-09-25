#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
for variant in normal features sanitized; do
    flags=""
    if [ "$variant" != normal ]; then flags="-DMAGIC_ENABLE -DNKRO_ENABLE -DAUTOCORRECT_ENABLE -DENABLE_RGB_MATRIX_BREATHING -DENABLE_RGB_MATRIX_CYCLE_ALL"; fi
    if [ "$variant" = sanitized ]; then flags="$flags -fsanitize=address,undefined -fno-omit-frame-pointer"; fi
    cc -std=c11 -Wall -Wextra -Werror $flags -DNOAH_PORTABLE_PROFILE_ENABLE -DQMK_KEYBOARD_H='"keycode_config.h"' \
        -I"$ROOT/tests/host/include/portable_editor" -I"$ROOT/tests/host/include" -I"$ROOT" \
        -I"$QMK_ROOT/quantum/rgb_matrix/animations" \
        "$ROOT/tests/host/qmk_portable_editor_test.c" "$ROOT/users/noah/lib/compat/qmk_portable_editor.c" -o "$BUILD_DIR/test"
    "$BUILD_DIR/test" "$BUILD_DIR/options.fixture"
    node - "$ROOT" "$BUILD_DIR/options.fixture" <<'JS'
const assert = require("node:assert/strict"), fs = require("node:fs");
const {readKeyboardOptions} = require(process.argv[2] + "/tools/charybdis-live/core/protocol/keyboard-options-v1");
const fixture = fs.readFileSync(process.argv[3]), metadata = fixture.subarray(0,9), data = fixture.subarray(9);
let id = 0;
readKeyboardOptions({request: async request => {
    const payload = request[4] === 2 ? metadata : data.subarray((request[4]-3)*25,(request[4]-2)*25);
    const response = Buffer.alloc(32); request.copy(response,0,0,5); response[6] = payload.length; payload.copy(response,7); return response;
}}, {next: () => ++id}).then(value => {
    assert.equal(value.effects.length, metadata[4]); assert.equal(value.effects[0].name, "SOLID_COLOR");
    assert.equal(value.keymapMasks[2], 4); assert.equal(value.ledFlags,5);
    console.log("app decodes firmware-produced keyboard option pages");
}).catch(error => {console.error(error); process.exitCode = 1;});
JS
done
