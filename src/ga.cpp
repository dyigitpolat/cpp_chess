#include "ga.hpp"

#include <algorithm>
#include <cstdio>
#include <array>
#include <numeric>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace chess {

namespace {

std::vector<uint8_t> piece_multiset() {
  std::vector<uint8_t> p;
  p.reserve(17);
  for (int i = 0; i < 9; ++i) p.push_back(W_Q);
  for (int i = 0; i < 2; ++i) p.push_back(W_R);
  for (int i = 0; i < 2; ++i) p.push_back(W_N);
  for (int i = 0; i < 2; ++i) p.push_back(W_B);
  p.push_back(W_K);
  p.push_back(B_K);
  return p;
}

void collect_white_squares(const Board& b, std::vector<int>& wsq) {
  wsq.clear();
  for (int i = 0; i < SQ_COUNT; ++i)
    if (is_white(b[i])) wsq.push_back(i);
}

void collect_empty_squares(const Board& b, std::vector<int>& esq) {
  esq.clear();
  for (int i = 0; i < SQ_COUNT; ++i)
    if (b[i] == EMPTY) esq.push_back(i);
}

bool mutate_board(Board& b, std::mt19937& rng) {
  std::vector<int> wsq, esq;
  collect_white_squares(b, wsq);
  collect_empty_squares(b, esq);
  if (wsq.empty()) return false;

  std::uniform_int_distribution<int> uwi(0, static_cast<int>(wsq.size()) - 1);
  int mode = std::uniform_int_distribution<int>(0, 2)(rng);

  if (mode == 0 && !esq.empty()) {
    int from = wsq[uwi(rng)];
    std::uniform_int_distribution<int> uei(0, static_cast<int>(esq.size()) - 1);
    int to = esq[uei(rng)];
    uint8_t pc = b[from];
    b[from] = EMPTY;
    b[to] = pc;
  } else if (mode == 1 && wsq.size() >= 2) {
    int i1 = uwi(rng);
    int i2 = uwi(rng);
    if (i1 == i2) i2 = (i2 + 1) % static_cast<int>(wsq.size());
    int a = wsq[i1], c = wsq[i2];
    std::swap(b[a], b[c]);
  } else {
    int bk = find_king_sq(b, false);
    if (bk < 0) return false;
    if (esq.empty()) return false;
    std::uniform_int_distribution<int> uei(0, static_cast<int>(esq.size()) - 1);
    int to = esq[uei(rng)];
    b[bk] = EMPTY;
    b[to] = B_K;
  }

  if (!valid_material(b) || !position_legal_quiet(b)) return false;
  return true;
}

// Per piece type: sorted square lists; i-th queen from A or B — preserves multiset.
Board crossover_merge(const Board& pa, const Board& pb, std::mt19937& rng) {
  Board child{};
  child.fill(EMPTY);
  std::uniform_int_distribution<int> pick(0, 1);

  auto place_type = [&](uint8_t t, int count) {
    std::vector<int> sa, sb;
    for (int s = 0; s < SQ_COUNT; ++s) {
      if (pa[s] == t) sa.push_back(s);
      if (pb[s] == t) sb.push_back(s);
    }
    std::sort(sa.begin(), sa.end());
    std::sort(sb.begin(), sb.end());
    if (static_cast<int>(sa.size()) != count || static_cast<int>(sb.size()) != count) return false;
    for (int i = 0; i < count; ++i) {
      int sq = pick(rng) ? sa[static_cast<size_t>(i)] : sb[static_cast<size_t>(i)];
      child[sq] = t;
    }
    return true;
  };

  if (!place_type(W_Q, 9) || !place_type(W_R, 2) || !place_type(W_N, 2) || !place_type(W_B, 2) ||
      !place_type(W_K, 1) || !place_type(B_K, 1))
    return pa;
  if (!valid_material(child) || !position_legal_quiet(child)) return pa;
  return child;
}

int tournament_pick(const std::vector<int>& fitness, std::mt19937& rng, int k) {
  std::uniform_int_distribution<int> ui(0, static_cast<int>(fitness.size()) - 1);
  int best = ui(rng);
  for (int j = 1; j < k; ++j) {
    int c = ui(rng);
    if (fitness[c] > fitness[best]) best = c;
  }
  return best;
}

}  // namespace

bool try_random_legal_board(std::mt19937& rng, Board& out) {
  auto pieces = piece_multiset();
  std::shuffle(pieces.begin(), pieces.end(), rng);
  std::array<int, 64> sq{};
  std::iota(sq.begin(), sq.end(), 0);
  std::shuffle(sq.begin(), sq.end(), rng);
  Board b{};
  b.fill(EMPTY);
  for (int i = 0; i < 17; ++i) b[sq[i]] = pieces[i];
  if (!valid_material(b) || !position_legal_quiet(b)) return false;
  out = b;
  return true;
}

GaResult run_genetic_algorithm(const GaConfig& cfg, const Board& seed_board) {
  GaResult result;
  GaConfig c = cfg;
  if (c.population < 2) c.population = 2;
  if (c.elite < 1) c.elite = 1;
  if (c.elite > c.population) c.elite = c.population;
  if (c.tournament_k < 2) c.tournament_k = 2;

  std::mt19937 rng(c.seed ? c.seed : std::random_device{}());

  std::vector<Board> pop(static_cast<size_t>(c.population));
  std::vector<int> fit(static_cast<size_t>(c.population));

  pop[0] = seed_board;
  for (int i = 1; i < c.population; ++i) {
    Board b = seed_board;
    int tries = 0;
    while (tries < 200 && !mutate_board(b, rng)) {
      b = seed_board;
      tries++;
    }
    if (tries >= 200) {
      int guard = 0;
      while (!try_random_legal_board(rng, b) && ++guard < 50000) {
      }
      if (guard >= 50000) b = seed_board;
    }
    pop[static_cast<size_t>(i)] = b;
  }

  result.best_board = seed_board;
  result.best_fitness = count_mate_in_one(seed_board, &result.best_mates);

  for (int gen = 0; gen < c.generations; ++gen) {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int i = 0; i < c.population; ++i) {
      fit[static_cast<size_t>(i)] = count_mate_in_one(pop[static_cast<size_t>(i)]);
    }

    int best_i = 0;
    for (int i = 1; i < c.population; ++i)
      if (fit[static_cast<size_t>(i)] > fit[static_cast<size_t>(best_i)]) best_i = i;

    if (fit[static_cast<size_t>(best_i)] > result.best_fitness) {
      result.best_fitness = fit[static_cast<size_t>(best_i)];
      result.best_board = pop[static_cast<size_t>(best_i)];
      count_mate_in_one(result.best_board, &result.best_mates);
    }

    if (c.report_each_generation) {
      int gen_best = fit[static_cast<size_t>(best_i)];
      std::fprintf(stderr, "generation %d: best=%d (overall so far: %d)\n", gen, gen_best,
                   result.best_fitness);
    }

    std::vector<Board> next;
    next.reserve(static_cast<size_t>(c.population));

    std::vector<std::pair<int, int>> order;
    order.reserve(static_cast<size_t>(c.population));
    for (int i = 0; i < c.population; ++i) order.push_back({fit[static_cast<size_t>(i)], i});
    std::sort(order.begin(), order.end(), [](auto& a, auto& b) { return a.first > b.first; });

    for (int e = 0; e < c.elite && e < c.population; ++e) next.push_back(pop[static_cast<size_t>(order[static_cast<size_t>(e)].second)]);

    std::uniform_real_distribution<double> prob(0.0, 1.0);
    while (static_cast<int>(next.size()) < c.population) {
      int p1 = tournament_pick(fit, rng, c.tournament_k);
      Board child = pop[static_cast<size_t>(p1)];
      if (prob(rng) < c.crossover_rate) {
        int p2 = tournament_pick(fit, rng, c.tournament_k);
        child = crossover_merge(pop[static_cast<size_t>(p1)], pop[static_cast<size_t>(p2)], rng);
      }
      if (prob(rng) < c.mutation_rate) {
        int attempts = 0;
        Board cand = child;
        while (attempts < 40 && !mutate_board(cand, rng)) {
          cand = child;
          ++attempts;
        }
        if (attempts < 40) child = cand;
      }
      next.push_back(child);
    }

    pop.swap(next);
  }

  return result;
}

}  // namespace chess
