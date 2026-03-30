#!/usr/bin/env bash
# Summarize best_fitness (column 4) from bench_ga CSV: count, mean, min, max.
# Usage: ./build/bench_ga ... | ./scripts/summarize_bench_csv.sh
set -euo pipefail
awk -F, '
NR==1 { next }
NF>=4 {
  v=$4+0
  n++; sum+=v; if(n==1||v<mn) mn=v; if(n==1||v>mx) mx=v
}
END {
  if(n==0) { print "no data rows"; exit 1 }
  printf "n=%d mean=%.3f min=%d max=%d\n", n, sum/n, mn, mx
}'
