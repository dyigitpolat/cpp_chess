#pragma once

#include "board.hpp"
#include "mate.hpp"

#include <random>
#include <string>
#include <vector>

namespace chess {

struct GaConfig {
  int population = 128;
  int generations = 500;
  unsigned seed = 0;  // 0 = random_device
  int elite = 2;
  int tournament_k = 3;
  double crossover_rate = 0.45;
  double mutation_rate = 0.85;
  /// Print best mate-in-one count for each generation (stderr).
  bool report_each_generation = true;
};

/// Random legal position with fixed composition (9Q/2R/2N/2B/K vs k). May retry many times.
bool try_random_legal_board(std::mt19937& rng, Board& out);

struct GaResult {
  Board best_board{};
  int best_fitness = 0;
  std::vector<MateMove> best_mates;
};

GaResult run_genetic_algorithm(const GaConfig& cfg, const Board& seed_board);

}  // namespace chess
