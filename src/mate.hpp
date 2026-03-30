#pragma once

#include "board.hpp"

#include <vector>

namespace chess {

struct MateMove {
  int from;
  int to;
  uint8_t piece;
};

// Same semantics as solve() in chess.jsx, plus require quiet start (both kings safe).
// atk_white = true means White is the attacker (mate-in-one for White).
int count_mate_in_one(const Board& b, std::vector<MateMove>* list_out = nullptr);

}  // namespace chess
