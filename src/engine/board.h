#pragma once

#include "types.h"
#include <array>
#include <string>
#include <vector>

namespace chess {

struct UndoInfo;
struct NullUndo;

class Board {
public:
    Board();
    explicit Board(const std::string& fen);

    // FEN
    void setFen(const std::string& fen);
    std::string fen() const;

    // Moves
    bool makeMove(Move move, UndoInfo& undo);
    void unmakeMove(Move move, const UndoInfo& undo);
    bool isMoveLegal(Move move) const;
    bool isInCheck() const;

    // Piece queries
    Piece pieceOn(Square s) const;
    Bitboard pieces() const;
    Bitboard pieces(Color c) const;
    Bitboard pieces(PieceType pt) const;
    Bitboard pieces(Color c, PieceType pt) const;
    Bitboard occupied() const;
    Bitboard empty() const;

    // State queries
    Color sideToMove() const { return stm_; }
    Square enPassant() const { return ep_; }
    int castlingRights() const { return castle_; }
    int halfMoveClock() const { return halfMoves_; }
    int fullMoveNumber() const { return fullMoves_; }
    uint64_t hash() const { return hash_; }

    // Null move (for search pruning)
    void makeNullMove(NullUndo& undo);
    void unmakeNullMove(const NullUndo& undo);

    // King square lookup
    Square kingSquare(Color c) const;

    static void initZobrist();

private:
    std::array<Bitboard, PIECE_NB> byPiece_{};
    std::array<Bitboard, COLOR_NB> byColor_{};
    std::array<Piece, 64> mailbox_{};

    Color stm_ = WHITE;
    Square ep_ = SQ_NONE;
    int castle_ = 0;
    int halfMoves_ = 0;
    int fullMoves_ = 1;
    uint64_t hash_ = 0;

    void putPiece(Piece p, Square s);
    void removePiece(Piece p, Square s);
    void movePiece(Piece p, Square from, Square to);
};

struct UndoInfo {
    Move move;
    Piece captured;
    Square oldEp;
    int oldCastle;
    int oldHalfMoves;
    int oldFullMoves;
    uint64_t oldHash;
};

struct NullUndo {
    Square oldEp;
    int oldHalfMoves;
    uint64_t oldHash;
};

} // namespace chess
