#pragma once

#include "board.hpp"
#include "mate.hpp"

#include <random>
#include <string>
#include <vector>

namespace chess {

enum class CrossoverMode {
  /// Legacy: per-type sorted squares (weak building blocks).
  Sorted,
  /// Fixed semantic slots (9 queens in row-major order, then rooks, …) + collision repair.
  VectorRepair,
};

struct GaConfig {
  int population = 128;
  int generations = 500;
  unsigned seed = 0;  // 0 = random_device
  int elite = 2;
  /// Independent subpopulations per generation; each pool uses a distinct RNG stream for initialization.
  /// When >1, each pool receives offspring from cross-pool crossover among top pool_cross_elite individuals
  /// from every pool (see cross_pool_inject_fraction).
  int num_pools = 1;
  /// Top Y individuals per pool (by fitness) that contribute boards to the cross-pool crossover gene pool.
  int pool_cross_elite = 5;
  /// Fraction of each pool's next generation (excluding elite copies) filled with cross-pool offspring
  /// (always two parents from the union via do_crossover; not gated by crossover_rate). Ignored when num_pools < 2.
  double cross_pool_inject_fraction = 0.10;
  int tournament_k = 2;
  double crossover_rate = 0.45;
  double mutation_rate = 0.65;
  CrossoverMode crossover_mode = CrossoverMode::VectorRepair;
  bool report_each_generation = true;

  /// If global best does not improve for this many generations, inject immigrants / hyper-mutation. 0 = off.
  int stagnation_generations = 40;
  /// Fraction of population replaced with random legal boards on stagnation (worst-first).
  double immigrant_fraction = 0.12;
  /// Successful single-step mutations applied to a copy of best on stagnation; 0 = off.
  int hyper_mutation_steps = 4;

  /// Every M generations, replace diversity_fraction random individuals (not elites). 0 = off.
  int diversity_inject_every = 50;
  double diversity_fraction = 0.08;

  /// With multi_mutation_prob, apply 2..multi_mutation_max successful mutations per offspring.
  int multi_mutation_max = 4;
  double multi_mutation_prob = 0.4;

  bool local_search_enabled = true;
  /// Run local search on global best every N generations (and at end if >0). 0 = only at end.
  int local_search_every = 25;
  int local_search_budget = 25000;

  /// If non-empty, append one CSV line per generation: generation,pop_best,global_best
  std::string trace_csv_path;
  /// Write trace every N generations (default 1).
  int trace_every = 1;
  /// If true, fill GaResult trace vectors (small memory cost).
  bool collect_trace = false;
};

bool try_random_legal_board(std::mt19937& rng, Board& out);

struct GaResult {
  Board best_board{};
  int best_fitness = 0;
  std::vector<MateMove> best_mates;

  /// Mate-in-one count of the seed board before the GA loop.
  int initial_fitness = 0;
  /// First generation index (0-based) where no further strict improvement of global best occurs
  /// after the last improving generation; i.e. last_improving_generation + 1.
  int plateau_generation = 0;

  /// Per-generation snapshots (only if GaConfig::collect_trace).
  std::vector<int> trace_pop_best;
  std::vector<int> trace_global_best;
};

GaResult run_genetic_algorithm(const GaConfig& cfg, const Board& seed_board);

}  // namespace chess
