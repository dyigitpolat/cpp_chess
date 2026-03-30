#pragma once

#include "board.hpp"
#include "mate.hpp"

#include <string>
#include <vector>

namespace chess {

void write_results_html(const std::string& path, const Board& board, int mate_count,
                        const std::vector<MateMove>& mates, const std::string& title);

}  // namespace chess
