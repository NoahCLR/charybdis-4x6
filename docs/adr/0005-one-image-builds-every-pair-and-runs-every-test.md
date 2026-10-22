# One image builds every pair and runs every host test

Cited as D-F05.

The pair Noah flashes, the pair a release publishes and the host suite all
run in the image `tools/build-image` names, pinned by its multi-arch index
digest, through Docker and without network; nothing falls back to the Mac's
compiler. Before, local pairs and releases used different GCCs, and host tests
passed on the Mac's clang and failed in CI on GCC-only warnings. Builds are
reproducible (`SKIP_VERSION`, mapped source paths), so one userspace commit and
BK pin give one pair on any machine.

The image is our own, built from QMK's official `qmk_cli` image by
`tools/build-image.dockerfile`. QMK's arm64 and amd64 variants link different
ARM target code, so both platforms get the amd64 variant's target files and
keep their native compiler programs: a Mac builds at native speed and gets
CI's bytes. It adds ripgrep and Node as release binaries pinned by checksum.

Considered: Debian's packages for the tools (a rebuild would install whatever
the mirror held that day), a separate test image (two images to pin for no
gain), and keeping the Mac's clang (it warns differently). Local builds and
tests compile in-memory copies of the checkout and BK, so several worktrees
build at once without a lock. Procedure: [`DEVELOPMENT.md`](../DEVELOPMENT.md),
"The build image".
