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
case "${NOAH_SPLIT_DIAGNOSTICS:-}" in
    ""|no) ;;
    yes) TRANSPORT_ARGS="$TRANSPORT_ARGS -e NOAH_SPLIT_DIAGNOSTICS=yes"; SUFFIX="${SUFFIX}_diagnostic" ;;
    *) echo "NOAH_SPLIT_DIAGNOSTICS must be yes or no" >&2; exit 1 ;;
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

build_half() {
    role="$1"
    half="$2"
    name="$3"

    echo "Building $half half ($role)..."
    # Object files are shared between the two compiles, so clear them or the
    # second half links the first half's translation units.
    rm -f "$QMK_ROOT/.build/obj_bastardkb_charybdis_4x6_noah"/*.o 2>/dev/null || true
    ( cd "$QMK_ROOT" && qmk compile -kb bastardkb/charybdis/4x6 -km noah \
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
