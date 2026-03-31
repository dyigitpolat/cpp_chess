# Optimization log (mate / board hot path)

This file records profiling baselines and code changes so regressions are traceable. See [`PROFILING.md`](PROFILING.md) for how to reproduce runs.

## 2026-03-31 — Target-only white attacks + cached black king square

### Profiling baseline (pre-change, same commit tree before edits)

- Full suite: `benchmarks/perf/run_20260331_152925_*` (Profile `chess_ga`).
- Top binary symbol (GA, `--no-local-search`): `chess::attacks_from_impl` (~5.6% self in `chess_ga` DSO); DSO mix ~91% `libgomp` (parallel fitness).
- `bench_mate` (`run_20260331_152925_bench_mate.data`): `attacks_from_impl` ~65% self; heavy `in_bounds` + `is_attacked` → `black_king_has_legal_move`.
- `OMP_NUM_THREADS=1` vs default: single-thread ~15.4B cycles vs ~242.5B total cycles on same logical work (parallel run uses many cores; compare wall time separately).
- Optional LS sample: `run_20260331_152925_with_ls.data` (shorter run) — fitness path still visible alongside local search.

### Change

1. **`white_piece_attacks_sq`**: `is_attacked(..., by_white=true)` no longer calls `attacks_from_impl` + vector scan per piece; it walks rays/sliding/knight/king toward `target_sq` only.
2. **`black_king_has_legal_move(b, black_king_sq)`**: `count_mate_in_one` caches `find_king_sq(b, false)` once; after White-only moves the black king square is unchanged, so repeated 64-square king lookups are avoided in the inner loop.

### Validation

- `./scripts/regression_bench.sh` — OK (`best_fitness=105`).
- **Profile** `perf stat -e cycles` (same fixed workload as baseline): **~203.8B cycles** after vs **~242.5B cycles** before (~16% fewer CPU cycles on aggregate counters for the run). `perf stat` “time elapsed” is not wall-clock for multi-threaded CPU time; use cycles for A/B.
- **Release** `perf stat` on the same command: **~236.0B cycles** (post-change snapshot; compare future runs on the same machine).
- `bench_mate` post-change: self-time shifts toward `is_attacked` (expected: less work inside `attacks_from_impl` for the white-attack probe).

### Follow-ups (if profiling again)

- Further gains may require reducing **per-square** scans in `is_attacked` (piece lists / bitboards) or tuning OpenMP only if profiles show non-mate bottlenecks.

### Re-run profiling suite

```bash
cmake -B build-profile -DCMAKE_BUILD_TYPE=Profile && cmake --build build-profile --parallel
CHESS_GA="$PWD/build-profile/chess_ga" BENCH_GA="$PWD/build-profile/bench_ga" ./scripts/profile_all.sh
# Optional memetic search:
# ENABLE_LOCAL_SEARCH=1 GENS=20 POP=128 CHESS_GA="$PWD/build-profile/chess_ga" ./scripts/profile_all.sh
```

## 2026-03-31 — Cached king squares in `count_mate_in_one` (mate loop + quiet check)

### Profiling

- `bench_mate` pre-change (`benchmarks/perf/confirm_pre_opt.data`): top self symbol **`chess::is_attacked`** (~77%); call path includes **`black_king_has_legal_move`** → **`in_check`** / **`find_king_sq`**.

### Change

1. **`black_king_in_check(b, black_king_sq)`** — `is_attacked(b, black_king_sq, true)` without scanning for the black king.
2. **`white_king_in_check_vs_lone_black(b, white_king_sq, black_king_sq)`** — same as the white branch of **`in_check`** (only Black can give check: lone king), **O(1)** via **`kings_adjacent`**, no **`find_king_sq`**.
3. **[`mate.cpp`](../src/mate.cpp)**: one **`find_king_sq`** each for White/Black at entry; inner loop derives **`white_king_sq`** after **`apply_move`**; replaces **`in_check(nb, …)`** with the two helpers above. Initial “quiet position” checks use the same helpers so **`in_check`** is not called twice with four extra king scans.

Documented in [`board.hpp`](../src/board.hpp): valid for **lone black king** and **White does not capture the black king** (mate-in-one GA).

### Validation

- `./scripts/regression_bench.sh` — OK (`best_fitness=105`).
- Snapshot: **`bench_mate` 100k** ~**25.6 s** wall (Profile build); **`perf stat` cycles** on fixed GA command ~**237.2B** (compare on the same machine under similar load; multi-threaded **`time elapsed`** is not comparable to user time).
- Post-change **`perf report`** on **`bench_mate`**: **`is_attacked`** self-% can **rise** vs older profiles because **`in_check` / `find_king_sq`** cost was removed from the hot path (denominator effect).

### Follow-ups

- Next lever: **Phase 3** in the further-perf plan — iterate **white pieces only** in **`is_attacked`** / **`pseudo_moves`** (mask or small list), if **`is_attacked`** remains the bottleneck.

## 2026-03-31 — White occupancy mask + bit iteration

### Change

1. **`white_piece_occ_mask(b)`** — one bit per square for white pieces (~16 bits set in the default composition).
2. **`white_attacks_target_sq`** (static) — given a mask, only runs **`white_piece_attacks_sq`** for **set** squares (uses **`std::countr_zero`** / clear-lowest-bit).
3. **`is_attacked(..., by_white=true)`** — builds the mask once, then uses **`white_attacks_target_sq`** (same work as a single pass, but inner probe loop is tight over **~16** squares).
4. **`black_king_in_check(b, black_king_sq, white_mask)`** — overload so **`count_mate_in_one`** can compute **`white_piece_occ_mask(nb)` once** per candidate position and reuse it for **`black_king_in_check`** (avoids building the mask twice inside **`is_attacked`**).
5. **`pseudo_moves` (White)** — generates from the mask iteration instead of scanning all 64 squares for white pieces.
6. **`pseudo_moves` (Black)** — unchanged from prior fast path: only the black king (no full-board scan).

### Validation

- `./scripts/regression_bench.sh` — OK (`best_fitness=105`).
- Compare **`bench_mate`** / **`perf stat`** on a fixed command on your machine when tuning (see [`PROFILING.md`](PROFILING.md)).
