#!/usr/bin/env bash
# Smoke test: image baseline, seed 1, pop 128, gen 100, chess_ga-equivalent GA config.
# Exits 0 if best_fitness >= MIN (default 105). Requires ./build/bench_ga.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MIN="${MIN:-105}"
BENCH="${ROOT}/build/bench_ga"
if [[ ! -x "$BENCH" ]]; then
  echo "regression_bench: build bench_ga first (./build.sh)" >&2
  exit 2
fi
line="$("$BENCH" --seeds=1 --pops=128 --gens=100 --match-chess-ga 2>/dev/null | tail -1)"
best="$(echo "$line" | cut -d, -f4)"
echo "regression_bench: best_fitness=$best (min_ok=$MIN)"
if [[ -z "${best// }" ]] || ! [[ "$best" =~ ^[0-9]+$ ]]; then
  echo "regression_bench: could not parse CSV line: $line" >&2
  exit 3
fi
if (( best < MIN )); then
  echo "regression_bench: FAIL ($best < $MIN)" >&2
  exit 1
fi
echo "regression_bench: OK"
