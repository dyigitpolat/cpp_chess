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

/// Square attacked by side `by_white`. For `by_white == false`, only the black king is considered
/// (this project’s positions use Black = lone king). For a full chess set, extend this.
bool is_attacked(const Board& b, int target_sq, bool by_white);

int find_king_sq(const Board& b, bool white_king);

bool in_check(const Board& b, bool white_to_move_side);

// Bitmask of squares (0..63) with a white piece — ~16 bits set in the puzzle composition.
uint64_t white_piece_occ_mask(const Board& b);

// Mate-in-one / lone-black-king fast paths: avoid find_king_sq when king squares are known.
// Black must have exactly one king; White must not capture it on the prior ply.
bool black_king_in_check(const Board& b, int black_king_sq);
// Reuse `white_mask` from white_piece_occ_mask(nb) when calling repeatedly on the same `b`.
bool black_king_in_check(const Board& b, int black_king_sq, uint64_t white_mask);
bool white_king_in_check_vs_lone_black(const Board& b, int white_king_sq, int black_king_sq);

void pseudo_moves(const Board& b, bool white_side, std::vector<Move>& out);

void apply_move(const Board& b, Move m, Board& out);

bool has_any_legal_move(const Board& b, bool white_side);

// Black has only a king — faster than generic has_any_legal_move.
bool black_king_has_legal_move(const Board& b);
// When the black king square is already known (e.g. unchanged after a White move), avoids a board scan.
bool black_king_has_legal_move(const Board& b, int black_king_sq);

// True if neither king is in check (legal starting position for GA).
bool position_legal_quiet(const Board& b);

/// White attacking force: 9Q (default), 2R, knights/bishops 1–2 each (default 2), 1K vs 1k.
struct MaterialSpec {
  int white_queens = 9;
  int white_knights = 2;
  int white_bishops = 2;
};

/// Total pieces on board for this composition (Q + 2R + N + B + both kings).
int material_piece_count(const MaterialSpec& m);

std::string composition_string(const MaterialSpec& m);

bool valid_material(const Board& b, const MaterialSpec& m = MaterialSpec{});

// Fixed composition: 9Q 2R 2N 2B 1K vs 1k
void set_baseline(Board& b);

/// Image baseline, then remove queens/knights/bishops (e1, then c1, …; g3/h6; g1/g4).
void set_baseline(Board& b, const MaterialSpec& m);

void clear_board(Board& b);

// Further speedup (large refactor): bitboards and precomputed sliding attacks.

}  // namespace chess
