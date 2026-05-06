#pragma once

#include "types.h"

namespace chess {

class Board;

namespace attacks {

void init();

Bitboard pawnAttacks(Square s, Color c);
Bitboard knightAttacks(Square s);
Bitboard bishopAttacks(Square s, Bitboard occupied);
Bitboard rookAttacks(Square s, Bitboard occupied);
Bitboard queenAttacks(Square s, Bitboard occupied);
Bitboard kingAttacks(Square s);

bool isSquareAttacked(const Board& board, Square s, Color by);
Bitboard attackersTo(const Board& board, Square s, Bitboard occupied);

} // namespace attacks
} // namespace chess
