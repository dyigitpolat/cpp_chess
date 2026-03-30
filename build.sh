#!/usr/bin/env bash
# Build cpp_chess (Release). From repo root: ./build.sh
#
# OpenMP (optional): CMake finds it automatically on many Linux installs (e.g. GCC + libgomp).
# On clusters, point CMake at your module-provided OpenMP if needed:
#   export OPENMP_ROOT=/path/to/openmp   # or LIBOMP_ROOT
#   ./build.sh
# macOS + Apple Clang: Homebrew libomp is detected when brew is available.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"

CMAKE_ARGS=(-DCMAKE_BUILD_TYPE=Release)

# Prefer explicit OpenMP location (HPC modules, custom installs, Spack, etc.)
if [[ -n "${OPENMP_ROOT:-}" ]]; then
  CMAKE_ARGS+=(-DOpenMP_ROOT="${OPENMP_ROOT}")
elif [[ -n "${LIBOMP_ROOT:-}" ]]; then
  CMAKE_ARGS+=(-DOpenMP_ROOT="${LIBOMP_ROOT}")
elif command -v brew >/dev/null 2>&1; then
  # Homebrew libomp: Apple Clang does not ship OpenMP
  LIBOMP="$(brew --prefix libomp 2>/dev/null)" || true
  if [[ -n "${LIBOMP:-}" && -d "${LIBOMP}/include" ]]; then
    CMAKE_ARGS+=(-DOpenMP_ROOT="${LIBOMP}")
  fi
fi

cmake -S "$ROOT" -B "$ROOT/build" "${CMAKE_ARGS[@]}"
cmake --build "$ROOT/build" --parallel
