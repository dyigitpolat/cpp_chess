#!/usr/bin/env bash
# Run the built binary (build first with ./build.sh).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
exec "$ROOT/build/chess_ga" "$@"
