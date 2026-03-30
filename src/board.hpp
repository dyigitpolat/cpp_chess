#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace chess {

constexpr int SQ_COUNT = 64;

// Piece codes (no pawns in this puzzle)
constexpr uint8_t EMPTY = 0;
constexpr uint8_t W_K = 1;
constexpr uint8_t W_Q = 2;
constexpr uint8_t W_R = 3;
constexpr uint8_t W_B = 4;
constexpr uint8_t W_N = 5;
constexpr uint8_t B_K = 6;

inline bool is_white(uint8_t p) { return p >= W_K && p <= W_N; }
inline bool is_black(uint8_t p) { return p == B_K; }

// Color: true = white (attacker), false = black (defender)
inline bool is_color_white(uint8_t p) { return is_white(p); }

using Board = std::array<uint8_t, SQ_COUNT>;

inline int rc_to_sq(int r, int c) { return r * 8 + c; }
inline int sq_r(int sq) { return sq / 8; }
inline int sq_c(int sq) { return sq % 8; }

// Algebraic like "d4" (rank 1-8, file a-h). Matches chess.jsx: r=0 rank 8.
int sq_from_algebraic(const char* alg);

std::string algebraic_from_sq(int sq);

struct Move {
  int from;
  int to;
};

bool in_bounds(int r, int c);

// Attacks from square `sq` (target squares a piece could move to for sliding/attacks).
void attacks_from(const Board& b, int sq, std::vector<int>& out);

bool is_attacked(const Board& b, int target_sq, bool by_white);

int find_king_sq(const Board& b, bool white_king);

bool in_check(const Board& b, bool white_to_move_side);

void pseudo_moves(const Board& b, bool white_side, std::vector<Move>& out);

void apply_move(const Board& b, Move m, Board& out);

bool has_any_legal_move(const Board& b, bool white_side);

// Black has only a king — faster than generic has_any_legal_move.
bool black_king_has_legal_move(const Board& b);

// True if neither king is in check (legal starting position for GA).
bool position_legal_quiet(const Board& b);

// Fixed composition: 9Q 2R 2N 2B 1K vs 1k
bool valid_material(const Board& b);

void clear_board(Board& b);

// Baseline from plan (image)
void set_baseline(Board& b);

}  // namespace chess
