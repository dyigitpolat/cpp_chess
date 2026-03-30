# cpp_chess — mate-in-one GA

Genetic search over legal positions with fixed material (9 queens, 2 rooks, 2 knights, 2 bishops, white king vs black king) to maximize the number of **mate-in-one** moves for White, using the same rules as `chess.jsx` `solve()` plus a quiet start (neither king in check).

## Build (Release)

```bash
./build.sh
```

Or manually:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Binaries: `build/chess_ga` (search + HTML), `build/bench_ga` (CSV grid benchmark).

**Scripts:** `./build.sh` configures and builds. `./run.sh` runs `chess_ga`. `./build_and_run.sh` builds then runs `chess_ga`. `./scripts/bench_ga.sh` runs `bench_ga` (after `./build.sh`).

OpenMP is used automatically when CMake finds it (parallel fitness evaluation). On Linux clusters, if CMake does not find OpenMP, set **`OPENMP_ROOT`** or **`LIBOMP_ROOT`** to your module/install prefix before `./build.sh`. On some Apple toolchains OpenMP may be unavailable without Homebrew **libomp**; the program still builds and runs single-threaded.

## Run

Baseline mate count (expected **105**):

```bash
./build/chess_ga --baseline-only
```

Full GA + HTML export:

```bash
./build/chess_ga --output results.html --generations 500 --population 128
```

Use `./build/chess_ga --help` for the full option list.

**Search / diversity (defaults tuned for evolution):**

- **`--tournament-k N`** — tournament selection size (default **2**; larger values increase selection pressure; **2** tends to perform better on random-seed boards in benchmarks).
- **`--pools N`**, **`--pool-cross-elite Y`**, **`--cross-pool-inject-fraction F`** — with **`--pools` ≥ 2**, run **N** subpopulations each generation (distinct seeds for initialization); the top **Y** boards per pool form a shared gene pool; each pool’s next generation includes at least fraction **F** (default **0.1**) of **cross-pool** offspring (**always** `do_crossover` on two parents from that union, then optional mutation; not gated by `--crossover`). Same options work on **`bench_ga`**.
- **`--crossover-mode sorted|vector`** — `vector` (default) uses fixed semantic slots for 9 queens + other pieces, then collision repair; `sorted` is the legacy per-type sorted-square crossover.
- **`--stagnation-generations N`** — after `N` generations with no global improvement, replace the worst fraction with random legal boards and optionally hyper-mutate the best (`0` disables).
- **`--immigrant-fraction F`**, **`--hyper-mutation-steps N`**
- **`--diversity-inject-every N`**, **`--diversity-fraction F`** — periodic injection of random individuals among non-elites.
- **`--multi-mutation-max N`**, **`--multi-mutation-prob F`** — sometimes apply several successful mutations per offspring.
- **`--local-search`**, **`--no-local-search`**, **`--local-search-every N`**, **`--local-search-budget N`** — memetic greedy hill-climb on the global best board (all single-piece moves to empty / swaps / black king moves).

**Other:**

- **`--random-seed-board`** — random legal starting position instead of the image baseline.
- **`--quiet`** — disable per-generation stderr lines.

## Benchmarks

`bench_ga` prints CSV:

`seed,population,generations,best_fitness,seconds,plateau_generation,local_search,initial_fitness`

- **`plateau_generation`** — `last_improving_generation + 1` (0 if the global best never strictly increased after the initial seed evaluation).
- **`local_search`** — `1`/`0` for this run.
- **`initial_fitness`** — mate-in-one count of the seed board before the GA loop.

Local search is **off** by default in `bench_ga` for speed. Use **`--match-chess-ga`** or **`--preset full`** to match `chess_ga` defaults (LS on). **`--preset fast`** turns LS off. **`--tournament-k N`** is accepted by both binaries (same `GaConfig` default).

```bash
./build.sh
./build/bench_ga --seeds=1,7,42,99,123 --pops=64,128,256,512 --gens=100,200,500
./build/bench_ga --match-chess-ga --seeds=1,7 --pops=128 --gens=100,200
# or
./scripts/bench_ga.sh --seeds=1,7 --pops=64,128 --gens=50,100
```

Older sample rows (fewer columns) are in [`benchmarks/sample_grid.csv`](benchmarks/sample_grid.csv). Newer samples are under [`benchmarks/runs/`](benchmarks/runs/).

## Iterative tuning workflow

1. **Regression gate** – after code changes, run `./scripts/regression_bench.sh` (expects `best_fitness ≥ 105` on the image baseline with seed 1, pop 128, gen 100, `chess_ga`-equivalent GA). Override with `MIN=100 ./scripts/regression_bench.sh` if needed.
2. **Per-generation trace** – `chess_ga --trace-csv path/to/trace.csv` writes `generation,pop_best,global_best`. To parse stderr from a non-quiet run: `./build/chess_ga ... 2>&1 | ./scripts/parse_trace.sh`.
3. **Seed sweeps** – compare runs on the same `(population, generations)` with several seeds; archive CSVs in `benchmarks/runs/` with a short note of the command and hypothesis.

Final `chess_ga` summary line includes `initial` and `plateau_gen` for quick inspection.
