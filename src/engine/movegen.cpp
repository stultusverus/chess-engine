#include "movegen.h"
#include "board.h"
#include "attacks.h"

namespace chess {

void MoveGenerator::generateMoves(const Board& board, MoveList& moves) {
    Board legalBoard = board;
    generateLegalMoves(legalBoard, moves);
}

void MoveGenerator::generateLegalMoves(Board& board, MoveList& moves) {
    moves.clear();

    MoveList pseudo;
    generatePawnMoves(board, pseudo);
    generateKnightMoves(board, pseudo);
    generateBishopMoves(board, pseudo);
    generateRookMoves(board, pseudo);
    generateQueenMoves(board, pseudo);
    generateKingMoves(board, pseudo);
    generateCastlingMoves(board, pseudo);

    addLegalMoves(board, pseudo, moves);
}

void MoveGenerator::generateLegalNoisyMoves(Board& board, MoveList& moves) {
    moves.clear();

    MoveList pseudo;
    generateNoisyPawnMoves(board, pseudo);
    generateNoisyKnightMoves(board, pseudo);
    generateNoisyBishopMoves(board, pseudo);
    generateNoisyRookMoves(board, pseudo);
    generateNoisyQueenMoves(board, pseudo);
    generateNoisyKingMoves(board, pseudo);

    addLegalMoves(board, pseudo, moves);
}

bool MoveGenerator::hasLegalMove(Board& board) {
    MoveList pseudo;

    generatePawnMoves(board, pseudo);
    if (hasLegalMove(board, pseudo)) return true;
    pseudo.clear();

    generateKnightMoves(board, pseudo);
    if (hasLegalMove(board, pseudo)) return true;
    pseudo.clear();

    generateBishopMoves(board, pseudo);
    if (hasLegalMove(board, pseudo)) return true;
    pseudo.clear();

    generateRookMoves(board, pseudo);
    if (hasLegalMove(board, pseudo)) return true;
    pseudo.clear();

    generateQueenMoves(board, pseudo);
    if (hasLegalMove(board, pseudo)) return true;
    pseudo.clear();

    generateKingMoves(board, pseudo);
    if (hasLegalMove(board, pseudo)) return true;
    pseudo.clear();

    generateCastlingMoves(board, pseudo);
    return hasLegalMove(board, pseudo);
}

void MoveGenerator::addLegalMoves(Board& board, const MoveList& pseudo, MoveList& legal) {
    for (const Move& m : pseudo) {
        UndoInfo undo;
        if (board.makeMove(m, undo)) {
            legal.add(m);
            board.unmakeMove(m, undo);
        }
    }
}

bool MoveGenerator::hasLegalMove(Board& board, const MoveList& pseudo) {
    for (const Move& m : pseudo) {
        UndoInfo undo;
        if (board.makeMove(m, undo)) {
            board.unmakeMove(m, undo);
            return true;
        }
    }
    return false;
}

void MoveGenerator::generatePawnMoves(const Board& board, MoveList& moves) {
    Color us = board.sideToMove();
    Color enemy = ~us;
    Bitboard pawns = board.pieces(us, PAWN);
    Bitboard empty = board.empty();
    Bitboard enemyPieces = board.pieces(enemy);
    Square ep = board.enPassant();

    int push = (us == WHITE) ? 8 : -8;
    int push2 = (us == WHITE) ? 16 : -16;
    Rank startRank = (us == WHITE) ? RANK_2 : RANK_7;
    Rank promoRank = (us == WHITE) ? RANK_8 : RANK_1;
    Rank prePromo = (us == WHITE) ? RANK_7 : RANK_2;

    while (pawns) {
        Square sq = popLsb(pawns);

        // Single push
        Square fwd = Square(sq + push);
        if (empty & squareBb(fwd)) {
            bool promo = (rankOf(sq) == prePromo);
            if (promo) {
                moves.add(Move(sq, fwd, QUEEN, PROMOTION));
                moves.add(Move(sq, fwd, ROOK, PROMOTION));
                moves.add(Move(sq, fwd, BISHOP, PROMOTION));
                moves.add(Move(sq, fwd, KNIGHT, PROMOTION));
            } else {
                moves.add(Move(sq, fwd));
            }
            // Double push
            if (rankOf(sq) == startRank) {
                Square fwd2 = Square(sq + push2);
                if (empty & squareBb(fwd2))
                    moves.add(Move(sq, fwd2));
            }
        }

        // Captures
        Bitboard attacks = attacks::pawnAttacks(sq, us);
        while (attacks) {
            Square to = popLsb(attacks);
            bool promo = (rankOf(to) == promoRank);
            if (enemyPieces & squareBb(to)) {
                if (promo) {
                    moves.add(Move(sq, to, QUEEN, PROMOTION_CAPTURE));
                    moves.add(Move(sq, to, ROOK, PROMOTION_CAPTURE));
                    moves.add(Move(sq, to, BISHOP, PROMOTION_CAPTURE));
                    moves.add(Move(sq, to, KNIGHT, PROMOTION_CAPTURE));
                } else {
                    moves.add(Move(sq, to, PIECE_TYPE_NB, CAPTURE));
                }
            } else if (to == ep) {
                moves.add(Move(sq, to, PIECE_TYPE_NB, EN_PASSANT));
            }
        }
    }
}

void MoveGenerator::generateNoisyPawnMoves(const Board& board, MoveList& moves) {
    Color us = board.sideToMove();
    Color enemy = ~us;
    Bitboard pawns = board.pieces(us, PAWN);
    Bitboard empty = board.empty();
    Bitboard enemyPieces = board.pieces(enemy);
    Square ep = board.enPassant();

    int push = (us == WHITE) ? 8 : -8;
    Rank promoRank = (us == WHITE) ? RANK_8 : RANK_1;
    Rank prePromo = (us == WHITE) ? RANK_7 : RANK_2;

    while (pawns) {
        Square sq = popLsb(pawns);

        if (rankOf(sq) == prePromo) {
            Square fwd = Square(sq + push);
            if (empty & squareBb(fwd)) {
                moves.add(Move(sq, fwd, QUEEN, PROMOTION));
                moves.add(Move(sq, fwd, ROOK, PROMOTION));
                moves.add(Move(sq, fwd, BISHOP, PROMOTION));
                moves.add(Move(sq, fwd, KNIGHT, PROMOTION));
            }
        }

        Bitboard attacks = attacks::pawnAttacks(sq, us);
        while (attacks) {
            Square to = popLsb(attacks);
            bool promo = (rankOf(to) == promoRank);
            if (enemyPieces & squareBb(to)) {
                if (promo) {
                    moves.add(Move(sq, to, QUEEN, PROMOTION_CAPTURE));
                    moves.add(Move(sq, to, ROOK, PROMOTION_CAPTURE));
                    moves.add(Move(sq, to, BISHOP, PROMOTION_CAPTURE));
                    moves.add(Move(sq, to, KNIGHT, PROMOTION_CAPTURE));
                } else {
                    moves.add(Move(sq, to, PIECE_TYPE_NB, CAPTURE));
                }
            } else if (to == ep) {
                moves.add(Move(sq, to, PIECE_TYPE_NB, EN_PASSANT));
            }
        }
    }
}

void MoveGenerator::generateKnightMoves(const Board& board, MoveList& moves) {
    Color us = board.sideToMove();
    Color enemy = ~us;
    Bitboard knights = board.pieces(us, KNIGHT);
    Bitboard targets = ~board.pieces(us);
    Bitboard enemyPieces = board.pieces(enemy);

    while (knights) {
        Square sq = popLsb(knights);
        Bitboard att = attacks::knightAttacks(sq) & targets;
        while (att) {
            Square to = popLsb(att);
            moves.add(Move(sq, to, PIECE_TYPE_NB, (enemyPieces & squareBb(to)) ? CAPTURE : NORMAL));
        }
    }
}

void MoveGenerator::generateNoisyKnightMoves(const Board& board, MoveList& moves) {
    Color us = board.sideToMove();
    Color enemy = ~us;
    Bitboard knights = board.pieces(us, KNIGHT);
    Bitboard enemyPieces = board.pieces(enemy);

    while (knights) {
        Square sq = popLsb(knights);
        Bitboard att = attacks::knightAttacks(sq) & enemyPieces;
        while (att) {
            Square to = popLsb(att);
            moves.add(Move(sq, to, PIECE_TYPE_NB, CAPTURE));
        }
    }
}

void MoveGenerator::generateBishopMoves(const Board& board, MoveList& moves) {
    Color us = board.sideToMove();
    Color enemy = ~us;
    Bitboard bishops = board.pieces(us, BISHOP);
    Bitboard occ = board.occupied();
    Bitboard targets = ~board.pieces(us);
    Bitboard enemyPieces = board.pieces(enemy);

    while (bishops) {
        Square sq = popLsb(bishops);
        Bitboard att = attacks::bishopAttacks(sq, occ) & targets;
        while (att) {
            Square to = popLsb(att);
            moves.add(Move(sq, to, PIECE_TYPE_NB, (enemyPieces & squareBb(to)) ? CAPTURE : NORMAL));
        }
    }
}

void MoveGenerator::generateNoisyBishopMoves(const Board& board, MoveList& moves) {
    Color us = board.sideToMove();
    Color enemy = ~us;
    Bitboard bishops = board.pieces(us, BISHOP);
    Bitboard occ = board.occupied();
    Bitboard enemyPieces = board.pieces(enemy);

    while (bishops) {
        Square sq = popLsb(bishops);
        Bitboard att = attacks::bishopAttacks(sq, occ) & enemyPieces;
        while (att) {
            Square to = popLsb(att);
            moves.add(Move(sq, to, PIECE_TYPE_NB, CAPTURE));
        }
    }
}

void MoveGenerator::generateRookMoves(const Board& board, MoveList& moves) {
    Color us = board.sideToMove();
    Color enemy = ~us;
    Bitboard rooks = board.pieces(us, ROOK);
    Bitboard occ = board.occupied();
    Bitboard targets = ~board.pieces(us);
    Bitboard enemyPieces = board.pieces(enemy);

    while (rooks) {
        Square sq = popLsb(rooks);
        Bitboard att = attacks::rookAttacks(sq, occ) & targets;
        while (att) {
            Square to = popLsb(att);
            moves.add(Move(sq, to, PIECE_TYPE_NB, (enemyPieces & squareBb(to)) ? CAPTURE : NORMAL));
        }
    }
}

void MoveGenerator::generateNoisyRookMoves(const Board& board, MoveList& moves) {
    Color us = board.sideToMove();
    Color enemy = ~us;
    Bitboard rooks = board.pieces(us, ROOK);
    Bitboard occ = board.occupied();
    Bitboard enemyPieces = board.pieces(enemy);

    while (rooks) {
        Square sq = popLsb(rooks);
        Bitboard att = attacks::rookAttacks(sq, occ) & enemyPieces;
        while (att) {
            Square to = popLsb(att);
            moves.add(Move(sq, to, PIECE_TYPE_NB, CAPTURE));
        }
    }
}

void MoveGenerator::generateQueenMoves(const Board& board, MoveList& moves) {
    Color us = board.sideToMove();
    Color enemy = ~us;
    Bitboard queens = board.pieces(us, QUEEN);
    Bitboard occ = board.occupied();
    Bitboard targets = ~board.pieces(us);
    Bitboard enemyPieces = board.pieces(enemy);

    while (queens) {
        Square sq = popLsb(queens);
        Bitboard att = attacks::queenAttacks(sq, occ) & targets;
        while (att) {
            Square to = popLsb(att);
            moves.add(Move(sq, to, PIECE_TYPE_NB, (enemyPieces & squareBb(to)) ? CAPTURE : NORMAL));
        }
    }
}

void MoveGenerator::generateNoisyQueenMoves(const Board& board, MoveList& moves) {
    Color us = board.sideToMove();
    Color enemy = ~us;
    Bitboard queens = board.pieces(us, QUEEN);
    Bitboard occ = board.occupied();
    Bitboard enemyPieces = board.pieces(enemy);

    while (queens) {
        Square sq = popLsb(queens);
        Bitboard att = attacks::queenAttacks(sq, occ) & enemyPieces;
        while (att) {
            Square to = popLsb(att);
            moves.add(Move(sq, to, PIECE_TYPE_NB, CAPTURE));
        }
    }
}

void MoveGenerator::generateKingMoves(const Board& board, MoveList& moves) {
    Color us = board.sideToMove();
    Color enemy = ~us;
    Square sq = board.kingSquare(us);
    Bitboard targets = ~board.pieces(us);
    Bitboard enemyPieces = board.pieces(enemy);
    Bitboard att = attacks::kingAttacks(sq) & targets;
    while (att) {
        Square to = popLsb(att);
        moves.add(Move(sq, to, PIECE_TYPE_NB, (enemyPieces & squareBb(to)) ? CAPTURE : NORMAL));
    }
}

void MoveGenerator::generateNoisyKingMoves(const Board& board, MoveList& moves) {
    Color us = board.sideToMove();
    Color enemy = ~us;
    Square sq = board.kingSquare(us);
    Bitboard enemyPieces = board.pieces(enemy);
    Bitboard att = attacks::kingAttacks(sq) & enemyPieces;
    while (att) {
        Square to = popLsb(att);
        moves.add(Move(sq, to, PIECE_TYPE_NB, CAPTURE));
    }
}

void MoveGenerator::generateCastlingMoves(const Board& board, MoveList& moves) {
    if (board.isInCheck()) return;

    Color us = board.sideToMove();
    Color enemy = ~us;
    Bitboard occ = board.occupied();
    int cr = board.castlingRights();

    if (us == WHITE) {
        if ((cr & WK) && board.pieceOn(E1) == W_KING && board.pieceOn(H1) == W_ROOK &&
            !(occ & (squareBb(F1) | squareBb(G1)))) {
            if (!attacks::isSquareAttacked(board, E1, enemy) &&
                !attacks::isSquareAttacked(board, F1, enemy) &&
                !attacks::isSquareAttacked(board, G1, enemy)) {
                moves.add(Move(E1, G1, PIECE_TYPE_NB, CASTLING));
            }
        }
        if ((cr & WQ) && board.pieceOn(E1) == W_KING && board.pieceOn(A1) == W_ROOK &&
            !(occ & (squareBb(D1) | squareBb(C1) | squareBb(B1)))) {
            if (!attacks::isSquareAttacked(board, E1, enemy) &&
                !attacks::isSquareAttacked(board, D1, enemy) &&
                !attacks::isSquareAttacked(board, C1, enemy)) {
                moves.add(Move(E1, C1, PIECE_TYPE_NB, CASTLING));
            }
        }
    } else {
        if ((cr & BK) && board.pieceOn(E8) == B_KING && board.pieceOn(H8) == B_ROOK &&
            !(occ & (squareBb(F8) | squareBb(G8)))) {
            if (!attacks::isSquareAttacked(board, E8, enemy) &&
                !attacks::isSquareAttacked(board, F8, enemy) &&
                !attacks::isSquareAttacked(board, G8, enemy)) {
                moves.add(Move(E8, G8, PIECE_TYPE_NB, CASTLING));
            }
        }
        if ((cr & BQ) && board.pieceOn(E8) == B_KING && board.pieceOn(A8) == B_ROOK &&
            !(occ & (squareBb(D8) | squareBb(C8) | squareBb(B8)))) {
            if (!attacks::isSquareAttacked(board, E8, enemy) &&
                !attacks::isSquareAttacked(board, D8, enemy) &&
                !attacks::isSquareAttacked(board, C8, enemy)) {
                moves.add(Move(E8, C8, PIECE_TYPE_NB, CASTLING));
            }
        }
    }
}

uint64_t MoveGenerator::perft(const Board& board, int depth) {
    Board temp = board;
    return perftMutable(temp, depth);
}

uint64_t MoveGenerator::perftMutable(Board& board, int depth) {
    if (depth == 0) return 1;

    MoveList moves;
    generateLegalMoves(board, moves);

    if (depth == 1) return moves.size();

    uint64_t nodes = 0;
    for (const Move& m : moves) {
        UndoInfo undo;
        if (!board.makeMove(m, undo))
            continue;
        nodes += perftMutable(board, depth - 1);
        board.unmakeMove(m, undo);
    }
    return nodes;
}

} // namespace chess
