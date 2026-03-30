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

The executable is `build/chess_ga`.

**Scripts:** `./build.sh` configures and builds. `./run.sh` runs the binary (after a build). `./build_and_run.sh` builds then runs, forwarding all arguments (for example `./build_and_run.sh --random-seed-board --seed 1 --output results.html`).

OpenMP is used automatically when CMake finds it (parallel fitness evaluation). On some Apple toolchains OpenMP may be unavailable; the program still builds and runs single-threaded.

## Run

Baseline mate count (expected **105**):

```bash
./build/chess_ga --baseline-only
```

Full GA + HTML export:

```bash
./build/chess_ga --output results.html --generations 500 --population 128
```

Use `./build/chess_ga --help` for all options (`--seed`, `--elite`, `--crossover`, `--mutation`, `--title`, etc.).

- **`--random-seed-board`** — start the GA from a random legal position (same material) instead of the image baseline.
- **`--quiet`** — disable per-generation lines on stderr (by default each generation prints `generation N: best=… (overall so far: …)`).
