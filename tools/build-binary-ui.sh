#!/bin/sh
# Binary UI fork build helper, 2026-10-01.
set -eu
BINARY_UI_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BINARY_UI_OUT=${BINARY_UI_BUILD_DIR:-$BINARY_UI_ROOT/build/binary-ui}
cmake -S "$BINARY_UI_ROOT" -B "$BINARY_UI_OUT" -DCMAKE_BUILD_TYPE=Release -DNLE_VERSION=1.3.0 -DHACKDIR="$BINARY_UI_OUT/nethackdir" "$@"
cmake --build "$BINARY_UI_OUT" --target nethack -j "${BINARY_UI_JOBS:-3}"
