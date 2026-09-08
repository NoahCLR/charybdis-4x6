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
#   sh tools/build-firmware-pair.sh [--no-owner|--snapshot-bridge]
#
# --no-owner builds the comparison pair without the live-profile owner, for
# A/B against ordinary behaviour while the pointing cadence regression in
# docs/architecture/pointing-cadence-known-issue.md is unresolved.

REPO_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
QMK_ROOT="${QMK_ROOT:-$(CDPATH= cd -- "$REPO_ROOT/../bastardkb-qmk" && pwd)}"
BUILD_ROOT="${BUILD_ROOT:-$(CDPATH= cd -- "$REPO_ROOT/.." && pwd)/builds}"
ARTIFACT="$QMK_ROOT/bastardkb_charybdis_4x6_noah.uf2"

OWNER_ARGS=""
SUFFIX=""
if [ "${1:-}" = "--no-owner" ]; then
    OWNER_ARGS="-e NOAH_LIVE_PROFILE_OWNER=no -e NOAH_LIVE_PROFILE_MUTATION=no"
    SUFFIX="_no_owner"
elif [ "${1:-}" = "--snapshot-bridge" ]; then
    OWNER_ARGS="-e NOAH_LEGACY_SNAPSHOT_BRIDGE=yes"
    SUFFIX="_snapshot_bridge"
elif [ $# -gt 0 ]; then
    echo "Unknown argument: $1" >&2
    echo "Usage: sh tools/build-firmware-pair.sh [--no-owner|--snapshot-bridge]" >&2
    exit 1
fi

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
        -e "$role=yes" -e "NOAH_PHYSICAL_HALF=$half" $OWNER_ARGS )
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
if [ "$SUFFIX" = "_snapshot_bridge" ]; then
    echo "Built the five-layer backup bridge. Export a complete profile before moving to eight layers."
elif [ -n "$SUFFIX" ]; then
    echo "Built the comparison pair WITHOUT the live-profile owner."
else
    echo "Built the flashable pair with the live-profile owner enabled."
fi
echo "Flash the right half to the master side and the left half to the slave side."
