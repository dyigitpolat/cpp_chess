#include "mate.hpp"

namespace chess {

int count_mate_in_one(const Board& b, std::vector<MateMove>* list_out) {
  if (list_out) list_out->clear();

  const int white_king_sq0 = find_king_sq(b, true);
  const int black_king_sq = find_king_sq(b, false);
  if (white_king_sq0 < 0 || black_king_sq < 0) return 0;

  // Legal table: neither side in check before counting (stricter than chess.jsx solve)
  if (white_king_in_check_vs_lone_black(b, white_king_sq0, black_king_sq)) return 0;
  if (black_king_in_check(b, black_king_sq)) return 0;

  thread_local std::vector<Move> moves;
  moves.clear();
  moves.reserve(512);
  pseudo_moves(b, true, moves);

  Board nb;
  int count = 0;
  for (const Move& m : moves) {
    apply_move(b, m, nb);
    const int white_king_sq = (m.from == white_king_sq0) ? m.to : white_king_sq0;
    if (white_king_in_check_vs_lone_black(nb, white_king_sq, black_king_sq)) continue;
    const uint64_t white_mask = white_piece_occ_mask(nb);
    if (!black_king_in_check(nb, black_king_sq, white_mask)) continue;
    // Defender has only king — black king square unchanged after a White-only move.
    if (black_king_has_legal_move(nb, black_king_sq)) continue;

    ++count;
    if (list_out) list_out->push_back(MateMove{m.from, m.to, b[m.from]});
  }
  return count;
}

}  // namespace chess
