#pragma once

#include "types.h"
#include <array>

namespace chess {

class Board;
struct UndoInfo;

class MoveList {
public:
    void add(Move m) { if (count_ < MAX_MOVES) moves_[count_++] = m; }
    int size() const { return count_; }
    void clear() { count_ = 0; }
    Move& operator[](int i) { return moves_[i]; }
    const Move& operator[](int i) const { return moves_[i]; }
    Move* begin() { return moves_.data(); }
    Move* end() { return moves_.data() + count_; }
    const Move* begin() const { return moves_.data(); }
    const Move* end() const { return moves_.data() + count_; }

private:
    std::array<Move, MAX_MOVES> moves_{};
    int count_ = 0;
};

class MoveGenerator {
public:
    void generateMoves(const Board& board, MoveList& moves);
    void generateLegalMoves(Board& board, MoveList& moves);
    uint64_t perft(const Board& board, int depth);

private:
    void generatePawnMoves(const Board& board, MoveList& moves);
    void generateKnightMoves(const Board& board, MoveList& moves);
    void generateBishopMoves(const Board& board, MoveList& moves);
    void generateRookMoves(const Board& board, MoveList& moves);
    void generateQueenMoves(const Board& board, MoveList& moves);
    void generateKingMoves(const Board& board, MoveList& moves);
    void generateCastlingMoves(const Board& board, MoveList& moves);
    uint64_t perftMutable(Board& board, int depth);
};

} // namespace chess
