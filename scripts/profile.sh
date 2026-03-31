#!/usr/bin/env sh
# Quick smoke: perf record with dwarf stacks (Profile build recommended), else time -v.
# Full suite: scripts/profile_all.sh
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1
BIN="${CHESS_GA:-$ROOT/build/chess_ga}"
[ -x "$BIN" ] || {
  echo "Build first: cmake -B build -DCMAKE_BUILD_TYPE=Profile && cmake --build build"
  echo "Or: CHESS_GA=/path/to/chess_ga $0"
  exit 1
}
OUT="${PERF_OUT:-/tmp/cpp_chess_perf.data}"
if command -v perf >/dev/null 2>&1 && perf record -g --call-graph dwarf,4096 -F 997 -o "$OUT" -- \
  "$BIN" --quiet --generations 25 --population 256 --seed 1 --no-local-search 2>/dev/null; then
  perf report -i "$OUT" --stdio --no-children | head -45
elif command -v perf >/dev/null 2>&1; then
  echo "perf record failed (kernel tools missing?). Wall time:"
  /usr/bin/time -v "$BIN" --quiet --generations 25 --population 256 --seed 1 --no-local-search 2>&1
else
  echo "perf(1) not installed. Wall time:"
  /usr/bin/time -v "$BIN" --quiet --generations 25 --population 256 --seed 1 --no-local-search 2>&1
fi
