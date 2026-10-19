#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

build_and_run() {
    name="$1"
    shift
    cc -std=c11 -Wall -Wextra -Werror -pedantic "$@" \
        -I"$ROOT" \
        "$ROOT/tests/host/profile_candidate_transaction_test.c" \
        "$ROOT/users/noah/lib/profile/protocol/profile_candidate_v1.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_candidate_transaction.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
        -o "$BUILD_DIR/profile_candidate_transaction_test_$name"
    "$BUILD_DIR/profile_candidate_transaction_test_$name" "$ROOT/tests/fixtures/profile_candidate_v1.fixture"
}

build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer
