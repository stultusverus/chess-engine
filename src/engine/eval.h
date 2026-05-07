#pragma once

#include "types.h"

namespace chess {

class Board;

class Eval {
public:
    static constexpr int PAWN_VALUE   = 100;
    static constexpr int KNIGHT_VALUE = 320;
    static constexpr int BISHOP_VALUE = 330;
    static constexpr int ROOK_VALUE   = 500;
    static constexpr int QUEEN_VALUE  = 900;
    static constexpr int KING_VALUE   = 20000;

    // Piece value array
    static constexpr int pieceValue(PieceType pt) {
        constexpr int vals[] = {PAWN_VALUE, KNIGHT_VALUE, BISHOP_VALUE, ROOK_VALUE, QUEEN_VALUE, KING_VALUE};
        return vals[pt];
    }

    // Evaluate position from white's perspective (positive = white advantage)
    int evaluate(const Board& board) const;

private:
    int material(const Board& board) const;
    int pieceSquare(const Board& board, int phase) const;
    int gamePhase(const Board& board) const;
    int pawnStructure(const Board& board) const;
    int mobility(const Board& board, int phase) const;
    int bishopPair(const Board& board, int phase) const;
    int rookOnFile(const Board& board, int phase) const;
    int kingSafety(const Board& board, int phase) const;
    int tempo(const Board& board) const;
};

} // namespace chess
