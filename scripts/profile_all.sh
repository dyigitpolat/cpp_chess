#!/usr/bin/env bash
# Full profiling suite: perf record (dwarf), perf stat, single-thread variant,
# optional flame graph. Outputs under benchmarks/perf/ (gitignored).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$ROOT/benchmarks/perf}"
mkdir -p "$OUT"

# Fixed workload (override via env): heavy fitness, long enough for stable %.
CHESS_GA="${CHESS_GA:-$ROOT/build/chess_ga}"
GENS="${GENS:-50}"
POP="${POP:-256}"
SEED="${SEED:-1}"
BENCH_GA="${BENCH_GA:-$ROOT/build/bench_ga}"
# Fixed grid for profiling (override BENCH_GA_ARGS). Full default grid is too slow here.
: "${BENCH_GA_ARGS:=--seeds=1 --pops=128 --gens=50}"
# Set ENABLE_LOCAL_SEARCH=1 to profile with memetic local search (omit --no-local-search).
ENABLE_LOCAL_SEARCH="${ENABLE_LOCAL_SEARCH:-0}"

STAMP="$(date +%Y%m%d_%H%M%S)"
PREFIX="$OUT/run_${STAMP}"

log() { printf '%s\n' "$*"; }

{
  echo "date=$(date -Iseconds 2>/dev/null || date)"
  echo "git=$(git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
  echo "perf_event_paranoid=$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo n/a)"
  echo "kptr_restrict=$(cat /proc/sys/kernel/kptr_restrict 2>/dev/null || echo n/a)"
  echo "CHESS_GA=$CHESS_GA"
  echo "OMP_NUM_THREADS=${OMP_NUM_THREADS:-unset}"
  echo "GENS=$GENS POP=$POP SEED=$SEED"
  echo "ENABLE_LOCAL_SEARCH=$ENABLE_LOCAL_SEARCH"
} | tee "${PREFIX}_meta.txt"

if [ ! -x "$CHESS_GA" ]; then
  log "Missing executable: $CHESS_GA"
  log "Build with: cmake -B build-profile -DCMAKE_BUILD_TYPE=Profile && cmake --build build-profile"
  exit 1
fi

if ! command -v perf >/dev/null 2>&1; then
  log "perf(1) not installed; skipping record/stat."
  exit 0
fi

if [ "$ENABLE_LOCAL_SEARCH" = 1 ]; then
  GA_ARGS=(--quiet --generations "$GENS" --population "$POP" --seed "$SEED")
else
  GA_ARGS=(--quiet --generations "$GENS" --population "$POP" --seed "$SEED" --no-local-search)
fi

log "=== perf record (dwarf stacks) ==="
if perf record -g --call-graph dwarf,4096 -F 997 -o "${PREFIX}_perf.data" -- \
  "$CHESS_GA" "${GA_ARGS[@]}"; then
  perf report -i "${PREFIX}_perf.data" --stdio --no-children | head -80 | tee "${PREFIX}_report_top.txt" || true
  log "Saved ${PREFIX}_perf.data"
else
  log "perf record failed (permissions? paranoid?)"
fi

log "=== perf report: chess_ga binary symbols (--dsos=chess_ga) ==="
if [ -f "${PREFIX}_perf.data" ]; then
  perf report -i "${PREFIX}_perf.data" --stdio --dsos=chess_ga --sort symbol --no-children 2>/dev/null | head -60 | tee "${PREFIX}_report_chess_symbols.txt" || true
  log "=== perf report: DSO share (binary vs libgomp, etc.) ==="
  perf report -i "${PREFIX}_perf.data" --stdio --sort dso --no-children 2>/dev/null | head -30 | tee "${PREFIX}_report_dso.txt" || true
fi

log "=== perf stat ==="
perf stat -e cycles,instructions,cache-misses,branch-misses,context-switches,cpu-migrations -- \
  "$CHESS_GA" "${GA_ARGS[@]}" 2>&1 | tee "${PREFIX}_perf_stat.txt" || true

log "=== OMP_NUM_THREADS=1 (same workload) ==="
OMP_NUM_THREADS=1 perf stat -e cycles,instructions,cache-misses,branch-misses,context-switches,cpu-migrations -- \
  "$CHESS_GA" "${GA_ARGS[@]}" 2>&1 | tee "${PREFIX}_perf_stat_omp1.txt" || true

if [ -x "$BENCH_GA" ]; then
  log "=== bench_ga (fixed grid: $BENCH_GA_ARGS) ==="
  # shellcheck disable=SC2086
  perf stat -- "$BENCH_GA" $BENCH_GA_ARGS 2>&1 | tee "${PREFIX}_bench_ga_stat.txt" || true
else
  log "bench_ga not built at $BENCH_GA (optional)"
fi

if command -v stackcollapse-perf.pl >/dev/null 2>&1 && command -v flamegraph.pl >/dev/null 2>&1 && [ -f "${PREFIX}_perf.data" ]; then
  log "=== flame graph ==="
  perf script -i "${PREFIX}_perf.data" | stackcollapse-perf.pl | flamegraph.pl > "${PREFIX}_flame.svg" && log "Wrote ${PREFIX}_flame.svg"
else
  log "Flame graph tools not found or no perf.data; skip flame.svg"
fi

log "Done. Artifacts in $OUT"
