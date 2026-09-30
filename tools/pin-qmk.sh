#!/bin/sh
set -eu

# Pin the BK commit this firmware is built with (qmk-pin.json).
#
#   sh tools/pin-qmk.sh [--unpublished] [REV]
#
# REV defaults to the published BK dev branch, origin/noah-userspace-contracts-dev
# in the BK checkout (QMK_ROOT, or the bastardkb-qmk checkout beside this
# repository's main checkout). A commit that is not on the published dev branch
# is refused unless --unpublished: then push BK before this firmware (its push
# hook enforces that). Prints what changed in BK since the previous pin.
# Commit the new qmk-pin.json on a task branch; verify and CI build against it.

REPO_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
PUBLISHED=refs/remotes/origin/noah-userspace-contracts-dev
unpublished=0 rev=""
while [ $# -gt 0 ]; do
    case "$1" in
        --unpublished) unpublished=1; shift ;;
        -*) echo "Usage: sh tools/pin-qmk.sh [--unpublished] [REV]" >&2; exit 1 ;;
        *) [ -z "$rev" ] || { echo "Only one REV" >&2; exit 1; }; rev="$1"; shift ;;
    esac
done

if [ -z "${QMK_ROOT:-}" ]; then
    main="$(dirname -- "$(git -C "$REPO_ROOT" rev-parse --path-format=absolute --git-common-dir)")"
    QMK_ROOT="$(dirname -- "$main")/bastardkb-qmk"
fi
git -C "$QMK_ROOT" rev-parse --git-dir >/dev/null 2>&1 || {
    echo "No BK checkout at $QMK_ROOT; set QMK_ROOT" >&2
    exit 1
}

new="$(git -C "$QMK_ROOT" rev-parse --verify "${rev:-$PUBLISHED}^{commit}")"
if ! git -C "$QMK_ROOT" merge-base --is-ancestor "$new" "$PUBLISHED" 2>/dev/null; then
    if [ "$unpublished" = 1 ]; then
        echo "Note: $new is not on published noah-userspace-contracts-dev yet; push BK before this firmware." >&2
    else
        echo "$new is not on published noah-userspace-contracts-dev (fetch BK, or push it first)." >&2
        echo "Use --unpublished to pin it locally anyway." >&2
        exit 1
    fi
fi

old="$(python3 -c 'import json, sys; print(json.load(open(sys.argv[1]))["commit"])' "$REPO_ROOT/qmk-pin.json" 2>/dev/null || true)"
python3 - "$REPO_ROOT/qmk-pin.json" "$new" <<'PY'
import json, sys
path, commit = sys.argv[1:]
pin = {"repository": "https://github.com/NoahCLR/bastardkb-qmk", "branch": "noah-userspace-contracts-dev", "commit": commit}
with open(path, "w") as f:
    f.write(json.dumps(pin, indent=2) + "\n")
PY

echo "qmk-pin.json: ${old:-none} -> $new"
if [ -z "$old" ] || [ "$old" = "$new" ]; then
    exit 0
fi
if ! git -C "$QMK_ROOT" merge-base --is-ancestor "$old" "$new" 2>/dev/null; then
    echo "Warning: the new pin does not contain the old one (a move backwards or sideways)." >&2
fi
echo
echo "BK commits since the previous pin:"
git -C "$QMK_ROOT" log --oneline --no-merges "$old..$new" | sed 's/^/  /'
area() { # area LABEL PATH...
    label="$1"; shift
    stat="$(git -C "$QMK_ROOT" diff --shortstat "$old" "$new" -- "$@")"
    [ -n "$stat" ] && echo "  $label:$stat"
    return 0
}
echo
echo "What changed, by area:"
area "hooks and core (quantum/, tmk_core/, platforms/)" quantum tmk_core platforms
area "keycode numbering (the client's catalog, the action-ABI digest)" data/constants/keycodes quantum/keycodes.h quantum/quantum_keycodes.h
area "VIA (the client's layout, storage and macros)" quantum/via.c quantum/via.h quantum/dynamic_keymap.c
area "RGB matrix (the client's effect list)" quantum/rgb_matrix
area "Charybdis board" keyboards/bastardkb/charybdis
area "submodules (ChibiOS, pico-sdk, ...)" lib
echo
echo "Keycode, VIA or RGB changes need a client follow-up (re-pin its upstream snapshots)."
