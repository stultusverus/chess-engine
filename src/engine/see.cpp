#include "see.h"
#include "attacks.h"
#include "board.h"
#include "eval.h"

#include <algorithm>

namespace chess {
namespace {

int pieceValue(PieceType pt) {
    return Eval::pieceValue(pt);
}

int promotionGain(PieceType from, PieceType promotion) {
    if (from != PAWN || promotion == PIECE_TYPE_NB)
        return 0;
    return pieceValue(promotion) - pieceValue(PAWN);
}

PieceType promotionOnTarget(PieceType pt, Color side, Square target) {
    if (pt != PAWN)
        return pt;
    if ((side == WHITE && rankOf(target) == RANK_8) ||
        (side == BLACK && rankOf(target) == RANK_1)) {
        return QUEEN;
    }
    return pt;
}

Bitboard piecesOf(const Board& board, Color side, PieceType pt, Bitboard occupied) {
    return board.pieces(side, pt) & occupied;
}

bool squareAttackedBy(const Board& board, Square square, Color by, Bitboard occupied) {
    if (by == BLACK) {
        if (piecesOf(board, BLACK, PAWN, occupied) & attacks::pawnAttacks(square, WHITE))
            return true;
    } else {
        if (piecesOf(board, WHITE, PAWN, occupied) & attacks::pawnAttacks(square, BLACK))
            return true;
    }

    if (piecesOf(board, by, KNIGHT, occupied) & attacks::knightAttacks(square))
        return true;
    if ((piecesOf(board, by, BISHOP, occupied) | piecesOf(board, by, QUEEN, occupied)) &
        attacks::bishopAttacks(square, occupied)) {
        return true;
    }
    if ((piecesOf(board, by, ROOK, occupied) | piecesOf(board, by, QUEEN, occupied)) &
        attacks::rookAttacks(square, occupied)) {
        return true;
    }
    if (piecesOf(board, by, KING, occupied) & attacks::kingAttacks(square))
        return true;

    return false;
}

bool exposesKing(const Board& board, Color side, Square from, Square target, PieceType pt, Bitboard occupied) {
    Bitboard after = occupied & ~squareBb(from);
    if (pt == KING)
        return squareAttackedBy(board, target, ~side, after);

    Square king = board.kingSquare(side);
    if (king == SQ_NONE)
        return false;
    return squareAttackedBy(board, king, ~side, after);
}

bool leastValuableAttacker(const Board& board, Square target, Color side, Bitboard occupied,
                           Square& from, PieceType& pt) {
    Bitboard attackers = attacks::attackersTo(board, target, occupied) & board.pieces(side) & occupied;
    for (PieceType candidate = PAWN; candidate <= KING; candidate = PieceType(candidate + 1)) {
        Bitboard pieces = attackers & board.pieces(side, candidate);
        while (pieces) {
            Square square = popLsb(pieces);
            if (!exposesKing(board, side, square, target, candidate, occupied)) {
                from = square;
                pt = candidate;
                return true;
            }
        }
    }
    return false;
}

int capturedValue(const Board& board, Move move, Color side) {
    if (move.type == EN_PASSANT)
        return pieceValue(PAWN);

    Piece captured = board.pieceOn(move.to);
    if (captured == NO_PIECE || colorOf(captured) == side)
        return 0;
    return pieceValue(typeOf(captured));
}

} // namespace

int staticExchangeEval(const Board& board, Move move) {
    if (move.from < A1 || move.from > H8 || move.to < A1 || move.to > H8)
        return 0;

    Piece movingPiece = board.pieceOn(move.from);
    if (movingPiece == NO_PIECE)
        return 0;

    Color side = board.sideToMove();
    PieceType movingType = typeOf(movingPiece);
    int gain[32]{};

    gain[0] = capturedValue(board, move, side) + promotionGain(movingType, move.promotion);

    Bitboard occupied = board.occupied();
    occupied &= ~squareBb(move.from);
    if (move.type == EN_PASSANT) {
        Square captured = Square(side == WHITE ? move.to - 8 : move.to + 8);
        occupied &= ~squareBb(captured);
        occupied |= squareBb(move.to);
    }

    PieceType targetPiece = move.promotion != PIECE_TYPE_NB ? move.promotion : movingType;
    side = ~side;

    int depth = 0;
    while (depth < 31) {
        Square from = SQ_NONE;
        PieceType attackerType = PIECE_TYPE_NB;
        if (!leastValuableAttacker(board, move.to, side, occupied, from, attackerType))
            break;

        depth++;
        PieceType promotedAttacker = promotionOnTarget(attackerType, side, move.to);
        gain[depth] = pieceValue(targetPiece) + promotionGain(attackerType, promotedAttacker) - gain[depth - 1];

        occupied &= ~squareBb(from);
        targetPiece = promotedAttacker;
        side = ~side;
    }

    while (depth > 0) {
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
        depth--;
    }

    return gain[0];
}

} // namespace chess
