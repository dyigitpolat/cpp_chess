# Profiling cpp_chess (Linux `perf`)

## Kernel access

- **`/proc/sys/kernel/perf_event_paranoid`**: must allow user profiling. Typical values:
  - **`-1`**: permissive (good for `perf record` / `perf stat`).
  - **`4`**: very restrictive; `perf` often fails for normal users.
- Temporary: `sudo sysctl kernel.perf_event_paranoid=-1`
- Persistent: e.g. `/etc/sysctl.d/99-perf.conf` with `kernel.perf_event_paranoid=-1`

## Kernel symbols (optional)

If `perf report` warns about **kptr_restrict** or **kallsyms**, userland stacks in **`chess_ga`** are still useful. Resolving kernel symbols is optional for application tuning.

## CPU governor (optional)

For repeatable microbenchmarks, consider `performance` governor; not required for GA profiling.

## Build: Profile configuration

Use **`CMAKE_BUILD_TYPE=Profile`** for readable stacks and line info:

```bash
cmake -B build-profile -DCMAKE_BUILD_TYPE=Profile
cmake --build build-profile --parallel
```

This sets **`-O3 -g -fno-omit-frame-pointer`** and **disables LTO** so `perf report` / flame graphs show clearer `chess::` symbols.

Release builds remain the default for real performance measurements (may use LTO).

## Scripts

- **`scripts/profile.sh`** — quick smoke (`perf record` or `/usr/bin/time`).
- **`scripts/profile_all.sh`** — full suite: `perf record` (dwarf), `perf stat`, `OMP_NUM_THREADS=1` variant, optional flame graph, writes under `benchmarks/perf/`.

## Workloads (fixed)

See `scripts/profile_all.sh` for `CHESS_GA`, `GENS`, `POP`, `SEED`. Override env vars when invoking.

## Microbenchmark

`bench_mate` (see CMake) runs **`count_mate_in_one`** on the image baseline in a tight loop with **no OpenMP**, isolating [`mate.cpp`](../src/mate.cpp) / [`board.cpp`](../src/board.cpp).

```bash
./build-profile/bench_mate 2000000
perf record -g --call-graph dwarf -o benchmarks/perf/bench_mate.data -- ./build-profile/bench_mate 500000
perf report -i benchmarks/perf/bench_mate.data
```

**Separate your binary from OpenMP runtime:**

```bash
perf report -i perf.data --stdio --dsos=chess_ga --sort symbol --no-children
perf report -i perf.data --stdio --sort dso --no-children
```

The second line shows **libgomp** vs **`chess_ga`** time at the DSO level; the first lists **`chess::`** symbols in the binary only.

## What to archive per run

- Git commit, **`CMAKE_BUILD_TYPE`**, relevant **CXXFLAGS**.
- **`cat /proc/sys/kernel/perf_event_paranoid`**
- Command line and **`OMP_NUM_THREADS`**
- Paths to **`perf.data`**, **`perf.stat.txt`**, **`report.txt`**, optional **`flame.svg`**
