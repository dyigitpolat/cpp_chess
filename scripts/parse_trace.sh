#!/usr/bin/env bash
# Convert chess_ga stderr lines to CSV on stdout:
#   generation N: best=X (overall so far: Y)
# -> generation,pop_best,global_best
# Usage: ./build/chess_ga ... 2>&1 | ./scripts/parse_trace.sh
set -euo pipefail
echo "generation,pop_best,global_best"
grep -E '^generation [0-9]+: best=' | sed -E 's/^generation ([0-9]+): best=([0-9]+) \(overall so far: ([0-9]+)\)/\1,\2,\3/'
