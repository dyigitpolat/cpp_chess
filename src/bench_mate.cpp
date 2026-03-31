#include "board.hpp"
#include "mate.hpp"

#include <cstdio>
#include <cstdlib>

// Tight loop over count_mate_in_one on the image baseline board (no OpenMP).
// Use with: perf record -g --call-graph dwarf -- ./bench_mate N
int main(int argc, char** argv) {
  long long iters = 5'000'000;
  if (argc > 1) {
    iters = std::atoll(argv[1]);
  }
  if (iters < 1) {
    std::fprintf(stderr, "usage: %s [iterations]\n", argv[0]);
    return 1;
  }

  chess::Board b{};
  chess::set_baseline(b);

  long long sink = 0;
  for (long long i = 0; i < iters; ++i) {
    sink += static_cast<long long>(chess::count_mate_in_one(b));
  }

  std::printf("%lld iterations (sink=%lld)\n", iters, sink);
  return 0;
}
