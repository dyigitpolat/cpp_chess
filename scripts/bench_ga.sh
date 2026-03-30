#!/usr/bin/env bash
# Run CSV benchmark (build first: ./build.sh).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec "$ROOT/build/bench_ga" "$@"
