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
               "  --quiet             Do not print per-generation best (stderr); timing line still prints\n"
               "  --no-generation-time  Do not print per-generation wall time (stderr)\n"
               "  --population N      GA population size (default: 128)\n"
               "  --generations N     GA generations (default: 500)\n"
               "  --seed N            RNG seed (default: random)\n"
               "  --elite N           Elite count (default: 2)\n"
               "  --pools N           Parallel subpopulations per generation (default: 1; 2+ enables cross-pool elite crossover)\n"
               "  --pool-cross-elite N  Top N individuals per pool in the cross-pool gene pool (default: 5)\n"
               "  --cross-pool-inject-fraction F  ceil(pop×F) cross-pool offspring per pool after elites (default: 0.1)\n"
               "  --tournament-k N    Tournament size for parent selection (default: 2, min 2)\n"
               "  --crossover F       Crossover probability 0..1 (default: 0.45)\n"
               "  --mutation F        Mutation probability 0..1 (default: 0.65)\n"
               "  --crossover-mode sorted|vector  Building blocks: sorted (legacy) or vector (default)\n"
               "  --stagnation-generations N  Inject immigrants after N gens without improvement (0=off, default 40)\n"
               "  --immigrant-fraction F      Worst fraction replaced on stagnation (default 0.12)\n"
               "  --hyper-mutation-steps N    Extra mutations on best when stagnating (default 4, 0=off)\n"
               "  --diversity-inject-every N  Every N gens inject random boards (0=off, default 50)\n"
               "  --diversity-fraction F      Fraction of pop replaced (default 0.08)\n"
               "  --multi-mutation-max N      Max steps in multi-mutation (default 4)\n"
               "  --multi-mutation-prob F     Chance to use 2..max steps (default 0.4)\n"
               "  --local-search              Enable memetic hill-climb (default on)\n"
               "  --no-local-search           Disable local search\n"
               "  --local-search-every N      Run LS on best every N generations (0=end only, default 25)\n"
               "  --local-search-budget N     Max mate evals per LS call (default 25000)\n"
               "  --trace-csv PATH    Write per-generation CSV (generation,pop_best,global_best)\n"
               "  --trace-every N     Trace every N generations (default 1)\n"
               "  --collect-trace     Store per-gen vectors in GaResult (for API use)\n"
               "  --output PATH       Write results HTML (default: results.html)\n"
               "  --title TEXT        HTML title (default: Best position)\n"
               "  --record-prefix STR Optional prefix when saving record HTML (see below)\n"
               "  On each new global best >= 100, also write {dir}{prefix}_{score}_{stem}.html\n"
               "  ({dir} from --output; {stem} from --output basename; omit prefix_ if --record-prefix omitted)\n"
               "  --minus-knight      Use one fewer white knight (1N instead of 2)\n"
               "  --minus-bishop      Use one fewer white bishop (1B instead of 2)\n"
               "  --minus-queen       Use one fewer white queen (8Q instead of 9)\n"
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

std::string stem_from_output_path(const std::string& path) {
  const size_t slash = path.find_last_of("/\\");
  std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
  const size_t dot = base.rfind('.');
  if (dot != std::string::npos && dot > 0) return base.substr(0, dot);
  return base;
}

}  // namespace

int main(int argc, char** argv) {
  bool baseline_only = false;
  bool random_seed_board = false;
  chess::GaConfig cfg;
  std::string output = "results.html";
  std::string title = "Best position";
  bool minus_knight = false;
  bool minus_bishop = false;
  bool minus_queen = false;

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
    if (!std::strcmp(argv[i], "--no-generation-time")) {
      cfg.report_generation_time = false;
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
    if (!std::strcmp(argv[i], "--pools")) {
      cfg.num_pools = arg_int(argc, argv, i, cfg.num_pools);
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--pool-cross-elite")) {
      cfg.pool_cross_elite = arg_int(argc, argv, i, cfg.pool_cross_elite);
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--cross-pool-inject-fraction")) {
      cfg.cross_pool_inject_fraction = arg_double(argc, argv, i, cfg.cross_pool_inject_fraction);
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--tournament-k")) {
      cfg.tournament_k = arg_int(argc, argv, i, cfg.tournament_k);
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
    if (!std::strcmp(argv[i], "--crossover-mode")) {
      if (i + 1 < argc) {
        if (!std::strcmp(argv[i + 1], "sorted"))
          cfg.crossover_mode = chess::CrossoverMode::Sorted;
        else
          cfg.crossover_mode = chess::CrossoverMode::VectorRepair;
      }
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--stagnation-generations")) {
      cfg.stagnation_generations = arg_int(argc, argv, i, cfg.stagnation_generations);
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--immigrant-fraction")) {
      cfg.immigrant_fraction = arg_double(argc, argv, i, cfg.immigrant_fraction);
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--hyper-mutation-steps")) {
      cfg.hyper_mutation_steps = arg_int(argc, argv, i, cfg.hyper_mutation_steps);
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--diversity-inject-every")) {
      cfg.diversity_inject_every = arg_int(argc, argv, i, cfg.diversity_inject_every);
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--diversity-fraction")) {
      cfg.diversity_fraction = arg_double(argc, argv, i, cfg.diversity_fraction);
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--multi-mutation-max")) {
      cfg.multi_mutation_max = arg_int(argc, argv, i, cfg.multi_mutation_max);
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--multi-mutation-prob")) {
      cfg.multi_mutation_prob = arg_double(argc, argv, i, cfg.multi_mutation_prob);
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--local-search")) {
      cfg.local_search_enabled = true;
      continue;
    }
    if (!std::strcmp(argv[i], "--no-local-search")) {
      cfg.local_search_enabled = false;
      continue;
    }
    if (!std::strcmp(argv[i], "--local-search-every")) {
      cfg.local_search_every = arg_int(argc, argv, i, cfg.local_search_every);
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--local-search-budget")) {
      cfg.local_search_budget = arg_int(argc, argv, i, cfg.local_search_budget);
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--trace-csv")) {
      if (i + 1 < argc) cfg.trace_csv_path = argv[++i];
      continue;
    }
    if (!std::strcmp(argv[i], "--trace-every")) {
      cfg.trace_every = arg_int(argc, argv, i, cfg.trace_every);
      ++i;
      continue;
    }
    if (!std::strcmp(argv[i], "--collect-trace")) {
      cfg.collect_trace = true;
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
    if (!std::strcmp(argv[i], "--record-prefix")) {
      if (i + 1 < argc) cfg.record_snapshot_prefix = argv[++i];
      continue;
    }
    if (!std::strcmp(argv[i], "--minus-knight")) {
      minus_knight = true;
      continue;
    }
    if (!std::strcmp(argv[i], "--minus-bishop")) {
      minus_bishop = true;
      continue;
    }
    if (!std::strcmp(argv[i], "--minus-queen")) {
      minus_queen = true;
      continue;
    }
    std::fprintf(stderr, "Unknown argument: %s\n", argv[i]);
    print_usage();
    return 1;
  }

  {
    int n_minus = (minus_knight ? 1 : 0) + (minus_bishop ? 1 : 0) + (minus_queen ? 1 : 0);
    if (n_minus > 1) {
      std::fprintf(stderr,
                   "chess_ga: use at most one of --minus-knight, --minus-bishop, and --minus-queen\n");
      return 1;
    }
  }
  if (minus_knight) cfg.material.white_knights = 1;
  if (minus_bishop) cfg.material.white_bishops = 1;
    if (minus_queen) cfg.material.white_queens = 8;

  cfg.record_snapshot_output_path = output;
  cfg.record_snapshot_stem = stem_from_output_path(output);
  cfg.html_export_title = title;

  chess::Board seed_board{};
  if (random_seed_board) {
    std::mt19937 rng(cfg.seed ? cfg.seed : std::random_device{}());
    int guard = 0;
    while (!chess::try_random_legal_board(rng, cfg.material, seed_board) && ++guard < 500000) {
    }
    if (guard >= 500000) {
      std::fprintf(stderr, "chess_ga: could not sample a random legal board (try another --seed)\n");
      return 1;
    }
    int start_fit = chess::count_mate_in_one(seed_board);
    std::fprintf(stderr, "Using random legal seed board (mate-in-ones=%d)\n", start_fit);
  } else {
    chess::set_baseline(seed_board, cfg.material);
  }

  if (baseline_only) {
    int n = chess::count_mate_in_one(seed_board);
    const bool default_material = cfg.material.white_queens == 9 && cfg.material.white_knights == 2 &&
                                  cfg.material.white_bishops == 2;
    std::printf("Mate-in-one count: %d%s\n", n,
                random_seed_board || !default_material ? "" : " (expected 105 for image baseline)");
    if (random_seed_board || !default_material) return 0;
    return n != 105 ? 2 : 0;
  }

  chess::GaResult res = chess::run_genetic_algorithm(cfg, seed_board);

  std::printf("Best mate-in-one count: %d (initial %d, plateau_gen %d)\n", res.best_fitness,
               res.initial_fitness, res.plateau_generation);

  chess::write_results_html(output, res.best_board, res.best_fitness, res.best_mates, title, cfg.material);
  std::printf("Wrote %s\n", output.c_str());

  return 0;
}
