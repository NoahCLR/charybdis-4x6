# Sourced, on the Mac side, by tests/host/in_build_image.sh and
# tools/build-firmware-pair.sh before they run in the build image.
#
# Compiling through Docker's file sharing with macOS is several times slower
# than on the image's own disk, so both read copies at the original paths: the
# checkout is copied into memory on every run (a fraction of a second), and
# QMK, a gigabyte, into a Docker volume once per commit
# (charybdis-host-qmk-<commit>; the three newest are kept). A QMK with local
# changes, untracked files or submodules not at their commits gets a volume
# named by its commit and a hash of those changes
# (charybdis-host-qmk-<commit>-<changes>), so the host tests and the pair build
# of one run share one copy, and a run with the same changes reuses it.

# noah_skip_git DIR: a main checkout's .git directory is mounted, not copied; a
# worktree's .git file is copied.
noah_skip_git() { [ -d "$1/.git" ] && printf '%s' "--exclude=./.git" || true; }

# noah_qmk_changes QMK: a hash of what QMK holds beyond its commit, or nothing
# when it holds nothing else. What counts is the pair build's own test of a
# clean BK: tracked and untracked changes, and submodules not at their commits
# or not initialized (untracked files inside submodules do not count).
noah_qmk_changes() {
    noah_cq_status="$(git -C "$1" status --porcelain --ignore-submodules=untracked)$(git -C "$1" submodule status --recursive | sed -n '/^[+-U]/p')"
    [ -n "$noah_cq_status" ] || return 0
    {
        printf '%s\n' "$noah_cq_status"
        git -C "$1" diff HEAD --binary --ignore-submodules=untracked
        git -C "$1" submodule foreach --quiet --recursive 'git diff HEAD --binary'
        git -C "$1" ls-files --others --exclude-standard | while IFS= read -r noah_cq_file; do
            printf '%s %s\n' "$(git -C "$1" hash-object --no-filters -- "$noah_cq_file")" "$noah_cq_file"
        done
    } | git hash-object --stdin | cut -c1-12
}

# noah_qmk_volume QMK IMAGE: prints the Docker volume holding QMK as it is,
# copying it there first if needed, or nothing when QMK is not a Git checkout
# (it is then copied for each run).
noah_qmk_volume() {
    noah_cq_root="$1"
    noah_cq_image="$2"
    noah_cq_head="$(git -C "$noah_cq_root" rev-parse HEAD 2>/dev/null || true)"
    [ -n "$noah_cq_head" ] || return 0
    noah_cq_key="$(noah_qmk_changes "$noah_cq_root")" || return 1
    noah_cq_volume="charybdis-host-qmk-$noah_cq_head${noah_cq_key:+-$noah_cq_key}"
    noah_cq_new=0
    docker volume inspect "$noah_cq_volume" >/dev/null 2>&1 || noah_cq_new=1
    # Locked, so runs starting together copy once; the marker is written last,
    # so an interrupted copy is made again.
    docker run --rm --network none -v "$noah_cq_volume:/q" -v "$noah_cq_root:/src:ro" "$noah_cq_image" flock /q/.charybdis-lock sh -c \
        "[ -f /q/.charybdis-copied ] || { echo 'Copying QMK $noah_cq_head${noah_cq_key:+ with its local changes} into Docker volume $noah_cq_volume (once)...' && tar -C /src $(noah_skip_git "$noah_cq_root") --exclude=./.build -cf - . | tar -C /q -xf - && chown -R $(id -u):$(id -g) /q && touch /q/.charybdis-copied; }" >&2 || return 1
    if [ "$noah_cq_new" = 1 ]; then
        # Keep the three newest; one in use by a running container is kept.
        docker volume ls -q --filter name=charybdis-host-qmk- | xargs docker volume inspect --format '{{.CreatedAt}} {{.Name}}' 2>/dev/null \
            | sort -r | sed -n '4,$s/^.* //p' | xargs docker volume rm >/dev/null 2>&1 || true
    fi
    printf '%s\n' "$noah_cq_volume"
}

# noah_qmk_copy_excludes VOLUME QMK: the tar options that copy QMK from where it
# is mounted: from its volume (VOLUME set), leaving out the volume's own
# markers, or from a QMK that is not a Git checkout (VOLUME empty), leaving
# out its build output.
# A main checkout's .git directory is mounted over the copy either way; in a
# volume it is the empty mount point a test run left.
noah_qmk_copy_excludes() {
    if [ -n "$1" ]; then
        printf '%s' "$(noah_skip_git "$2") --exclude=./.charybdis-copied --exclude=./.charybdis-lock"
    else
        printf '%s' "$(noah_skip_git "$2") --exclude=./.build"
    fi
}
