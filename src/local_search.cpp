#include "local_search.hpp"

#include "mate.hpp"

#include <algorithm>
#include <vector>

namespace chess {

namespace {

void collect_neighbors(const Board& orig, std::vector<Board>& out) {
  out.clear();
  std::vector<int> wsq, esq;
  for (int s = 0; s < SQ_COUNT; ++s) {
    if (is_white(orig[s])) wsq.push_back(s);
    if (orig[s] == EMPTY) esq.push_back(s);
  }
  for (int from : wsq) {
    for (int to : esq) {
      if (from == to) continue;
      Board b = orig;
      uint8_t pc = b[from];
      b[from] = EMPTY;
      b[to] = pc;
      if (valid_material(b) && position_legal_quiet(b)) out.push_back(b);
    }
  }
  for (size_t i = 0; i < wsq.size(); ++i)
    for (size_t j = i + 1; j < wsq.size(); ++j) {
      Board b = orig;
      int a = wsq[i], c = wsq[j];
      std::swap(b[a], b[c]);
      if (valid_material(b) && position_legal_quiet(b)) out.push_back(b);
    }
  int bk = find_king_sq(orig, false);
  if (bk >= 0) {
    for (int to : esq) {
      Board b = orig;
      b[bk] = EMPTY;
      b[to] = B_K;
      if (valid_material(b) && position_legal_quiet(b)) out.push_back(b);
    }
  }
}

}  // namespace

void local_search_hill_climb(Board& b, std::mt19937& rng, int max_mate_evals) {
  int evals = 0;
  std::vector<Board> neighbors;
  neighbors.reserve(2048);

  while (evals < max_mate_evals) {
    int cur = count_mate_in_one(b);
    ++evals;
    collect_neighbors(b, neighbors);
    std::shuffle(neighbors.begin(), neighbors.end(), rng);

    int best_f = cur;
    int best_i = -1;
    for (size_t i = 0; i < neighbors.size() && evals < max_mate_evals; ++i) {
      int f = count_mate_in_one(neighbors[i]);
      ++evals;
      if (f > best_f) {
        best_f = f;
        best_i = static_cast<int>(i);
      }
    }
    if (best_i < 0) break;
    b = neighbors[static_cast<size_t>(best_i)];
  }
}

}  // namespace chess
