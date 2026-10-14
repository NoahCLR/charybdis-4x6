#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
cat > "$BUILD_DIR/probe.c" <<'C'
#include "users/noah/lib/rgb/lighting/lighting_events.h"
#include <assert.h>
#if defined(RECORD_OBSERVER)
static unsigned observations;
void noah_lighting_event_observe(const noah_lighting_event_t *event) {
    assert(event->kind == NOAH_LIGHTING_EVENT_MACRO_PLAYED && event->thing == 127u);
    ++observations;
}
#endif
int main(void) {
    noah_lighting_event_emit(&(noah_lighting_event_t){.kind = NOAH_LIGHTING_EVENT_MACRO_PLAYED, .thing = 127u});
#if defined(RECORD_OBSERVER)
    assert(noah_lighting_events_observed() && observations == 1u);
#else
    assert(!noah_lighting_events_observed());
#endif
    return 0;
}
C
build_probe() {
    cc -std=c11 -Wall -Wextra -Werror -pedantic \
      -DQMK_KEYBOARD_H='"qmk_stub.h"' \
      -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
      "$@" "$BUILD_DIR/probe.c" -o "$BUILD_DIR/probe"
}
build_probe
"$BUILD_DIR/probe"
build_probe -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=0
"$BUILD_DIR/probe"
if build_probe -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 > "$BUILD_DIR/missing.log" 2>&1; then
    echo 'enabled event capture unexpectedly linked without its observer' >&2
    exit 1
fi
if ! rg -q 'noah_lighting_event_observe' "$BUILD_DIR/missing.log"; then
    cat "$BUILD_DIR/missing.log" >&2
    exit 1
fi
build_probe -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 -DRECORD_OBSERVER=1
"$BUILD_DIR/probe"
printf 'lighting event capture: disabled needs no observer; enabled requires and calls it\n'
