#include "local_search.hpp"

#include "mate.hpp"

#include <algorithm>
#include <vector>

namespace chess {

namespace {

void collect_neighbors(const Board& orig, const MaterialSpec& mat, std::vector<Board>& out) {
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
      if (valid_material(b, mat) && position_legal_quiet(b)) out.push_back(b);
    }
  }
  for (size_t i = 0; i < wsq.size(); ++i)
    for (size_t j = i + 1; j < wsq.size(); ++j) {
      Board b = orig;
      int a = wsq[i], c = wsq[j];
      std::swap(b[a], b[c]);
      if (valid_material(b, mat) && position_legal_quiet(b)) out.push_back(b);
    }
  int bk = find_king_sq(orig, false);
  if (bk >= 0) {
    for (int to : esq) {
      Board b = orig;
      b[bk] = EMPTY;
      b[to] = B_K;
      if (valid_material(b, mat) && position_legal_quiet(b)) out.push_back(b);
    }
  }
}

}  // namespace

void local_search_hill_climb(Board& b, std::mt19937& rng, int max_mate_evals, const MaterialSpec& material) {
  int evals = 0;
  std::vector<Board> neighbors;
  neighbors.reserve(2048);
  constexpr int k_neighbor_chunk = 32;

  while (evals < max_mate_evals) {
    int cur = count_mate_in_one(b);
    ++evals;
    collect_neighbors(b, material, neighbors);
    std::shuffle(neighbors.begin(), neighbors.end(), rng);

    const int nbr = static_cast<int>(neighbors.size());
    int pos = 0;
    int best_i = -1;
    while (pos < nbr && evals < max_mate_evals) {
      const int rem = max_mate_evals - evals;
      const int avail = nbr - pos;
      int chunk = avail;
      if (chunk > rem) chunk = rem;
      if (chunk > k_neighbor_chunk) chunk = k_neighbor_chunk;
      if (chunk <= 0) break;

      std::vector<int> chunk_fit(static_cast<size_t>(chunk));
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
      for (int j = 0; j < chunk; ++j)
        chunk_fit[static_cast<size_t>(j)] = count_mate_in_one(neighbors[static_cast<size_t>(pos + j)]);

      evals += chunk;
      for (int j = 0; j < chunk; ++j) {
        if (chunk_fit[static_cast<size_t>(j)] > cur) {
          best_i = pos + j;
          break;
        }
      }
      if (best_i >= 0) break;
      pos += chunk;
    }
    if (best_i < 0) break;
    b = neighbors[static_cast<size_t>(best_i)];
  }
}

}  // namespace chess
