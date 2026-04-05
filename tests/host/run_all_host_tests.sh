#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"

sh "$ROOT/run_key_behavior_lookup_tests.sh"
sh "$ROOT/run_key_runtime_transition_tests.sh"
sh "$ROOT/run_pd_mode_tests.sh"
