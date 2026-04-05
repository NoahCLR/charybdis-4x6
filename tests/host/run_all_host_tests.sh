#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"

"$ROOT/run_key_runtime_transition_tests.sh"
"$ROOT/run_pd_mode_tests.sh"
