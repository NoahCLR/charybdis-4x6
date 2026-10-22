#!/bin/sh
set -eu

# Builds, checks, publishes and pins the image every pair is built and every
# host test runs in (D-F05).
#
#   sh tools/make-build-image.sh <tag>
#
# tools/build-image.dockerfile names QMK's official image by digest, gives both
# of its platforms the amd64 variant's ARM target files and adds ripgrep and
# Node, pinned by checksum. This builds both platforms, checks that their target
# files are byte-identical and that the tools run without network, runs the
# host suite in the new image on this machine, builds the pair in it with every
# resource gate, and only then pushes to ghcr.io/noahclr/charybdis-build:<tag>.
# It also builds the pinned image's pair from the same sources and says whether
# the bytes changed. A tag is never reused. It then pins the pushed image by
# digest in tools/build-image and every workflow's image: line; commit those.
# The pairs need BK at the pin (QMK_ROOT, by default ../bastardkb-qmk). Pushing
# needs `docker login ghcr.io` with a token that has write:packages.

REPO_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET_REPO="${NOAH_BUILD_IMAGE_REPO:-ghcr.io/noahclr/charybdis-build}"
# The directories the Dockerfile replaces: only ARM target code and headers.
TARGET_DIRS="arm-none-eabi/lib arm-none-eabi/include arm-none-eabi/sys-include lib/gcc/arm-none-eabi newlib-nano"
PLATFORMS="linux/amd64 linux/arm64"

[ $# -eq 1 ] || { echo "usage: sh tools/make-build-image.sh <tag>" >&2; exit 2; }
tag="$1"
printf '%s\n' "$tag" | grep -Eq '^[A-Za-z0-9_][A-Za-z0-9._-]{0,127}$' || { echo "malformed tag: $tag" >&2; exit 2; }
target="$TARGET_REPO:$tag"

index_digest() {
    docker buildx imagetools inspect "$1" 2>/dev/null | awk '$1 == "Digest:" { print $2; exit }'
}

if [ -n "$(index_digest "$target" || true)" ]; then
    echo "$target already exists; never reuse a tag. Choose a new one." >&2
    exit 1
fi

old="$(sed -n '1p' "$REPO_ROOT/tools/build-image")"
context="$(mktemp -d)"
pairs="$(mktemp -d)"
trap 'rm -rf "$context" "$pairs"' EXIT
docker buildx build --platform "$(echo $PLATFORMS | tr ' ' ',')" --provenance=false --sbom=false \
    -f "$REPO_ROOT/tools/build-image.dockerfile" --tag "$target" --load "$context"

target_files() {
    docker run --rm --platform "$1" --network none "$target" sh -c \
        "cd /opt/qmk && find $TARGET_DIRS -type f | LC_ALL=C sort | xargs sha256sum | sha256sum"
}
amd64="$(target_files linux/amd64)"
arm64="$(target_files linux/arm64)"
if [ -z "$amd64" ] || [ "$amd64" != "$arm64" ]; then
    echo "the two platforms' target files differ (amd64 ${amd64:-<none>}, arm64 ${arm64:-<none>}); nothing was pushed" >&2
    exit 1
fi

for platform in $PLATFORMS; do
    docker run --rm --platform "$platform" --network none "$target" sh -c 'node --version && rg --version' >/dev/null || {
        echo "ripgrep or Node does not run in the $platform image; nothing was pushed" >&2; exit 1; }
done

# The suite runs in this machine's platform of the new image, so a pinned image
# has passed it at least once before anyone else pulls it.
NOAH_BUILD_IMAGE="$target" sh "$REPO_ROOT/tests/host/run_all_host_tests.sh" || {
    echo "the host suite failed in $target; nothing was pushed" >&2; exit 1; }

# The pair builds in the new image and passes every resource gate before
# anything is pushed: a new compiler can move memory and the reviewed stack
# paths. The pair builds from copies in memory, so it needs no lock and writes
# nothing into BK.
pair() { # pair IMAGE RESOURCE_CHECKS OUT
    NOAH_BUILD_IMAGE="$1" NOAH_RESOURCE_CHECKS="$2" BUILD_ROOT="$3" sh "$REPO_ROOT/tools/build-firmware-pair.sh" >&2
}
pair "$target" yes "$pairs/resources" || {
    echo "the pair or a resource gate failed in $target; nothing was pushed" >&2; exit 1; }
pair "$target" no "$pairs/new" && pair "$old" no "$pairs/old" || {
    echo "a pair to compare did not build; nothing was pushed" >&2; exit 1; }
uf2s="$(cd "$pairs/new" && find . -name '*.uf2' | LC_ALL=C sort)"
[ -n "$uf2s" ] || { echo "the pair built no UF2; nothing was pushed" >&2; exit 1; }
same=1
for uf2 in $uf2s; do cmp -s "$pairs/new/$uf2" "$pairs/old/$uf2" || same=0; done
if [ "$same" = 1 ]; then
    echo "The pair is byte-identical in $target and the pinned image." >&2
else
    echo "The pair differs from the pinned image's: the compiler changed. Once it is pinned," >&2
    echo "record the memory effect in docs/architecture/memory-budgets.md (D-F05)." >&2
fi

docker push "$target"
digest="$(index_digest "$target" || true)"
[ -n "$digest" ] || { echo "pushed $target but cannot read its index digest" >&2; exit 1; }
pin="$target@$digest"

printf '%s\n' "$pin" >"$REPO_ROOT/tools/build-image"
for workflow in "$REPO_ROOT"/.github/workflows/*.yml "$REPO_ROOT"/.github/workflows/*.yaml; do
    [ -f "$workflow" ] || continue
    awk -v old="$old" -v new="$pin" '$1 == "image:" && $2 == old { sub(/image:.*/, "image: " new) } { print }' \
        "$workflow" >"$workflow.new"
    mv "$workflow.new" "$workflow"
done
echo "Pinned in tools/build-image and every workflow's image: line; commit them." >&2
echo "$pin"
