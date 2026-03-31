#pragma once

#include "board.hpp"

#include <random>

namespace chess {

/// Greedy hill-climb: repeatedly move to a neighboring legal board with strictly higher mate count.
/// Budget caps total mate evaluations (excluding initial).
void local_search_hill_climb(Board& b, std::mt19937& rng, int max_mate_evals = 50000,
                             const MaterialSpec& material = MaterialSpec{});

}  // namespace chess
