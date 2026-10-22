# Sourced by every host runner right after `set -eu`:
#
#   . "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"
#
# Host tests run in the image tools/build-image names, the one the pair is
# built in and CI runs (D-F05), so a local run and CI compile and run the same
# tests with the same compiler, sanitizers, Python, Node and ripgrep. Outside
# the image this runs the runner there instead, with Docker, and never falls
# back to the host's tools. Inside it (CI's container, or a runner started by
# another one), it does nothing.
#
# The runner reads copies at the original paths, made as tools/qmk-copies.sh
# describes: the checkout in memory, QMK from a volume per commit.
# Git directories are mounted, so worktree .git pointers resolve. A runner
# that writes into the checkout or QMK (a regenerated header, a qmk compile)
# sets noah_writes_checkout=1 before sourcing this, or is asked to write by
# NOAH_WRITE_AUTHORED_MACRO_BANK=1, and works on the mounts themselves. There
# is no network: everything a test reads is in the image or the checkouts.
# The image is pulled once, the only time a run needs the network.
# NOAH_* variables and QMK_ROOT pass through. NOAH_BUILD_IMAGE runs the tests
# in another image, such as one tools/make-build-image.sh has built but not
# yet published.

# NOAH_IN_BUILD_IMAGE is 1 inside; 0 forces the Docker path (its tests use it).
case "${NOAH_IN_BUILD_IMAGE:-}" in
    1) noah_in_image=1 ;;
    0) noah_in_image=0 ;;
    *) noah_in_image=0; [ ! -f /.dockerenv ] || noah_in_image=1 ;;
esac

if [ "$noah_in_image" = 0 ]; then
    noah_repo="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
    noah_image="${NOAH_BUILD_IMAGE:-$(cat "$noah_repo/tools/build-image")}"
    [ -z "${NOAH_BUILD_IMAGE:-}" ] || echo "Host tests run in $noah_image (NOAH_BUILD_IMAGE), not the pinned image." >&2
    command -v docker >/dev/null 2>&1 || {
        echo "Docker is required: host tests run in $noah_image, the image CI uses." >&2; exit 1; }
    docker info >/dev/null 2>&1 || {
        echo "Docker is not running; start Docker Desktop. Host tests run in $noah_image, the image CI uses." >&2; exit 1; }
    docker image inspect "$noah_image" >/dev/null 2>&1 || {
        echo "Pulling $noah_image (once)..." >&2
        docker pull "$noah_image" >&2 || { echo "Cannot pull $noah_image." >&2; exit 1; }
    }

    . "$noah_repo/tests/host/noah_host_qmk_env.sh"
    . "$noah_repo/tools/qmk-copies.sh"
    noah_qmk="$(noah_host_find_qmk_root "$noah_repo" 2>/dev/null || true)"
    noah_script="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/$(basename -- "$0")"
    noah_uid="$(id -u)"
    noah_gid="$(id -g)"
    noah_direct="${noah_writes_checkout:-0}"
    [ "${NOAH_WRITE_AUTHORED_MACRO_BANK:-}" != 1 ] || noah_direct=1

    noah_run="docker run --rm --network none --user $noah_uid:$noah_gid -e HOME=/tmp -e PYTHONDONTWRITEBYTECODE=1 -e NOAH_IN_BUILD_IMAGE=1"
    noah_run="$noah_run -e GIT_CONFIG_COUNT=1 -e GIT_CONFIG_KEY_0=safe.directory -e GIT_CONFIG_VALUE_0=*"
    [ -z "$noah_qmk" ] || noah_run="$noah_run -e QMK_ROOT=$noah_qmk"
    for noah_var in $(env | sed -n 's/^\(NOAH_[A-Z0-9_]*\)=.*/\1/p'); do
        [ "$noah_var" = NOAH_IN_BUILD_IMAGE ] || noah_run="$noah_run -e $noah_var"
    done

    # Git directories, at their own paths.
    for noah_dir in "$(git -C "$noah_repo" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || true)" \
                    ${noah_qmk:+"$(git -C "$noah_qmk" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || true)"}; do
        [ -z "$noah_dir" ] || noah_run="$noah_run -v $noah_dir:$noah_dir"
    done

    noah_setup=""
    if [ "$noah_direct" = 1 ]; then
        noah_run="$noah_run -v $noah_repo:$noah_repo"
        [ -z "$noah_qmk" ] || noah_run="$noah_run -v $noah_qmk:$noah_qmk"
    else
        # The copy lives in memory, at the checkout's own path.
        noah_run="$noah_run --tmpfs $noah_repo:uid=$noah_uid,gid=$noah_gid,exec -v $noah_repo:/mnt/noah/repo:ro"
        noah_setup="tar -C /mnt/noah/repo $(noah_skip_git "$noah_repo") -cf - . | tar -C '$noah_repo' -xf -"
        if [ -n "$noah_qmk" ]; then
            noah_volume="$(noah_qmk_volume "$noah_qmk" "$noah_image")"
            if [ -n "$noah_volume" ]; then
                # Tests only read QMK, so they read the volume itself.
                noah_run="$noah_run -v $noah_volume:$noah_qmk"
            else
                noah_run="$noah_run --tmpfs $noah_qmk:uid=$noah_uid,gid=$noah_gid,exec -v $noah_qmk:/mnt/noah/qmk:ro"
                noah_setup="$noah_setup && tar -C /mnt/noah/qmk $(noah_qmk_copy_excludes "" "$noah_qmk") -cf - . | tar -C '$noah_qmk' -xf -"
            fi
        fi
    fi

    # Paths here have no spaces (checkouts and worktrees under the workspace);
    # the runner's own arguments keep their quoting through "$@". No globbing:
    # safe.directory is a literal *.
    set -f
    if [ -n "$noah_setup" ]; then
        exec $noah_run -w / "$noah_image" sh -c "$noah_setup && cd '$noah_repo' && exec sh '$noah_script' \"\$@\"" runner "$@"
    fi
    exec $noah_run -w "$noah_repo" "$noah_image" sh "$noah_script" "$@"
fi
unset noah_in_image
# Undefined behaviour fails the test that reaches it, as an address error does.
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}"
