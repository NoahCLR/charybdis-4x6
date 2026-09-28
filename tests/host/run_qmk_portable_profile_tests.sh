#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_live_env.sh"
noah_host_export_live_root "$ROOT"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
for variant in normal sanitized; do
    flags=""
    if [ "$variant" = sanitized ]; then flags="-fsanitize=address,undefined -fno-omit-frame-pointer"; fi
    cc -std=c11 -Wall -Wextra -Werror $flags -DNOAH_PORTABLE_PROFILE_ENABLE -DNOAH_PD_PROFILE_ENABLE -DQMK_KEYBOARD_H='"portable_profile_keyboard.h"' \
        -I"$ROOT/tests/host/include/portable_profile" -I"$ROOT/tests/host/include" -I"$ROOT" -I"$ROOT/users/noah" \
        "$ROOT/tests/host/qmk_portable_profile_test.c" \
        "$ROOT/users/noah/lib/compat/qmk_portable_profile.c" \
        "$ROOT/users/noah/lib/profile/runtime/effective_settings_runtime.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
        -o "$BUILD_DIR/test"
    "$BUILD_DIR/test" "$BUILD_DIR/responses.fixture" "$BUILD_DIR/named.fixture"
    # The app reads the firmware-produced pages and finds the macro's name,
    # and with nothing stored, the keymap's layer and macro names.
    node - "$ROOT" "$BUILD_DIR/responses.fixture" "$BUILD_DIR/named.fixture" <<'JS'
const assert = require("node:assert/strict"), fs = require("node:fs");
const {readSettings} = require(process.env.CHARYBDIS_LIVE_ROOT + "/core/protocol/portable-profile-v1");
const {decodeSettings, macroNamesOf} = require(process.env.CHARYBDIS_LIVE_ROOT + "/core/schema/settings-domain-v1");
function read(path) {
    const fixture = fs.readFileSync(path), pages = fixture.length / 32;
    let id = 0;
    return readSettings({request: async request => {
        assert.ok(request[4] < pages);
        const response = Buffer.from(fixture.subarray(request[4] * 32, request[4] * 32 + 32));
        request.copy(response, 0, 0, 5);
        return response;
    }}, {next: () => ++id}).then(decodeSettings);
}
(async () => {
    const stored = macroNamesOf(await read(process.argv[3]));
    assert.equal(stored[5], "Screenshot");
    assert.equal(stored.filter(Boolean).length, 1);
    const authored = await read(process.argv[4]);
    assert.deepEqual(macroNamesOf(authored).filter(Boolean), ["Drag Screenshot", "Twenty characters!!!"]);
    assert.deepEqual([authored.names[0], authored.names[1], authored.names[7]], ["Base", "", "Twenty-three bytes long"]);
    console.log("app reads the firmware's streamed settings and its macro and layer names");
})().catch(error => {console.error(error); process.exitCode = 1;});
JS
done
