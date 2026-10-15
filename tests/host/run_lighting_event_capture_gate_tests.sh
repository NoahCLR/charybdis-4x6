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
cat > "$BUILD_DIR/capability.c" <<'C'
#include "users/noah/lib/profile/schema/profile_lighting_kinds_v1.h"
_Static_assert((NOAH_LIGHTING_COMPILED_FACTS & 0x1ff80000u) == 0u, "disabled consumer advertises no event");
_Static_assert((NOAH_LIGHTING_COMPILED_TIMINGS & 0xcu) == 0u, "disabled consumer advertises no retained timing");
int main(void) { return 0; }
C
cc -std=c11 -Wall -Wextra -Werror -pedantic -DRGB_MATRIX_ENABLE -DNOAH_PORTABLE_PROFILE_ENABLE \
    -I"$ROOT" "$BUILD_DIR/capability.c" -o "$BUILD_DIR/capability"
"$BUILD_DIR/capability"
if cc -std=c11 -Wall -Wextra -Werror -pedantic -DRGB_MATRIX_ENABLE -DNOAH_PORTABLE_PROFILE_ENABLE \
    -DNOAH_LIGHTING_FACT_MACRO_PLAYED=1 -I"$ROOT" -c "$BUILD_DIR/capability.c" -o "$BUILD_DIR/unsupported.o" > "$BUILD_DIR/capability.log" 2>&1; then
    echo 'event module unexpectedly accepted without consumer integration' >&2
    exit 1
fi
rg -q 'retained lighting modules require the linked event consumer' "$BUILD_DIR/capability.log"
printf 'lighting event capture: disabled needs no observer; enabled requires and calls it\n'

cat > "$BUILD_DIR/producer-capabilities.c" <<'C'
#include "users/noah/lib/profile/schema/profile_lighting_kinds_v1.h"
_Static_assert(!!NOAH_LIGHTING_FACT_MODE_STARTED == EXPECT_POINTING, "mode-start capability follows real producer");
_Static_assert(!!NOAH_LIGHTING_FACT_MODE_ENDED == EXPECT_POINTING, "mode-end capability follows real producer");
_Static_assert(!!NOAH_LIGHTING_FACT_COMBO_FIRED == EXPECT_COMBO, "combo capability follows real producer");
int main(void) { return 0; }
C
for pointing in 0 1; do
    for combo in 0 1; do
        flags=""
        [ "$pointing" = 0 ] || flags="$flags -DPOINTING_DEVICE_ENABLE"
        [ "$combo" = 0 ] || flags="$flags -DCOMBO_ENABLE"
        # Schema-only metadata probe; executable observer linkage is checked above.
        # shellcheck disable=SC2086
        cc -std=c11 -Wall -Wextra -Werror -pedantic -DRGB_MATRIX_ENABLE -DNOAH_PORTABLE_PROFILE_ENABLE \
          -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 -DEXPECT_POINTING="$pointing" -DEXPECT_COMBO="$combo" \
          $flags -I"$ROOT" "$BUILD_DIR/producer-capabilities.c" -o "$BUILD_DIR/producer-capabilities"
        "$BUILD_DIR/producer-capabilities"
    done
done
for flag in NOAH_LIGHTING_FACT_MODE_STARTED NOAH_LIGHTING_FACT_MODE_ENDED NOAH_LIGHTING_FACT_COMBO_FIRED; do
    if cc -std=c11 -Wall -Wextra -Werror -pedantic -DRGB_MATRIX_ENABLE -DNOAH_PORTABLE_PROFILE_ENABLE \
      -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 -D"$flag"=1 -DEXPECT_POINTING=0 -DEXPECT_COMBO=0 \
      -I"$ROOT" -c "$BUILD_DIR/producer-capabilities.c" -o "$BUILD_DIR/unsupported-producer.o" > "$BUILD_DIR/producer.log" 2>&1; then
        echo 'event module unexpectedly accepted without its producer' >&2
        exit 1
    fi
    rg -q 'lighting event module requires its actual producer' "$BUILD_DIR/producer.log"
done
printf 'lighting event capabilities: producer feature combinations and unavailable overrides passed\n'
