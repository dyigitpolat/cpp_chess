#include "board.hpp"
#include "ga.hpp"
#include "html_export.hpp"
#include "mate.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

namespace {

void print_usage() {
  std::fprintf(stderr,
               "cpp_chess — genetic search for mate-in-one density\n\n"
               "Usage:\n"
               "  chess_ga [options]\n\n"
               "Options:\n"
               "  --baseline-only     Print mate-in-one count for the baseline board and exit\n"
               "  --random-seed-board Use a random legal starting position instead of the image baseline\n"
               "  --quiet             Do not print per-generation best (stderr)\n"
               "  --population N      GA population size (default: 128)\n"
               "  --generations N     GA generations (default: 500)\n"
               "  --seed N            RNG seed (default: random)\n"
               "  --elite N           Elite count (default: 2)\n"
               "  --crossover F       Crossover probability 0..1 (default: 0.45)\n"
               "  --mutation F        Mutation probability 0..1 (default: 0.85)\n"
               "  --output PATH       Write results HTML (default: results.html)\n"
               "  --title TEXT        HTML title (default: Best position)\n"
               "  -h, --help          This help\n");
}

int arg_int(int argc, char** argv, int i, int def) {
  if (i + 1 >= argc) return def;
  return std::atoi(argv[i + 1]);
}

double arg_double(int argc, char** argv, int i, double def) {
  if (i + 1 >= argc) return def;
  return std::strtod(argv[i + 1], nullptr);
}

}  // namespace

int main(int argc, char** argv) {
  bool baseline_only = false;
  bool random_seed_board = false;
  chess::GaConfig cfg;
  std::string output = "results.html";
  std::string title = "Best position";

  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "-h") || !std::strcmp(argv[i], "--help")) {
      print_usage();
      return 0;
    }
    if (!std::strcmp(argv[i], "--baseline-only")) {
      baseline_only = true;
      continue;
    }
    if (!std::strcmp(argv[i], "--random-seed-board")) {
      random_seed_board = true;
      continue;
    }
    if (!std::strcmp(argv[i], "--quiet")) {
      cfg.report_each_generation = false;
      continue;
    }
    if (!std::strcmp(argv[i], "--population")) {
      cfg.population = arg_int(argc, argv, i, cfg.population);
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--generations")) {
      cfg.generations = arg_int(argc, argv, i, cfg.generations);
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--seed")) {
      cfg.seed = static_cast<unsigned>(arg_int(argc, argv, i, 0));
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--elite")) {
      cfg.elite = arg_int(argc, argv, i, cfg.elite);
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--crossover")) {
      cfg.crossover_rate = arg_double(argc, argv, i, cfg.crossover_rate);
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--mutation")) {
      cfg.mutation_rate = arg_double(argc, argv, i, cfg.mutation_rate);
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--output")) {
      if (i + 1 < argc) output = argv[++i];
      continue;
    }
    if (!std::strcmp(argv[i], "--title")) {
      if (i + 1 < argc) title = argv[++i];
      continue;
    }
    std::fprintf(stderr, "Unknown argument: %s\n", argv[i]);
    print_usage();
    return 1;
  }

  chess::Board seed_board{};
  if (random_seed_board) {
    std::mt19937 rng(cfg.seed ? cfg.seed : std::random_device{}());
    int guard = 0;
    while (!chess::try_random_legal_board(rng, seed_board) && ++guard < 500000) {
    }
    if (guard >= 500000) {
      std::fprintf(stderr, "chess_ga: could not sample a random legal board (try another --seed)\n");
      return 1;
    }
    int start_fit = chess::count_mate_in_one(seed_board);
    std::fprintf(stderr, "Using random legal seed board (mate-in-ones=%d)\n", start_fit);
  } else {
    chess::set_baseline(seed_board);
  }

  if (baseline_only) {
    int n = chess::count_mate_in_one(seed_board);
    std::printf("Mate-in-one count: %d%s\n", n, random_seed_board ? "" : " (expected 105 for image baseline)");
    return (!random_seed_board && n != 105) ? 2 : 0;
  }

  chess::GaResult res = chess::run_genetic_algorithm(cfg, seed_board);

  std::printf("Best mate-in-one count: %d\n", res.best_fitness);

  chess::write_results_html(output, res.best_board, res.best_fitness, res.best_mates, title);
  std::printf("Wrote %s\n", output.c_str());

  return 0;
}
