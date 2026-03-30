#!/usr/bin/env bash
# Build then run chess_ga with the same arguments (default: short GA + HTML).
# Example: ./build_and_run.sh --random-seed-board --seed 7 --generations 100
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
"$ROOT/build.sh"
exec "$ROOT/build/chess_ga" "$@"
