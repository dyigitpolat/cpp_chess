#include "mate.hpp"

namespace chess {

int count_mate_in_one(const Board& b, std::vector<MateMove>* list_out) {
  if (list_out) list_out->clear();

  if (find_king_sq(b, true) < 0 || find_king_sq(b, false) < 0) return 0;

  // Legal table: neither side in check before counting (stricter than chess.jsx solve)
  if (in_check(b, true)) return 0;
  if (in_check(b, false)) return 0;

  std::vector<Move> moves;
  moves.reserve(512);
  pseudo_moves(b, true, moves);

  Board nb;
  int count = 0;
  for (const Move& m : moves) {
    apply_move(b, m, nb);
    if (in_check(nb, true)) continue;
    if (!in_check(nb, false)) continue;
    // Defender has only king — optimized check
    if (black_king_has_legal_move(nb)) continue;

    ++count;
    if (list_out) list_out->push_back(MateMove{m.from, m.to, b[m.from]});
  }
  return count;
}

}  // namespace chess
