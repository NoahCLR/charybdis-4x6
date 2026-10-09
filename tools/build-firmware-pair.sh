#!/bin/sh
set -eu

# Builds the flashable left/right pair.
#
# This exists because the pair is the firmware you actually flash, and it is
# not what a plain `qmk compile` produces. The live-profile owner needs
# NOAH_PHYSICAL_HALF, since durable profile origin identity is side-specific,
# so a build without it silently omits the owner and the keyboard has no
# committed profile to read. See docs/LIVE_EDIT_APP_DIRECTION.md, D-L08.
#
# NOAH_PHYSICAL_HALF is deliberately not derived from FORCE_MASTER/FORCE_SLAVE.
# Those select the transport role, which can swap at runtime; the physical half
# is durable identity burned into the artifact. Keeping them separate is the
# contract, so both are passed explicitly here.
#
#   sh tools/build-firmware-pair.sh [--no-owner]
#
# --no-owner builds the comparison pair without the live-profile owner, for
# A/B against ordinary behaviour.
# The split link always runs at QMK's default speed; the faster one garbled
# split messages (D-L43), so NOAH_SPLIT_BAUD is refused rather than ignored.
#
# The pair is always compiled in the image tools/build-image names, the one CI
# builds releases in, so the firmware flashed here and the firmware users
# download come from the same compiler. Outside that image this script runs
# itself inside it with Docker; it never falls back to another compiler. The
# build is reproducible: QMK's version stamps are fixed (SKIP_VERSION) and
# source paths are mapped, so one commit and BK pin give the same bytes on any
# machine. A note beside the pair records the inputs, compiler and hashes.

REPO_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
QMK_ROOT="${QMK_ROOT:-$(CDPATH= cd -- "$REPO_ROOT/../bastardkb-qmk" && pwd)}"
BUILD_ROOT="${BUILD_ROOT:-$(CDPATH= cd -- "$REPO_ROOT/.." && pwd)/builds}"
ARTIFACT="$QMK_ROOT/bastardkb_charybdis_4x6_noah.uf2"
IMAGE="$(sed -n '1p' "$REPO_ROOT/tools/build-image")"

# The pair is firmware and BK compiled together, so it is built only against
# the BK commit qmk-pin.json names, from a clean checkout. verify and CI check
# BK out at the pin themselves. NOAH_ALLOW_UNPINNED_QMK=1 builds against the BK
# checkout as it is: VS Code's build tasks (development pairs), trials, and BK
# changes tested against current firmware. Such a pair is never released, and
# its note says which BK it used and that it is not the pin.
PIN="$(python3 -c 'import json, sys; print(json.load(open(sys.argv[1]))["commit"])' "$REPO_ROOT/qmk-pin.json")" || {
    echo "qmk-pin.json must name the BK commit this firmware builds with (tools/pin-qmk.sh)" >&2
    exit 1
}
QMK_HEAD="$(git -C "$QMK_ROOT" rev-parse HEAD 2>/dev/null || echo "not a Git checkout")"
QMK_DIRTY="$(git -C "$QMK_ROOT" status --porcelain --ignore-submodules=all 2>/dev/null || true)"
if [ "$QMK_HEAD" != "$PIN" ] || [ -n "$QMK_DIRTY" ]; then
    if [ "${NOAH_ALLOW_UNPINNED_QMK:-}" = 1 ]; then
        echo "Not the pin: building against BK $QMK_HEAD${QMK_DIRTY:+ (modified)}, not the pinned $PIN." >&2
        echo "A development pair; verify, CI and releases build at the pin." >&2
        UNPINNED=" (not the pin $PIN)"
    else
        echo "BK at $QMK_ROOT is $QMK_HEAD${QMK_DIRTY:+ with local changes}, but qmk-pin.json pins $PIN." >&2
        echo "Check BK out at the pin (verify does this itself), or re-pin with tools/pin-qmk.sh." >&2
        exit 1
    fi
fi

# Outside the build image (CI jobs run inside it), run this script in it.
# Every checkout and Git directory the build reads is mounted at its own path,
# so worktree and cached-pin .git pointers still resolve. NOAH_IN_BUILD_IMAGE
# is set to 1 inside; 0 forces the Docker path (the host tests use it).
case "${NOAH_IN_BUILD_IMAGE:-}" in
    1) in_image=1 ;;
    0) in_image=0 ;;
    *) in_image=0; [ ! -f /.dockerenv ] || in_image=1 ;;
esac
if [ "$in_image" = 0 ]; then
    command -v docker >/dev/null 2>&1 || {
        echo "Docker is required: the pair is built in $IMAGE, the image CI uses." >&2; exit 1; }
    docker info >/dev/null 2>&1 || {
        echo "Docker is not running; start Docker Desktop. The pair is built in $IMAGE, the image CI uses." >&2; exit 1; }
    mkdir -p "$BUILD_ROOT"
    BUILD_ROOT="$(CDPATH= cd -- "$BUILD_ROOT" && pwd)"
    first="${1:-}"
    # No network: everything the build reads is in the image and the mounted
    # checkouts, so an attempt to download anything fails instead of changing
    # the pair. Docker fetches the pinned image itself before the container runs.
    set -- run --rm --network none --user "$(id -u):$(id -g)" -e HOME=/tmp -e NOAH_IN_BUILD_IMAGE=1 \
        -e "QMK_ROOT=$QMK_ROOT" -e "BUILD_ROOT=$BUILD_ROOT" \
        -e GIT_CONFIG_COUNT=1 -e GIT_CONFIG_KEY_0=safe.directory -e "GIT_CONFIG_VALUE_0=*"
    for var in $(env | sed -n 's/^\(NOAH_[A-Z0-9_]*\)=.*/\1/p'); do
        [ "$var" = NOAH_IN_BUILD_IMAGE ] || set -- "$@" -e "$var"
    done
    mounted=" "
    for dir in "$REPO_ROOT" "$(git -C "$REPO_ROOT" rev-parse --path-format=absolute --git-common-dir)" \
               "$QMK_ROOT" "$(git -C "$QMK_ROOT" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || echo "$QMK_ROOT")" \
               "$BUILD_ROOT"; do
        case "$mounted" in *" $dir "*) continue ;; esac
        mounted="$mounted$dir "
        set -- "$@" -v "$dir:$dir"
    done
    set -- "$@" -w "$REPO_ROOT" "$IMAGE" sh "$REPO_ROOT/tools/build-firmware-pair.sh"
    [ -z "$first" ] || set -- "$@" "$first"
    echo "Building in $IMAGE..."
    exec docker "$@"
fi

OWNER_ARGS=""
SUFFIX=""
TRANSPORT_ARGS=""
if [ "${1:-}" = "--no-owner" ]; then
    OWNER_ARGS="-e NOAH_LIVE_PROFILE_OWNER=no -e NOAH_LIVE_PROFILE_MUTATION=no"
    SUFFIX="_no_owner"
elif [ $# -gt 0 ]; then
    echo "Unknown argument: $1" >&2
    echo "Usage: sh tools/build-firmware-pair.sh [--no-owner]" >&2
    exit 1
fi

if [ -n "${NOAH_SPLIT_BAUD:-}" ]; then
    echo "NOAH_SPLIT_BAUD was removed: the split link stays at QMK's default speed (D-L43)" >&2
    exit 1
fi

# Activity coalescing is the default; =no builds the comparison pair.
case "${NOAH_SPLIT_ACTIVITY_COALESCE:-}" in
    ""|yes) ;;
    no) TRANSPORT_ARGS="$TRANSPORT_ARGS -e NOAH_SPLIT_ACTIVITY_COALESCE=no"; SUFFIX="${SUFFIX}_no_activity" ;;
    *) echo "NOAH_SPLIT_ACTIVITY_COALESCE must be yes or no" >&2; exit 1 ;;
esac
# The split frame CRC is the default; =no builds the comparison pair. Both
# halves must match: a mixed pair fails at the handshake.
case "${NOAH_SPLIT_CRC:-}" in
    ""|yes) ;;
    no) TRANSPORT_ARGS="$TRANSPORT_ARGS -e NOAH_SPLIT_CRC=no"; SUFFIX="${SUFFIX}_no_crc" ;;
    *) echo "NOAH_SPLIT_CRC must be yes or no" >&2; exit 1 ;;
esac
case "${NOAH_SPLIT_DIAGNOSTICS:-}" in
    ""|no) ;;
    yes) TRANSPORT_ARGS="$TRANSPORT_ARGS -e NOAH_SPLIT_DIAGNOSTICS=yes"; SUFFIX="${SUFFIX}_diagnostic" ;;
    *) echo "NOAH_SPLIT_DIAGNOSTICS must be yes or no" >&2; exit 1 ;;
esac
# The pointing-cadence recorder, read with the split recorder by
# tools/capture-split-diagnostics.cjs.
case "${NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS:-}" in
    ""|no) ;;
    yes) TRANSPORT_ARGS="$TRANSPORT_ARGS -e NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS=yes"; SUFFIX="${SUFFIX}_cadence" ;;
    *) echo "NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS must be yes or no" >&2; exit 1 ;;
esac

# Resource acceptance uses an instrumented pair in the same pinned compiler.
# Keep its artifacts separate from the ordinary flashable pair.
RESOURCE_ARGS=""
case "${NOAH_RESOURCE_CHECKS:-}" in
    ""|no) ;;
    yes)
        [ -z "$OWNER_ARGS" ] || { echo "resource checks require the profile owner" >&2; exit 1; }
        RESOURCE_ARGS="-e NOAH_STACK_BUDGET_ENABLE=yes"
        SUFFIX="${SUFFIX}_resources"
        ;;
    *) echo "NOAH_RESOURCE_CHECKS must be yes or no" >&2; exit 1 ;;
esac

branch=$(git -C "$REPO_ROOT" rev-parse --abbrev-ref HEAD)
destdir="$BUILD_ROOT/$branch"
mkdir -p "$destdir"

# Keep the alias's numbering so builds stay in flash order.
n=1
while [ -f "$destdir/${n}_charybdis_right${SUFFIX}.uf2" ]; do
    n=$((n + 1))
done

export QMK_USERSPACE="$REPO_ROOT"
export QMK_HOME="$QMK_ROOT"
KEYMAP_ROOT="$REPO_ROOT/keyboards/bastardkb/charybdis/4x6/keymaps/noah"
# QMK CLI 1.1.8 prefers saved overlay_dir/qmk_home over environment variables.
# Isolate this invocation and every QMK generator make calls from that config;
# otherwise a task worktree can silently build the main checkout instead.
# Explicit keymap paths also override old keymap symlinks inside the QMK tree.

build_half() {
    role="$1"
    half="$2"
    name="$3"

    echo "Building $half half ($role)..."
    # The object directory is shared between the two compiles and between
    # worktrees, so clear all of it: stale objects would link the other half's
    # translation units, and stale make dependency files name sources in
    # whichever worktree built last, which fails once that worktree is removed.
    rm -rf "$QMK_ROOT/.build/obj_bastardkb_charybdis_4x6_noah"
    # A successful command must produce this half's UF2, not leave the previous
    # half's artifact at the shared QMK output path.
    rm -f "$ARTIFACT"
    # -j 0 compiles on every core; the pair's bytes do not depend on job order.
    ( cd "$QMK_ROOT" && qmk --config-file /dev/null compile -j 0 -kb bastardkb/charybdis/4x6 -km noah \
        -e "QMK_BIN=qmk --config-file /dev/null" \
        -e "MAIN_KEYMAP_PATH_1=$KEYMAP_ROOT" -e "MAIN_KEYMAP_PATH_2=$KEYMAP_ROOT" \
        -e "MAIN_KEYMAP_PATH_3=$KEYMAP_ROOT" -e "MAIN_KEYMAP_PATH_4=$KEYMAP_ROOT" \
        -e "MAIN_KEYMAP_PATH_5=$KEYMAP_ROOT" \
        -e SKIP_VERSION=yes \
        -e "EXTRAFLAGS=-ffile-prefix-map=$REPO_ROOT=/userspace -ffile-prefix-map=$QMK_ROOT=/qmk" \
        -e "$role=yes" -e "NOAH_PHYSICAL_HALF=$half" $OWNER_ARGS $TRANSPORT_ARGS $RESOURCE_ARGS )
    if [ ! -f "$ARTIFACT" ]; then
        echo "Expected firmware not found: $ARTIFACT" >&2
        exit 1
    fi
    cp "$ARTIFACT" "$destdir/$name.uf2"
    echo "  -> $destdir/$name.uf2"
    if [ -n "$RESOURCE_ARGS" ]; then
        target=bastardkb_charybdis_4x6_noah
        # Preserve this half before the next compile replaces QMK's output.
        for extension in elf map; do
            cp "$QMK_ROOT/.build/$target.$extension" "$destdir/$name.$extension"
        done
        for flags in cflags ldflags; do
            cp "$QMK_ROOT/.build/obj_$target/$flags.txt" "$destdir/$name.$flags.txt"
        done
        if ! sh "$REPO_ROOT/tests/host/run_firmware_memory_budget_checks.sh" >"$destdir/$name.memory.txt" 2>&1; then
            cat "$destdir/$name.memory.txt"; return 1
        fi
        cat "$destdir/$name.memory.txt"
        for manifest in firmware_stack_budget firmware_stack_budget_live_profile_owner; do
            if ! python3 "$REPO_ROOT/tools/check_firmware_stack_budget.py" \
                --manifest "$REPO_ROOT/tools/$manifest.json" \
                --elf "$destdir/$name.elf" --map "$destdir/$name.map" \
                >"$destdir/$name.$manifest.txt" 2>&1; then
                cat "$destdir/$name.$manifest.txt"; return 1
            fi
            cat "$destdir/$name.$manifest.txt"
        done
    fi
}

build_half FORCE_MASTER right "${n}_charybdis_right${SUFFIX}"
build_half FORCE_SLAVE left "${n}_charybdis_left${SUFFIX}"

# What the pair was built from and with, and its hashes: a published pair can
# be matched to a local one byte for byte.
uncommitted() { [ -z "$(git -C "$1" status --porcelain --ignore-submodules=all 2>/dev/null)" ] || echo " +uncommitted"; }
{
    echo "firmware $(git -C "$REPO_ROOT" rev-parse HEAD)$(uncommitted "$REPO_ROOT")"
    echo "bk $QMK_HEAD$(uncommitted "$QMK_ROOT")${UNPINNED:-}"
    echo "image $IMAGE"
    echo "compiler $(arm-none-eabi-gcc --version 2>/dev/null | head -n 1 || echo unknown)"
    for half in right left; do
        file="${n}_charybdis_${half}${SUFFIX}.uf2"
        echo "sha256 $(python3 -c 'import hashlib, sys; print(hashlib.sha256(open(sys.argv[1], "rb").read()).hexdigest())' "$destdir/$file") $file"
    done
} > "$destdir/${n}_charybdis${SUFFIX}.build.txt"

echo
if [ -n "$OWNER_ARGS" ]; then
    echo "Built the 32-PD-slot factory-only comparison pair WITHOUT the live-profile owner."
else
    echo "Built the 32-PD-slot pair."
fi
echo "Flash the right half to the master side and the left half to the slave side."
