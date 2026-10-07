#!/bin/sh
set -eu

# Copies a build image, byte for byte, into our own registry package.
#
#   sh tools/copy-build-image.sh <source>@sha256:<index digest> <tag>
#
# The pair is built in the image tools/build-image names (D-F05). That image is
# QMK's official one, copied unchanged to ghcr.io/noahclr/charybdis-build, so
# nobody upstream can move or delete what our releases were built with. The copy
# keeps the multi-arch index digest, which proves it is the official image.
#
# The source must be named by its digest, never by a tag alone, and an existing
# tag is never pointed at a different image. On success this prints the
# reference to put in tools/build-image and every workflow's image: line.
# Pushing needs `docker login ghcr.io` with a token that has write:packages.

TARGET_REPO="${NOAH_BUILD_IMAGE_REPO:-ghcr.io/noahclr/charybdis-build}"

[ $# -eq 2 ] || { echo "usage: sh tools/copy-build-image.sh <source>@sha256:<digest> <tag>" >&2; exit 2; }
source="$1"
tag="$2"
case "$source" in
    *@sha256:*) ;;
    *) echo "name the source by its index digest (<image>@sha256:...), not a tag: a tag can move" >&2; exit 2 ;;
esac
digest="sha256:${source##*@sha256:}"
printf '%s\n' "$digest" | grep -Eq '^sha256:[0-9a-f]{64}$' || { echo "malformed digest in $source" >&2; exit 2; }
printf '%s\n' "$tag" | grep -Eq '^[A-Za-z0-9_][A-Za-z0-9._-]{0,127}$' || { echo "malformed tag: $tag" >&2; exit 2; }
target="$TARGET_REPO:$tag"

index_digest() {
    docker buildx imagetools inspect "$1" 2>/dev/null | awk '$1 == "Digest:" { print $2; exit }'
}

existing="$(index_digest "$target" || true)"
if [ -n "$existing" ] && [ "$existing" != "$digest" ]; then
    echo "$target already names $existing; never move a tag. Choose a new tag." >&2
    exit 1
fi
if [ -z "$existing" ]; then
    docker buildx imagetools create --tag "$target" "$source"
fi
copied="$(index_digest "$target" || true)"
if [ "$copied" != "$digest" ]; then
    echo "$target has digest ${copied:-<none>}, not $digest: the copy is not the source image" >&2
    exit 1
fi
echo "$target@$digest"
