#include "board.hpp"

#include <bit>
#include <cstring>
#include <vector>

// Performance: this engine assumes Black has only a king (mate-in-one GA). See is_attacked / in_check.

namespace chess {

bool in_bounds(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }

int sq_from_algebraic(const char* alg) {
  if (!alg || !alg[0] || !alg[1]) return -1;
  int c = alg[0] - 'a';
  int rank = alg[1] - '0';
  if (rank < 1 || rank > 8) return -1;
  if (c < 0 || c > 7) return -1;
  int r = 8 - rank;
  return rc_to_sq(r, c);
}

std::string algebraic_from_sq(int sq) {
  static const char* files = "abcdefgh";
  int r = sq_r(sq);
  int c = sq_c(sq);
  int rank = 8 - r;
  std::string s;
  s += files[c];
  s += char('0' + rank);
  return s;
}

static char piece_type_upper(uint8_t p) {
  switch (p) {
    case W_K:
      return 'K';
    case W_Q:
      return 'Q';
    case W_R:
      return 'R';
    case W_B:
      return 'B';
    case W_N:
      return 'N';
    default:
      return 0;
  }
}

static void attacks_from_impl(const Board& b, int r, int c, std::vector<int>& out) {
  uint8_t p = b[rc_to_sq(r, c)];
  if (p == EMPTY) return;
  out.clear();

  char t = piece_type_upper(p);
  if (t == 'N') {
    static const int dr[] = {-2, -2, -1, -1, 1, 1, 2, 2};
    static const int dc[] = {-1, 1, -2, 2, -2, 2, -1, 1};
    for (int i = 0; i < 8; ++i) {
      int nr = r + dr[i], nc = c + dc[i];
      if (in_bounds(nr, nc)) out.push_back(rc_to_sq(nr, nc));
    }
    return;
  }
  if (t == 'K') {
    for (int dr = -1; dr <= 1; ++dr)
      for (int dc = -1; dc <= 1; ++dc) {
        if (dr == 0 && dc == 0) continue;
        int nr = r + dr, nc = c + dc;
        if (in_bounds(nr, nc)) out.push_back(rc_to_sq(nr, nc));
      }
    return;
  }

  static const int dirs_b[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
  static const int dirs_r[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
  static const int dirs_q[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};

  const int (*dirs)[2] = nullptr;
  int nd = 0;
  if (t == 'B') {
    dirs = dirs_b;
    nd = 4;
  } else if (t == 'R') {
    dirs = dirs_r;
    nd = 4;
  } else if (t == 'Q') {
    dirs = dirs_q;
    nd = 8;
  } else {
    return;
  }

  for (int i = 0; i < nd; ++i) {
    int dr = dirs[i][0], dc = dirs[i][1];
    int nr = r + dr, nc = c + dc;
    while (in_bounds(nr, nc)) {
      out.push_back(rc_to_sq(nr, nc));
      if (b[rc_to_sq(nr, nc)] != EMPTY) break;
      nr += dr;
      nc += dc;
    }
  }
}

void attacks_from(const Board& b, int sq, std::vector<int>& out) {
  attacks_from_impl(b, sq_r(sq), sq_c(sq), out);
}

// Target-only check for is_attacked(by_white): avoids building a vector per white piece.
static bool white_piece_attacks_sq(const Board& b, int from_sq, int target_sq) {
  const int r = sq_r(from_sq);
  const int c = sq_c(from_sq);
  const uint8_t p = b[static_cast<size_t>(from_sq)];
  if (p == EMPTY) return false;

  const char t = piece_type_upper(p);
  if (t == 'N') {
    static const int dr[] = {-2, -2, -1, -1, 1, 1, 2, 2};
    static const int dc[] = {-1, 1, -2, 2, -2, 2, -1, 1};
    for (int i = 0; i < 8; ++i) {
      const int nr = r + dr[i], nc = c + dc[i];
      if (in_bounds(nr, nc) && rc_to_sq(nr, nc) == target_sq) return true;
    }
    return false;
  }
  if (t == 'K') {
    for (int dr = -1; dr <= 1; ++dr)
      for (int dc = -1; dc <= 1; ++dc) {
        if (dr == 0 && dc == 0) continue;
        const int nr = r + dr, nc = c + dc;
        if (in_bounds(nr, nc) && rc_to_sq(nr, nc) == target_sq) return true;
      }
    return false;
  }

  static const int dirs_b[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
  static const int dirs_r[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
  static const int dirs_q[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};

  const int (*dirs)[2] = nullptr;
  int nd = 0;
  if (t == 'B') {
    dirs = dirs_b;
    nd = 4;
  } else if (t == 'R') {
    dirs = dirs_r;
    nd = 4;
  } else if (t == 'Q') {
    dirs = dirs_q;
    nd = 8;
  } else {
    return false;
  }

  for (int i = 0; i < nd; ++i) {
    const int dr = dirs[i][0], dc = dirs[i][1];
    int nr = r + dr, nc = c + dc;
    while (in_bounds(nr, nc)) {
      const int sq = rc_to_sq(nr, nc);
      if (sq == target_sq) return true;
      if (b[static_cast<size_t>(sq)] != EMPTY) break;
      nr += dr;
      nc += dc;
    }
  }
  return false;
}

/// True if the two squares are different and king-adjacent (Black has only K in this puzzle).
static bool kings_adjacent(int sq_a, int sq_b) {
  if (sq_a == sq_b) return false;
  int dr = sq_r(sq_a) - sq_r(sq_b);
  if (dr < 0) dr = -dr;
  int dc = sq_c(sq_a) - sq_c(sq_b);
  if (dc < 0) dc = -dc;
  return dr <= 1 && dc <= 1;
}

uint64_t white_piece_occ_mask(const Board& b) {
  uint64_t m = 0;
  for (int i = 0; i < SQ_COUNT; ++i) {
    const uint8_t p = b[static_cast<size_t>(i)];
    if (p != EMPTY && is_white(p)) m |= (1ULL << static_cast<unsigned>(i));
  }
  return m;
}

// True if some white piece attacks target_sq; only iterates set bits in white_mask.
static bool white_attacks_target_sq(const Board& b, int target_sq, uint64_t white_mask) {
  uint64_t wm = white_mask;
  while (wm != 0) {
    const int sq = static_cast<int>(std::countr_zero(wm));
    wm &= wm - 1;
    if (white_piece_attacks_sq(b, sq, target_sq)) return true;
  }
  return false;
}

bool is_attacked(const Board& b, int target_sq, bool by_white) {
  if (!by_white) {
    // Only the black king can attack (no other Black material in this program).
    const int bksq = find_king_sq(b, false);
    if (bksq < 0) return false;
    return kings_adjacent(bksq, target_sq);
  }
  return white_attacks_target_sq(b, target_sq, white_piece_occ_mask(b));
}

int find_king_sq(const Board& b, bool white_king) {
  uint8_t want = white_king ? W_K : B_K;
  for (int i = 0; i < SQ_COUNT; ++i)
    if (b[i] == want) return i;
  return -1;
}

bool in_check(const Board& b, bool white_side) {
  const int ksq = find_king_sq(b, white_side);
  if (ksq < 0) return false;
  if (white_side) {
    // White king in check: only Black's king can give check here.
    const int bksq = find_king_sq(b, false);
    if (bksq < 0) return false;
    return kings_adjacent(bksq, ksq);
  }
  return is_attacked(b, ksq, true);
}

bool black_king_in_check(const Board& b, int black_king_sq, uint64_t white_mask) {
  if (black_king_sq < 0) return false;
  return white_attacks_target_sq(b, black_king_sq, white_mask);
}

bool black_king_in_check(const Board& b, int black_king_sq) {
  if (black_king_sq < 0) return false;
  return black_king_in_check(b, black_king_sq, white_piece_occ_mask(b));
}

bool white_king_in_check_vs_lone_black(const Board& b, int white_king_sq, int black_king_sq) {
  (void)b;
  if (white_king_sq < 0 || black_king_sq < 0) return false;
  return kings_adjacent(black_king_sq, white_king_sq);
}

void pseudo_moves(const Board& b, bool white_side, std::vector<Move>& moves) {
  moves.clear();
  thread_local std::vector<int> atks;
  atks.reserve(64);
  if (!white_side) {
    // Black has only the king in this engine; avoid scanning all 64 squares.
    const int sq = find_king_sq(b, false);
    if (sq < 0) return;
    const uint8_t p = b[static_cast<size_t>(sq)];
    if (p != B_K) return;
    const bool is_w = false;
    attacks_from_impl(b, sq_r(sq), sq_c(sq), atks);
    for (int dest : atks) {
      const uint8_t dp = b[static_cast<size_t>(dest)];
      if (dp == EMPTY || is_white(dp) != is_w) moves.push_back(Move{sq, dest});
    }
    return;
  }
  uint64_t wm = white_piece_occ_mask(b);
  while (wm != 0) {
    const int sq = static_cast<int>(std::countr_zero(wm));
    wm &= wm - 1;
    const bool is_w = true;
    attacks_from_impl(b, sq_r(sq), sq_c(sq), atks);
    for (int dest : atks) {
      const uint8_t dp = b[static_cast<size_t>(dest)];
      if (dp == EMPTY || is_white(dp) != is_w) moves.push_back(Move{sq, dest});
    }
  }
}

void apply_move(const Board& b, Move m, Board& out) {
  out = b;
  out[m.to] = out[m.from];
  out[m.from] = EMPTY;
}

bool has_any_legal_move(const Board& b, bool white_side) {
  std::vector<Move> moves;
  moves.reserve(256);
  pseudo_moves(b, white_side, moves);
  Board nb;
  for (const Move& m : moves) {
    apply_move(b, m, nb);
    if (!in_check(nb, white_side)) return true;
  }
  return false;
}

bool black_king_has_legal_move(const Board& b, int black_king_sq) {
  if (black_king_sq < 0) return false;
  const int kr = sq_r(black_king_sq);
  const int kc = sq_c(black_king_sq);
  Board nb;
  for (int dr = -1; dr <= 1; ++dr)
    for (int dc = -1; dc <= 1; ++dc) {
      if (dr == 0 && dc == 0) continue;
      const int nr = kr + dr, nc = kc + dc;
      if (!in_bounds(nr, nc)) continue;
      const int to = rc_to_sq(nr, nc);
      const uint8_t target = b[static_cast<size_t>(to)];
      if (target != EMPTY && is_black(target)) continue;
      const Move m{black_king_sq, to};
      apply_move(b, m, nb);
      if (!in_check(nb, false)) return true;
    }
  return false;
}

bool black_king_has_legal_move(const Board& b) {
  return black_king_has_legal_move(b, find_king_sq(b, false));
}

bool position_legal_quiet(const Board& b) {
  int wk = find_king_sq(b, true);
  int bk = find_king_sq(b, false);
  if (wk < 0 || bk < 0) return false;
  if (in_check(b, true)) return false;
  if (in_check(b, false)) return false;
  return true;
}

int material_piece_count(const MaterialSpec& m) {
  return m.white_queens + 2 + m.white_knights + m.white_bishops + 2;
}

std::string composition_string(const MaterialSpec& m) {
  std::string s = std::to_string(m.white_queens);
  s += "Q 2R ";
  s += std::to_string(m.white_knights);
  s += "N ";
  s += std::to_string(m.white_bishops);
  s += "B + K vs k";
  return s;
}

bool valid_material(const Board& b, const MaterialSpec& m) {
  if (m.white_queens < 1 || m.white_queens > 9 || m.white_knights < 1 || m.white_knights > 2 ||
      m.white_bishops < 1 || m.white_bishops > 2)
    return false;
  int nq = 0, nr = 0, nb = 0, nn = 0, nwk = 0, nbk = 0;
  for (uint8_t p : b) {
    switch (p) {
      case EMPTY:
        break;
      case W_K:
        ++nwk;
        break;
      case W_Q:
        ++nq;
        break;
      case W_R:
        ++nr;
        break;
      case W_B:
        ++nb;
        break;
      case W_N:
        ++nn;
        break;
      case B_K:
        ++nbk;
        break;
      default:
        return false;
    }
  }
  return nq == m.white_queens && nr == 2 && nb == m.white_bishops && nn == m.white_knights && nwk == 1 &&
         nbk == 1;
}

void clear_board(Board& b) { b.fill(EMPTY); }

void set_baseline(Board& b) {
  clear_board(b);
  auto place = [&](const char* alg, uint8_t pc) {
    int s = sq_from_algebraic(alg);
    if (s >= 0) b[s] = pc;
  };
  place("h1", W_K);
  place("b8", W_Q);
  place("e7", W_Q);
  place("c6", W_Q);
  place("a5", W_Q);
  place("g5", W_Q);
  place("a3", W_Q);
  place("b3", W_Q);
  place("c1", W_Q);
  place("e1", W_Q);
  place("h4", W_R);
  place("f2", W_R);
  place("h6", W_N);
  place("g3", W_N);
  place("g4", W_B);
  place("g1", W_B);
  place("d4", B_K);
}

void set_baseline(Board& b, const MaterialSpec& m) {
  set_baseline(b);
  if (m.white_queens < 9) {
    static const char* queen_sq[] = {"e1", "c1", "b3", "a3", "g5", "a5", "c6", "e7", "b8"};
    for (int i = static_cast<int>(m.white_queens); i < 9; ++i) {
      int s = sq_from_algebraic(queen_sq[static_cast<size_t>(i)]);
      if (s >= 0 && b[static_cast<size_t>(s)] == W_Q) b[static_cast<size_t>(s)] = EMPTY;
    }
  }
  if (m.white_knights < 2) {
    int s = sq_from_algebraic("g3");
    if (s >= 0 && b[static_cast<size_t>(s)] == W_N)
      b[static_cast<size_t>(s)] = EMPTY;
    else {
      s = sq_from_algebraic("h6");
      if (s >= 0 && b[static_cast<size_t>(s)] == W_N) b[static_cast<size_t>(s)] = EMPTY;
    }
  }
  if (m.white_bishops < 2) {
    int s = sq_from_algebraic("g1");
    if (s >= 0 && b[static_cast<size_t>(s)] == W_B)
      b[static_cast<size_t>(s)] = EMPTY;
    else {
      s = sq_from_algebraic("g4");
      if (s >= 0 && b[static_cast<size_t>(s)] == W_B) b[static_cast<size_t>(s)] = EMPTY;
    }
  }
}

}  // namespace chess
