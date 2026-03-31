#include "ga.hpp"

#include "html_export.hpp"
#include "local_search.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <fstream>
#include <numeric>
#include <string>
#include <vector>

namespace chess {

namespace {

/// Deterministic 32-bit seed for per-thread / per-slot RNGs (parallel GA).
inline unsigned mix_seed(unsigned a, unsigned b, unsigned c) {
  uint64_t x = static_cast<uint64_t>(a) * 0x9E3779B1ull;
  x ^= static_cast<uint64_t>(b) + 0x85EBCA6Bull + (x << 6) + (x >> 2);
  x ^= static_cast<uint64_t>(c) + 0xC2B2AE35ull + (x << 6) + (x >> 2);
  return static_cast<unsigned>(x ^ (x >> 32));
}

/// Directory prefix for snapshot paths (same folder as main `--output` when that path includes a directory).
inline std::string output_dir_prefix(const std::string& output_path) {
  const size_t slash = output_path.find_last_of("/\\");
  return slash == std::string::npos ? std::string() : output_path.substr(0, slash + 1);
}

void maybe_write_record_html(const GaConfig& c, int prev_best, int new_best, const Board& board,
                             const std::vector<MateMove>& mates) {
  if (new_best <= prev_best) return;
  if (new_best < c.record_snapshot_min_fitness) return;
  if (c.record_snapshot_stem.empty()) return;
  std::string path = output_dir_prefix(c.record_snapshot_output_path);
  if (!c.record_snapshot_prefix.empty())
    path += c.record_snapshot_prefix + "_" + std::to_string(new_best) + "_" + c.record_snapshot_stem + ".html";
  else
    path += std::to_string(new_best) + "_" + c.record_snapshot_stem + ".html";
  write_results_html(path, board, new_best, mates, c.html_export_title, c.material);
  std::fprintf(stderr, "new record %d — wrote %s\n", new_best, path.c_str());
}

void recompute_fitness_parallel(int P, int population, const std::vector<std::vector<Board>>& pops,
                                std::vector<std::vector<int>>& fits) {
  const int total_fit = P * population;
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
  for (int t = 0; t < total_fit; ++t) {
    int pid = t / population;
    int i = t % population;
    fits[static_cast<size_t>(pid)][static_cast<size_t>(i)] =
        count_mate_in_one(pops[static_cast<size_t>(pid)][static_cast<size_t>(i)]);
  }
}

std::vector<uint8_t> build_slot_types(const MaterialSpec& m) {
  std::vector<uint8_t> p;
  p.reserve(static_cast<size_t>(material_piece_count(m)));
  for (int i = 0; i < m.white_queens; ++i) p.push_back(W_Q);
  for (int i = 0; i < 2; ++i) p.push_back(W_R);
  for (int i = 0; i < m.white_knights; ++i) p.push_back(W_N);
  for (int i = 0; i < m.white_bishops; ++i) p.push_back(W_B);
  p.push_back(W_K);
  p.push_back(B_K);
  return p;
}

void board_to_slots(const Board& b, const MaterialSpec& m, std::vector<int>& sq) {
  const int n = material_piece_count(m);
  sq.assign(static_cast<size_t>(n), -1);
  int i = 0;
  for (int s = 0; s < SQ_COUNT; ++s)
    if (b[s] == W_Q) sq[static_cast<size_t>(i++)] = s;
  for (int s = 0; s < SQ_COUNT; ++s)
    if (b[s] == W_R) sq[static_cast<size_t>(i++)] = s;
  for (int s = 0; s < SQ_COUNT; ++s)
    if (b[s] == W_N) sq[static_cast<size_t>(i++)] = s;
  for (int s = 0; s < SQ_COUNT; ++s)
    if (b[s] == W_B) sq[static_cast<size_t>(i++)] = s;
  for (int s = 0; s < SQ_COUNT; ++s)
    if (b[s] == W_K) sq[static_cast<size_t>(i++)] = s;
  for (int s = 0; s < SQ_COUNT; ++s)
    if (b[s] == B_K) sq[static_cast<size_t>(i++)] = s;
}

bool repair_slots_to_board(const std::vector<int>& child_sq, std::mt19937& rng, const MaterialSpec& mat,
                           Board& out) {
  const std::vector<uint8_t> types = build_slot_types(mat);
  const int n = static_cast<int>(types.size());
  if (static_cast<int>(child_sq.size()) != n) return false;
  out.fill(EMPTY);
  std::vector<int> pending_idx;
  for (int i = 0; i < n; ++i) {
    int s = child_sq[static_cast<size_t>(i)];
    uint8_t t = types[static_cast<size_t>(i)];
    if (out[s] == EMPTY)
      out[s] = t;
    else
      pending_idx.push_back(i);
  }
  std::vector<int> empties;
  empties.reserve(64);
  for (int s = 0; s < SQ_COUNT; ++s)
    if (out[s] == EMPTY) empties.push_back(s);
  std::shuffle(empties.begin(), empties.end(), rng);
  for (size_t j = 0; j < pending_idx.size(); ++j) {
    if (j >= empties.size()) return false;
    int pi = pending_idx[j];
    out[empties[j]] = types[static_cast<size_t>(pi)];
  }
  return valid_material(out, mat) && position_legal_quiet(out);
}

Board crossover_vector_merge(const Board& pa, const Board& pb, std::mt19937& rng, const MaterialSpec& mat) {
  const int n = material_piece_count(mat);
  std::vector<int> sa(static_cast<size_t>(n)), sb(static_cast<size_t>(n)), child(static_cast<size_t>(n));
  board_to_slots(pa, mat, sa);
  board_to_slots(pb, mat, sb);
  std::uniform_int_distribution<int> pick(0, 1);
  for (int i = 0; i < n; ++i) child[static_cast<size_t>(i)] = pick(rng) ? sa[static_cast<size_t>(i)] : sb[static_cast<size_t>(i)];
  Board out{};
  if (!repair_slots_to_board(child, rng, mat, out)) return pa;
  return out;
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

bool mutate_board(Board& b, std::mt19937& rng, const MaterialSpec& mat) {
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

  if (!valid_material(b, mat) || !position_legal_quiet(b)) return false;
  return true;
}

bool apply_mutation_attempt(Board& child, std::mt19937& rng, const MaterialSpec& mat) {
  int attempts = 0;
  Board cand = child;
  while (attempts < 80 && !mutate_board(cand, rng, mat)) {
    cand = child;
    ++attempts;
  }
  if (attempts < 80) {
    child = cand;
    return true;
  }
  return false;
}

Board crossover_merge_sorted(const Board& pa, const Board& pb, std::mt19937& rng, const MaterialSpec& mat) {
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

  if (!place_type(W_Q, mat.white_queens) || !place_type(W_R, 2) || !place_type(W_N, mat.white_knights) ||
      !place_type(W_B, mat.white_bishops) || !place_type(W_K, 1) || !place_type(B_K, 1))
    return pa;
  if (!valid_material(child, mat) || !position_legal_quiet(child)) return pa;
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

Board do_crossover(const Board& pa, const Board& pb, CrossoverMode mode, std::mt19937& rng,
                     const MaterialSpec& mat) {
  if (mode == CrossoverMode::VectorRepair) return crossover_vector_merge(pa, pb, rng, mat);
  return crossover_merge_sorted(pa, pb, rng, mat);
}

}  // namespace

bool try_random_legal_board(std::mt19937& rng, const MaterialSpec& mat, Board& out) {
  std::vector<uint8_t> pieces = build_slot_types(mat);
  std::shuffle(pieces.begin(), pieces.end(), rng);
  std::array<int, 64> sq{};
  std::iota(sq.begin(), sq.end(), 0);
  std::shuffle(sq.begin(), sq.end(), rng);
  Board b{};
  b.fill(EMPTY);
  const int n = static_cast<int>(pieces.size());
  for (int i = 0; i < n; ++i) b[sq[static_cast<size_t>(i)]] = pieces[static_cast<size_t>(i)];
  if (!valid_material(b, mat) || !position_legal_quiet(b)) return false;
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
  if (c.num_pools < 1) c.num_pools = 1;
  if (c.pool_cross_elite < 1) c.pool_cross_elite = 1;
  if (c.pool_cross_elite > c.population) c.pool_cross_elite = c.population;
  if (c.cross_pool_inject_fraction < 0.0) c.cross_pool_inject_fraction = 0.0;
  if (c.cross_pool_inject_fraction > 1.0) c.cross_pool_inject_fraction = 1.0;
  if (c.immigrant_fraction < 0.0) c.immigrant_fraction = 0.0;
  if (c.immigrant_fraction > 1.0) c.immigrant_fraction = 1.0;
  if (c.diversity_fraction < 0.0) c.diversity_fraction = 0.0;
  if (c.diversity_fraction > 1.0) c.diversity_fraction = 1.0;
  if (c.multi_mutation_max < 2) c.multi_mutation_max = 2;

  const int P = c.num_pools;
  unsigned base_seed = c.seed ? c.seed : std::random_device{}();
  std::mt19937 rng(base_seed);

  std::vector<std::vector<Board>> pops(static_cast<size_t>(P),
                                         std::vector<Board>(static_cast<size_t>(c.population)));
  std::vector<std::vector<int>> fits(static_cast<size_t>(P),
                                     std::vector<int>(static_cast<size_t>(c.population)));

  for (int pid = 0; pid < P; ++pid) pops[static_cast<size_t>(pid)][0] = seed_board;

#ifdef _OPENMP
#pragma omp parallel for collapse(2) schedule(dynamic, 1)
#endif
  for (int pid = 0; pid < P; ++pid) {
    for (int i = 1; i < c.population; ++i) {
      unsigned seed =
          mix_seed(static_cast<unsigned>(base_seed), static_cast<unsigned>(pid) * 1000003u,
                   static_cast<unsigned>(i) * 0xFEDCBA98u + 0x12345678u);
      std::mt19937 rng_pool(seed);
      Board b = seed_board;
      int tries = 0;
      while (tries < 200 && !mutate_board(b, rng_pool, c.material)) {
        b = seed_board;
        tries++;
      }
      if (tries >= 200) {
        int guard = 0;
        while (!try_random_legal_board(rng_pool, c.material, b) && ++guard < 50000) {
        }
        if (guard >= 50000) b = seed_board;
      }
      pops[static_cast<size_t>(pid)][static_cast<size_t>(i)] = b;
    }
  }

  result.best_board = seed_board;
  result.best_fitness = count_mate_in_one(seed_board, &result.best_mates);
  result.initial_fitness = result.best_fitness;

  int last_improve_gen = -1;
  int prev_global_end = result.best_fitness;

  std::ofstream trace_out;
  if (!c.trace_csv_path.empty()) {
    trace_out.open(c.trace_csv_path);
    if (trace_out) trace_out << "generation,pop_best,global_best\n";
  }

  int stagnation_counter = 0;

  const int total_fit = P * c.population;
  int inject_count = 0;
  if (P >= 2) {
    inject_count = static_cast<int>(std::ceil(c.cross_pool_inject_fraction * static_cast<double>(c.population)));
    inject_count = std::max(1, inject_count);
    inject_count = std::min(inject_count, c.population - c.elite);
  }

  for (int gen = 0; gen < c.generations; ++gen) {
    const auto gen_t0 = std::chrono::steady_clock::now();
    const int best_before_gen = result.best_fitness;

    recompute_fitness_parallel(P, c.population, pops, fits);

    int best_pid = 0;
    int best_i = 0;
    for (int pid = 0; pid < P; ++pid) {
      for (int i = 0; i < c.population; ++i) {
        if (fits[static_cast<size_t>(pid)][static_cast<size_t>(i)] >
            fits[static_cast<size_t>(best_pid)][static_cast<size_t>(best_i)]) {
          best_pid = pid;
          best_i = i;
        }
      }
    }

    if (fits[static_cast<size_t>(best_pid)][static_cast<size_t>(best_i)] > result.best_fitness) {
      const int prev_best = result.best_fitness;
      result.best_fitness = fits[static_cast<size_t>(best_pid)][static_cast<size_t>(best_i)];
      result.best_board = pops[static_cast<size_t>(best_pid)][static_cast<size_t>(best_i)];
      count_mate_in_one(result.best_board, &result.best_mates);
      maybe_write_record_html(c, prev_best, result.best_fitness, result.best_board, result.best_mates);
    }

    if (result.best_fitness > best_before_gen)
      stagnation_counter = 0;
    else
      ++stagnation_counter;

    int gen_pop_best = 0;
    for (int pid = 0; pid < P; ++pid) {
      int pb = 0;
      for (int i = 1; i < c.population; ++i) {
        if (fits[static_cast<size_t>(pid)][static_cast<size_t>(i)] >
            fits[static_cast<size_t>(pid)][static_cast<size_t>(pb)])
          pb = i;
      }
      gen_pop_best = std::max(gen_pop_best, fits[static_cast<size_t>(pid)][static_cast<size_t>(pb)]);
    }

    // Diversity injection (periodic random immigrants among non-elite), per pool
    if (c.diversity_inject_every > 0 && gen > 0 && gen % c.diversity_inject_every == 0 &&
        c.diversity_fraction > 0.0 && c.population > c.elite) {
      for (int pid = 0; pid < P; ++pid) {
        auto& pop = pops[static_cast<size_t>(pid)];
        auto& fit = fits[static_cast<size_t>(pid)];
        std::vector<std::pair<int, int>> order_div;
        for (int i = 0; i < c.population; ++i) order_div.push_back({fit[static_cast<size_t>(i)], i});
        std::sort(order_div.begin(), order_div.end(),
                  [](auto& a, auto& b) { return a.first < b.first; });
        int ninj = static_cast<int>(c.diversity_fraction * c.population);
        if (ninj < 1) ninj = 1;
        const int worst_span = c.population - c.elite;
        std::uniform_int_distribution<int> uidx(0, worst_span - 1);
        std::vector<int> div_target(static_cast<size_t>(ninj));
        for (int j = 0; j < ninj; ++j)
          div_target[static_cast<size_t>(j)] = order_div[static_cast<size_t>(uidx(rng))].second;
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic, 1)
#endif
        for (int j = 0; j < ninj; ++j) {
          unsigned im_seed = mix_seed(static_cast<unsigned>(base_seed),
                                      static_cast<unsigned>(gen) * 0x10001u + static_cast<unsigned>(pid),
                                      static_cast<unsigned>(j) ^ 0xD3D3D3u);
          std::mt19937 rng_im(im_seed);
          Board nb{};
          int guard = 0;
          while (!try_random_legal_board(rng_im, c.material, nb) && ++guard < 100000) {
          }
          if (guard < 100000) pop[static_cast<size_t>(div_target[static_cast<size_t>(j)])] = nb;
        }
      }
      recompute_fitness_parallel(P, c.population, pops, fits);
      best_pid = 0;
      best_i = 0;
      for (int pid = 0; pid < P; ++pid) {
        for (int i = 0; i < c.population; ++i) {
          if (fits[static_cast<size_t>(pid)][static_cast<size_t>(i)] >
              fits[static_cast<size_t>(best_pid)][static_cast<size_t>(best_i)]) {
            best_pid = pid;
            best_i = i;
          }
        }
      }
      if (fits[static_cast<size_t>(best_pid)][static_cast<size_t>(best_i)] > result.best_fitness) {
        const int prev_best = result.best_fitness;
        result.best_fitness = fits[static_cast<size_t>(best_pid)][static_cast<size_t>(best_i)];
        result.best_board = pops[static_cast<size_t>(best_pid)][static_cast<size_t>(best_i)];
        count_mate_in_one(result.best_board, &result.best_mates);
        maybe_write_record_html(c, prev_best, result.best_fitness, result.best_board, result.best_mates);
      }
    }

    // Stagnation: replace worst + hyper-mutate best, per pool
    if (c.stagnation_generations > 0 && stagnation_counter >= c.stagnation_generations) {
      for (int pid = 0; pid < P; ++pid) {
        auto& pop = pops[static_cast<size_t>(pid)];
        auto& fit = fits[static_cast<size_t>(pid)];
        std::vector<std::pair<int, int>> order;
        for (int i = 0; i < c.population; ++i) order.push_back({fit[static_cast<size_t>(i)], i});
        std::sort(order.begin(), order.end(), [](auto& a, auto& b) { return a.first < b.first; });

        int nw = static_cast<int>(c.immigrant_fraction * c.population);
        if (nw < 1) nw = 1;
        const int n_stag_imm = std::min(nw, c.population);
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
        for (int j = 0; j < n_stag_imm; ++j) {
          int idx = order[static_cast<size_t>(j)].second;
          unsigned im_seed = mix_seed(static_cast<unsigned>(base_seed),
                                        static_cast<unsigned>(gen) * 0x20003u + static_cast<unsigned>(pid),
                                        static_cast<unsigned>(j) ^ 0x5A5A5A5Au);
          std::mt19937 rng_im(im_seed);
          Board nb{};
          int guard = 0;
          while (!try_random_legal_board(rng_im, c.material, nb) && ++guard < 100000) {
          }
          if (guard < 100000) pop[static_cast<size_t>(idx)] = nb;
        }

        if (c.hyper_mutation_steps > 0) {
          Board hyper = result.best_board;
          for (int h = 0; h < c.hyper_mutation_steps; ++h) {
            Board cand = hyper;
            if (mutate_board(cand, rng, c.material))
              hyper = cand;
          }
          if (valid_material(hyper, c.material) && position_legal_quiet(hyper) && c.population > c.elite) {
            std::uniform_int_distribution<int> urep(0, c.population - 1 - c.elite);
            int replace_idx = order[static_cast<size_t>(urep(rng))].second;
            pop[static_cast<size_t>(replace_idx)] = hyper;
          }
        }
      }

      stagnation_counter = 0;

      recompute_fitness_parallel(P, c.population, pops, fits);
      best_pid = 0;
      best_i = 0;
      for (int pid = 0; pid < P; ++pid) {
        for (int i = 0; i < c.population; ++i) {
          if (fits[static_cast<size_t>(pid)][static_cast<size_t>(i)] >
              fits[static_cast<size_t>(best_pid)][static_cast<size_t>(best_i)]) {
            best_pid = pid;
            best_i = i;
          }
        }
      }
      if (fits[static_cast<size_t>(best_pid)][static_cast<size_t>(best_i)] > result.best_fitness) {
        const int prev_best = result.best_fitness;
        result.best_fitness = fits[static_cast<size_t>(best_pid)][static_cast<size_t>(best_i)];
        result.best_board = pops[static_cast<size_t>(best_pid)][static_cast<size_t>(best_i)];
        count_mate_in_one(result.best_board, &result.best_mates);
        maybe_write_record_html(c, prev_best, result.best_fitness, result.best_board, result.best_mates);
      }
    }

    // Memetic local search on global best
    if (c.local_search_enabled) {
      bool run_ls = (c.local_search_every > 0 && (gen + 1) % c.local_search_every == 0) ||
                    (c.local_search_every <= 0 && gen + 1 == c.generations);
      if (run_ls) {
        Board ls = result.best_board;
        local_search_hill_climb(ls, rng, c.local_search_budget, c.material);
        int lf = count_mate_in_one(ls);
        if (lf > result.best_fitness) {
          const int prev_best = result.best_fitness;
          result.best_fitness = lf;
          result.best_board = ls;
          count_mate_in_one(result.best_board, &result.best_mates);
          maybe_write_record_html(c, prev_best, result.best_fitness, result.best_board, result.best_mates);
          pops[static_cast<size_t>(best_pid)][static_cast<size_t>(best_i)] = ls;
          fits[static_cast<size_t>(best_pid)][static_cast<size_t>(best_i)] = lf;
        }
      }
    }

    if (result.best_fitness > prev_global_end) last_improve_gen = gen;
    prev_global_end = result.best_fitness;

    if (c.collect_trace) {
      result.trace_pop_best.push_back(gen_pop_best);
      result.trace_global_best.push_back(result.best_fitness);
    }
    const int te = c.trace_every < 1 ? 1 : c.trace_every;
    if (trace_out && gen % te == 0) trace_out << gen << "," << gen_pop_best << "," << result.best_fitness << "\n";

    // Union of top pool_cross_elite boards per pool for cross-pool crossover
    std::vector<Board> ue_b;
    std::vector<int> ue_f;
    ue_b.reserve(static_cast<size_t>(P * c.pool_cross_elite));
    for (int pid = 0; pid < P; ++pid) {
      std::vector<std::pair<int, int>> order;
      order.reserve(static_cast<size_t>(c.population));
      for (int i = 0; i < c.population; ++i)
        order.push_back({fits[static_cast<size_t>(pid)][static_cast<size_t>(i)], i});
      std::sort(order.begin(), order.end(),
                [](auto& a, auto& b) { return a.first > b.first; });
      int take = std::min(c.pool_cross_elite, c.population);
      for (int e = 0; e < take; ++e) {
        ue_b.push_back(pops[static_cast<size_t>(pid)][static_cast<size_t>(order[static_cast<size_t>(e)].second)]);
        ue_f.push_back(order[static_cast<size_t>(e)].first);
      }
    }

    for (int pid = 0; pid < P; ++pid) {
      auto& pop = pops[static_cast<size_t>(pid)];
      auto& fit = fits[static_cast<size_t>(pid)];

      std::vector<Board> next(static_cast<size_t>(c.population));

      std::vector<std::pair<int, int>> order;
      order.reserve(static_cast<size_t>(c.population));
      for (int i = 0; i < c.population; ++i) order.push_back({fit[static_cast<size_t>(i)], i});
      std::sort(order.begin(), order.end(), [](auto& a, auto& b) { return a.first > b.first; });

      for (int e = 0; e < c.elite && e < c.population; ++e)
        next[static_cast<size_t>(e)] = pop[static_cast<size_t>(order[static_cast<size_t>(e)].second)];

      int fill_start = c.elite;
      if (P >= 2 && inject_count > 0 && !ue_b.empty()) {
        int tk_u = std::min(c.tournament_k, std::max(2, static_cast<int>(ue_f.size())));
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
        for (int inj = 0; inj < inject_count; ++inj) {
          unsigned loc_seed =
              mix_seed(static_cast<unsigned>(base_seed),
                       static_cast<unsigned>(gen) * 0x1000003u + static_cast<unsigned>(pid),
                       static_cast<unsigned>(inj) + 0x70000000u);
          std::mt19937 loc(loc_seed);
          std::uniform_real_distribution<double> prob_loc(0.0, 1.0);
          int pi1 = tournament_pick(ue_f, loc, tk_u);
          int pi2 = tournament_pick(ue_f, loc, tk_u);
          if (ue_f.size() >= 2) {
            for (int guard = 0; guard < 32 && pi2 == pi1; ++guard)
              pi2 = tournament_pick(ue_f, loc, tk_u);
          }
          Board child = do_crossover(ue_b[static_cast<size_t>(pi1)], ue_b[static_cast<size_t>(pi2)], c.crossover_mode,
                                     loc, c.material);
          if (prob_loc(loc) < c.mutation_rate) {
            int max_steps = 1;
            if (prob_loc(loc) < c.multi_mutation_prob) {
              std::uniform_int_distribution<int> ust(2, c.multi_mutation_max);
              max_steps = ust(loc);
            }
            for (int step = 0; step < max_steps; ++step) {
              if (!apply_mutation_attempt(child, loc, c.material)) break;
            }
          }
          next[static_cast<size_t>(c.elite + inj)] = child;
        }
        fill_start = c.elite + inject_count;
      }

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
      for (int s = fill_start; s < c.population; ++s) {
        unsigned loc_seed =
            mix_seed(static_cast<unsigned>(base_seed),
                     static_cast<unsigned>(gen) * 0x3000007u + static_cast<unsigned>(pid),
                     static_cast<unsigned>(s) + 0x80000000u);
        std::mt19937 loc(loc_seed);
        std::uniform_real_distribution<double> prob_loc(0.0, 1.0);
        int p1 = tournament_pick(fit, loc, c.tournament_k);
        Board child = pop[static_cast<size_t>(p1)];
        if (prob_loc(loc) < c.crossover_rate) {
          int p2 = tournament_pick(fit, loc, c.tournament_k);
          child = do_crossover(pop[static_cast<size_t>(p1)], pop[static_cast<size_t>(p2)], c.crossover_mode, loc,
                               c.material);
        }
        if (prob_loc(loc) < c.mutation_rate) {
          int max_steps = 1;
          if (prob_loc(loc) < c.multi_mutation_prob) {
            std::uniform_int_distribution<int> ust(2, c.multi_mutation_max);
            max_steps = ust(loc);
          }
          for (int step = 0; step < max_steps; ++step) {
            if (!apply_mutation_attempt(child, loc, c.material)) break;
          }
        }
        next[static_cast<size_t>(s)] = child;
      }

      pop.swap(next);
    }

    const double gen_sec =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - gen_t0).count();
    if (c.report_each_generation) {
      std::fprintf(stderr, "generation %d: best=%d (overall so far: %d)  %.2f s\n", gen, gen_pop_best,
                   result.best_fitness, gen_sec);
    } else if (c.report_generation_time) {
      std::fprintf(stderr, "generation %d: %.2f s\n", gen, gen_sec);
    }
  }

  result.plateau_generation = last_improve_gen + 1;

  return result;
}

}  // namespace chess
