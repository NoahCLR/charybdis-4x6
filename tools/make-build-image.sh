#!/bin/sh
set -eu

# Builds and publishes the image every pair is built in (D-F05).
#
#   sh tools/make-build-image.sh <tag>
#
# tools/build-image.dockerfile names QMK's official image by digest and gives
# both of its platforms the amd64 variant's ARM target files, so a Mac building
# natively and CI build identical pairs. This builds both platforms, checks that
# their target files are byte-identical, and only then pushes to
# ghcr.io/noahclr/charybdis-build:<tag>. A tag is never reused. On success it
# prints the reference to put in tools/build-image and every workflow's image:
# line. Pushing needs `docker login ghcr.io` with a token that has
# write:packages.

REPO_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TARGET_REPO="${NOAH_BUILD_IMAGE_REPO:-ghcr.io/noahclr/charybdis-build}"
# The directories the Dockerfile replaces: only ARM target code and headers.
TARGET_DIRS="arm-none-eabi/lib arm-none-eabi/include arm-none-eabi/sys-include lib/gcc/arm-none-eabi newlib-nano"

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

context="$(mktemp -d)"
trap 'rm -rf "$context"' EXIT
docker buildx build --platform linux/amd64,linux/arm64 --provenance=false --sbom=false \
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

docker push "$target"
digest="$(index_digest "$target" || true)"
[ -n "$digest" ] || { echo "pushed $target but cannot read its index digest" >&2; exit 1; }
echo "$target@$digest"
