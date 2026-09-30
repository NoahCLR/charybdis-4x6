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

REPO_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
QMK_ROOT="${QMK_ROOT:-$(CDPATH= cd -- "$REPO_ROOT/../bastardkb-qmk" && pwd)}"
BUILD_ROOT="${BUILD_ROOT:-$(CDPATH= cd -- "$REPO_ROOT/.." && pwd)/builds}"
ARTIFACT="$QMK_ROOT/bastardkb_charybdis_4x6_noah.uf2"

# The pair is firmware and BK compiled together, so it is built only against
# the BK commit qmk-pin.json names, from a clean checkout. verify and CI check
# BK out at the pin themselves. NOAH_ALLOW_UNPINNED_QMK=1 is for trials and for
# testing a BK change against current firmware; such a pair is never released.
PIN="$(python3 -c 'import json, sys; print(json.load(open(sys.argv[1]))["commit"])' "$REPO_ROOT/qmk-pin.json")" || {
    echo "qmk-pin.json must name the BK commit this firmware builds with (tools/pin-qmk.sh)" >&2
    exit 1
}
QMK_HEAD="$(git -C "$QMK_ROOT" rev-parse HEAD 2>/dev/null || echo "not a Git checkout")"
QMK_DIRTY="$(git -C "$QMK_ROOT" status --porcelain --ignore-submodules=all 2>/dev/null || true)"
if [ "$QMK_HEAD" != "$PIN" ] || [ -n "$QMK_DIRTY" ]; then
    if [ "${NOAH_ALLOW_UNPINNED_QMK:-}" = 1 ]; then
        echo "TRIAL: building against BK $QMK_HEAD${QMK_DIRTY:+ (modified)}, not the pinned $PIN" >&2
    else
        echo "BK at $QMK_ROOT is $QMK_HEAD${QMK_DIRTY:+ with local changes}, but qmk-pin.json pins $PIN." >&2
        echo "Check BK out at the pin (verify does this itself), or re-pin with tools/pin-qmk.sh." >&2
        exit 1
    fi
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
    ( cd "$QMK_ROOT" && qmk --config-file /dev/null compile -kb bastardkb/charybdis/4x6 -km noah \
        -e "QMK_BIN=qmk --config-file /dev/null" \
        -e "MAIN_KEYMAP_PATH_1=$KEYMAP_ROOT" -e "MAIN_KEYMAP_PATH_2=$KEYMAP_ROOT" \
        -e "MAIN_KEYMAP_PATH_3=$KEYMAP_ROOT" -e "MAIN_KEYMAP_PATH_4=$KEYMAP_ROOT" \
        -e "MAIN_KEYMAP_PATH_5=$KEYMAP_ROOT" \
        -e "$role=yes" -e "NOAH_PHYSICAL_HALF=$half" $OWNER_ARGS $TRANSPORT_ARGS )
    if [ ! -f "$ARTIFACT" ]; then
        echo "Expected firmware not found: $ARTIFACT" >&2
        exit 1
    fi
    cp "$ARTIFACT" "$destdir/$name.uf2"
    echo "  -> $destdir/$name.uf2"
}

build_half FORCE_MASTER right "${n}_charybdis_right${SUFFIX}"
build_half FORCE_SLAVE left "${n}_charybdis_left${SUFFIX}"

echo
if [ -n "$OWNER_ARGS" ]; then
    echo "Built the eight-PD-slot factory-only comparison pair WITHOUT the live-profile owner."
else
    echo "Built the eight-PD-slot pair."
fi
echo "Flash the right half to the master side and the left half to the slave side."
