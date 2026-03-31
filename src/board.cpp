#include "board.hpp"

#include <cstring>

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

bool is_attacked(const Board& b, int target_sq, bool by_white) {
  std::vector<int> atks;
  atks.reserve(32);
  for (int r = 0; r < 8; ++r)
    for (int c = 0; c < 8; ++c) {
      int sq = rc_to_sq(r, c);
      uint8_t p = b[sq];
      if (p == EMPTY) continue;
      bool is_w = is_white(p);
      if (is_w != by_white) continue;
      attacks_from_impl(b, r, c, atks);
      for (int a : atks)
        if (a == target_sq) return true;
    }
  return false;
}

int find_king_sq(const Board& b, bool white_king) {
  uint8_t want = white_king ? W_K : B_K;
  for (int i = 0; i < SQ_COUNT; ++i)
    if (b[i] == want) return i;
  return -1;
}

bool in_check(const Board& b, bool white_side) {
  int ksq = find_king_sq(b, white_side);
  if (ksq < 0) return false;
  return is_attacked(b, ksq, !white_side);
}

void pseudo_moves(const Board& b, bool white_side, std::vector<Move>& moves) {
  moves.clear();
  std::vector<int> atks;
  atks.reserve(32);
  for (int r = 0; r < 8; ++r)
    for (int c = 0; c < 8; ++c) {
      int sq = rc_to_sq(r, c);
      uint8_t p = b[sq];
      if (p == EMPTY) continue;
      bool is_w = is_white(p);
      if (is_w != white_side) continue;

      attacks_from_impl(b, r, c, atks);
      for (int dest : atks) {
        uint8_t dp = b[dest];
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

bool black_king_has_legal_move(const Board& b) {
  int ksq = find_king_sq(b, false);
  if (ksq < 0) return false;
  int kr = sq_r(ksq);
  int kc = sq_c(ksq);
  Board nb;
  for (int dr = -1; dr <= 1; ++dr)
    for (int dc = -1; dc <= 1; ++dc) {
      if (dr == 0 && dc == 0) continue;
      int nr = kr + dr, nc = kc + dc;
      if (!in_bounds(nr, nc)) continue;
      int to = rc_to_sq(nr, nc);
      uint8_t target = b[to];
      if (target != EMPTY && is_black(target)) continue;
      Move m{ksq, to};
      apply_move(b, m, nb);
      if (!in_check(nb, false)) return true;
    }
  return false;
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
