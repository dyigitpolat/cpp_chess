#include "board.hpp"
#include "ga.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<int> parse_int_list(const char* s) {
  std::vector<int> v;
  std::stringstream ss(s);
  std::string part;
  while (std::getline(ss, part, ',')) {
    if (part.empty()) continue;
    v.push_back(std::stoi(part));
  }
  return v;
}

void apply_preset_fast(chess::GaConfig& cfg) {
  cfg.local_search_enabled = false;
  cfg.local_search_every = 0;
}

void apply_match_chess_ga(chess::GaConfig& cfg) {
  cfg = chess::GaConfig{};
  cfg.report_each_generation = false;
}

void print_usage() {
  std::cerr
      << "bench_ga — CSV benchmark over seed × population × generations\n\n"
         "Usage: bench_ga [options]\n\n"
         "Options:\n"
         "  --seeds LIST       Comma-separated seeds (default: 1,7,42,99,123)\n"
         "  --pops LIST        Comma-separated population sizes (default: 64,128,256,512)\n"
         "  --gens LIST        Comma-separated generation counts (default: 100,200,500)\n"
         "  --random-seed-board  Use random legal board instead of image baseline\n"
         "  --local-search       Enable memetic local search (default off for speed)\n"
         "  --match-chess-ga     Use same GaConfig defaults as chess_ga (LS on, etc.)\n"
         "  --preset fast        LS off (quick runs); --preset full same as --match-chess-ga\n"
         "  --tournament-k N     Tournament size (passed to GaConfig)\n"
         "  --pools N            Subpopulations (default 1); 2+ enables cross-pool elite crossover\n"
         "  --pool-cross-elite N Top elites per pool in cross-pool gene pool (default 5)\n"
         "  --cross-pool-inject-fraction F  Min fraction of pool from cross-pool offspring (default 0.1)\n"
         "  CSV columns:\n"
         "    seed,population,generations,best_fitness,seconds,plateau_generation,\n"
         "    local_search,initial_fitness\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<int> seeds = {1, 7, 42, 99, 123};
  std::vector<int> pops = {64, 128, 256, 512};
  std::vector<int> gens = {100, 200, 500};
  bool random_seed_board = false;
  chess::GaConfig cfg;
  cfg.report_each_generation = false;
  cfg.local_search_enabled = false;

  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "-h") || !std::strcmp(argv[i], "--help")) {
      print_usage();
      return 0;
    }
    if (!std::strcmp(argv[i], "--random-seed-board")) {
      random_seed_board = true;
      continue;
    }
    if (!std::strcmp(argv[i], "--local-search")) {
      cfg.local_search_enabled = true;
      continue;
    }
    if (!std::strcmp(argv[i], "--match-chess-ga")) {
      apply_match_chess_ga(cfg);
      continue;
    }
    if (!std::strcmp(argv[i], "--preset") && i + 1 < argc) {
      ++i;
      if (!std::strcmp(argv[i], "full"))
        apply_match_chess_ga(cfg);
      else if (!std::strcmp(argv[i], "fast"))
        apply_preset_fast(cfg);
      else {
        std::cerr << "Unknown --preset value (use fast or full)\n";
        return 1;
      }
      continue;
    }
    if (!std::strncmp(argv[i], "--seeds=", 8)) {
      seeds = parse_int_list(argv[i] + 8);
      continue;
    }
    if (!std::strncmp(argv[i], "--pops=", 7)) {
      pops = parse_int_list(argv[i] + 7);
      continue;
    }
    if (!std::strncmp(argv[i], "--gens=", 7)) {
      gens = parse_int_list(argv[i] + 7);
      continue;
    }
    if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) {
      seeds = parse_int_list(argv[++i]);
      continue;
    }
    if (!std::strcmp(argv[i], "--pops") && i + 1 < argc) {
      pops = parse_int_list(argv[++i]);
      continue;
    }
    if (!std::strcmp(argv[i], "--gens") && i + 1 < argc) {
      gens = parse_int_list(argv[++i]);
      continue;
    }
    if (!std::strcmp(argv[i], "--tournament-k") && i + 1 < argc) {
      cfg.tournament_k = std::atoi(argv[++i]);
      continue;
    }
    if (!std::strcmp(argv[i], "--pools") && i + 1 < argc) {
      cfg.num_pools = std::atoi(argv[++i]);
      continue;
    }
    if (!std::strcmp(argv[i], "--pool-cross-elite") && i + 1 < argc) {
      cfg.pool_cross_elite = std::atoi(argv[++i]);
      continue;
    }
    if (!std::strcmp(argv[i], "--cross-pool-inject-fraction") && i + 1 < argc) {
      cfg.cross_pool_inject_fraction = std::strtod(argv[++i], nullptr);
      continue;
    }
    std::cerr << "Unknown or unsupported argument: " << argv[i] << "\n";
    print_usage();
    return 1;
  }

  std::cout << "seed,population,generations,best_fitness,seconds,plateau_generation,local_search,"
               "initial_fitness\n";

  chess::Board seed_board{};

  for (unsigned sd : seeds) {
    cfg.seed = sd;
    if (random_seed_board) {
      std::mt19937 rng(sd);
      int guard = 0;
      while (!chess::try_random_legal_board(rng, seed_board) && ++guard < 500000) {
      }
      if (guard >= 500000) continue;
    } else {
      chess::set_baseline(seed_board);
    }

    for (int pop : pops) {
      for (int gen : gens) {
        cfg.population = pop;
        cfg.generations = gen;
        auto t0 = std::chrono::steady_clock::now();
        chess::GaResult res = chess::run_genetic_algorithm(cfg, seed_board);
        auto t1 = std::chrono::steady_clock::now();
        double sec = std::chrono::duration<double>(t1 - t0).count();
        std::cout << sd << "," << pop << "," << gen << "," << res.best_fitness << "," << sec << ","
                  << res.plateau_generation << "," << (cfg.local_search_enabled ? 1 : 0) << ","
                  << res.initial_fitness << "\n";
      }
    }
  }

  return 0;
}
