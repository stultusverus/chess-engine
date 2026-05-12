#include "movegen.h"
#include "board.h"
#include "attacks.h"

#include <array>

namespace chess {
namespace {

constexpr Bitboard ALL_SQUARES = ~Bitboard(0);

struct Direction {
    int df;
    int dr;
    bool diagonal;
};

constexpr std::array<Direction, 8> DIRECTIONS{{
    { 0,  1, false}, { 0, -1, false}, { 1,  0, false}, {-1,  0, false},
    { 1,  1, true},  {-1,  1, true},  { 1, -1, true},  {-1, -1, true},
}};

struct LegalContext {
    explicit LegalContext(const Board& b)
        : board(b),
          us(b.sideToMove()),
          them(~us),
          kingSq(b.kingSquare(us)),
          occ(b.occupied()),
          own(b.pieces(us)),
          enemy(b.pieces(them)),
          enemyKing(b.pieces(them, KING)),
          enemyNoKing(enemy & ~enemyKing) {}

    const Board& board;
    Color us;
    Color them;
    Square kingSq;
    Bitboard occ;
    Bitboard own;
    Bitboard enemy;
    Bitboard enemyKing;
    Bitboard enemyNoKing;
    Bitboard checkers = 0;
    Bitboard checkMask = ALL_SQUARES;
    Bitboard pinned = 0;
    int checkCount = 0;
    std::array<Bitboard, 64> pinRays{};
};

bool onBoard(int file, int rank) {
    return file >= FILE_A && file <= FILE_H && rank >= RANK_1 && rank <= RANK_8;
}

bool isNoisyMove(MoveType type) {
    return type == CAPTURE || type == EN_PASSANT ||
           type == PROMOTION || type == PROMOTION_CAPTURE;
}

bool attacksAlong(PieceType pt, bool diagonal) {
    return pt == QUEEN || (diagonal ? pt == BISHOP : pt == ROOK);
}

int directionStep(Square from, Square to) {
    int fromFile = fileOf(from);
    int fromRank = rankOf(from);
    int toFile = fileOf(to);
    int toRank = rankOf(to);
    int df = toFile - fromFile;
    int dr = toRank - fromRank;

    if (df == 0)
        return dr > 0 ? 8 : -8;
    if (dr == 0)
        return df > 0 ? 1 : -1;
    if (df == dr)
        return df > 0 ? 9 : -9;
    if (df == -dr)
        return df > 0 ? -7 : 7;
    return 0;
}

Bitboard rayBetweenInclusive(Square from, Square to) {
    int step = directionStep(from, to);
    if (step == 0)
        return squareBb(to);

    Bitboard ray = 0;
    for (int sq = from + step; sq >= A1 && sq <= H8; sq += step) {
        ray |= squareBb(Square(sq));
        if (sq == to)
            break;
    }
    return ray;
}

bool isSquareAttackedBy(const Board& board, Square square, Color by,
                        Bitboard occupied, Bitboard ignoredEnemyPieces = 0) {
    Bitboard pawns = board.pieces(by, PAWN) & ~ignoredEnemyPieces;
    if (by == BLACK) {
        if (pawns & attacks::pawnAttacks(square, WHITE))
            return true;
    } else {
        if (pawns & attacks::pawnAttacks(square, BLACK))
            return true;
    }

    if ((board.pieces(by, KNIGHT) & ~ignoredEnemyPieces) & attacks::knightAttacks(square))
        return true;
    if (((board.pieces(by, BISHOP) | board.pieces(by, QUEEN)) & ~ignoredEnemyPieces) &
        attacks::bishopAttacks(square, occupied)) {
        return true;
    }
    if (((board.pieces(by, ROOK) | board.pieces(by, QUEEN)) & ~ignoredEnemyPieces) &
        attacks::rookAttacks(square, occupied)) {
        return true;
    }
    if ((board.pieces(by, KING) & ~ignoredEnemyPieces) & attacks::kingAttacks(square))
        return true;

    return false;
}

LegalContext buildLegalContext(const Board& board) {
    LegalContext ctx(board);
    if (ctx.kingSq == SQ_NONE)
        return ctx;

    ctx.checkers = attacks::attackersTo(board, ctx.kingSq, ctx.occ) & ctx.enemy;
    ctx.checkCount = popcount(ctx.checkers);
    if (ctx.checkCount == 1) {
        Square checker = lsb(ctx.checkers);
        Piece checkerPiece = board.pieceOn(checker);
        PieceType checkerType = checkerPiece == NO_PIECE ? PIECE_TYPE_NB : typeOf(checkerPiece);
        if (checkerType == BISHOP || checkerType == ROOK || checkerType == QUEEN)
            ctx.checkMask = rayBetweenInclusive(ctx.kingSq, checker);
        else
            ctx.checkMask = squareBb(checker);
    } else if (ctx.checkCount > 1) {
        ctx.checkMask = 0;
    }

    int kingFile = fileOf(ctx.kingSq);
    int kingRank = rankOf(ctx.kingSq);
    for (const Direction& dir : DIRECTIONS) {
        Square pinnedSq = SQ_NONE;
        Bitboard ray = 0;

        for (int file = kingFile + dir.df, rank = kingRank + dir.dr;
             onBoard(file, rank);
             file += dir.df, rank += dir.dr) {
            Square sq = makeSquare(File(file), Rank(rank));
            ray |= squareBb(sq);

            Piece piece = board.pieceOn(sq);
            if (piece == NO_PIECE)
                continue;

            if (colorOf(piece) == ctx.us) {
                if (pinnedSq != SQ_NONE)
                    break;
                pinnedSq = sq;
                continue;
            }

            if (pinnedSq != SQ_NONE && attacksAlong(typeOf(piece), dir.diagonal)) {
                ctx.pinned |= squareBb(pinnedSq);
                ctx.pinRays[pinnedSq] = ray;
            }
            break;
        }
    }

    return ctx;
}

void addMove(MoveList& moves, Move move, bool noisyOnly) {
    if (!noisyOnly || isNoisyMove(move.type))
        moves.add(move);
}

void addPromotionMoves(MoveList& moves, Square from, Square to, MoveType type, bool noisyOnly) {
    addMove(moves, Move(from, to, QUEEN, type), noisyOnly);
    addMove(moves, Move(from, to, ROOK, type), noisyOnly);
    addMove(moves, Move(from, to, BISHOP, type), noisyOnly);
    addMove(moves, Move(from, to, KNIGHT, type), noisyOnly);
}

bool legalEnPassant(const LegalContext& ctx, Square from, Square to) {
    if (ctx.kingSq == SQ_NONE || to != ctx.board.enPassant())
        return false;

    int capturedIdx = ctx.us == WHITE ? to - 8 : to + 8;
    if (capturedIdx < A1 || capturedIdx > H8)
        return false;

    Square captured = Square(capturedIdx);
    Bitboard fromBb = squareBb(from);
    Bitboard toBb = squareBb(to);
    Bitboard capturedBb = squareBb(captured);
    if (ctx.board.pieceOn(to) != NO_PIECE ||
        ctx.board.pieceOn(captured) != makePiece(ctx.them, PAWN)) {
        return false;
    }

    Bitboard occupied = (ctx.occ & ~fromBb & ~capturedBb) | toBb;
    return !isSquareAttackedBy(ctx.board, ctx.kingSq, ctx.them, occupied, capturedBb);
}

void generatePawnMoves(const LegalContext& ctx, MoveList& moves, bool noisyOnly) {
    Bitboard pawns = ctx.board.pieces(ctx.us, PAWN);
    Bitboard empty = ~ctx.occ;
    Square ep = ctx.board.enPassant();

    int push = ctx.us == WHITE ? 8 : -8;
    int push2 = ctx.us == WHITE ? 16 : -16;
    Rank startRank = ctx.us == WHITE ? RANK_2 : RANK_7;
    Rank promoRank = ctx.us == WHITE ? RANK_8 : RANK_1;
    Rank prePromo = ctx.us == WHITE ? RANK_7 : RANK_2;

    while (pawns) {
        Square from = popLsb(pawns);
        Bitboard fromBb = squareBb(from);
        Bitboard pinMask = (ctx.pinned & fromBb) ? ctx.pinRays[from] : ALL_SQUARES;

        if (ctx.checkCount <= 1) {
            int oneIdx = from + push;
            if (oneIdx >= A1 && oneIdx <= H8) {
                Square to = Square(oneIdx);
                Bitboard toBb = squareBb(to);
                if (empty & toBb) {
                    if ((ctx.checkMask & toBb) && (pinMask & toBb)) {
                        if (rankOf(from) == prePromo) {
                            addPromotionMoves(moves, from, to, PROMOTION, noisyOnly);
                        } else if (!noisyOnly) {
                            moves.add(Move(from, to));
                        }
                    }

                    if (!noisyOnly && rankOf(from) == startRank) {
                        int twoIdx = from + push2;
                        if (twoIdx >= A1 && twoIdx <= H8) {
                            Square to2 = Square(twoIdx);
                            Bitboard to2Bb = squareBb(to2);
                            if ((empty & to2Bb) && (ctx.checkMask & to2Bb) && (pinMask & to2Bb))
                                moves.add(Move(from, to2));
                        }
                    }
                }
            }

            Bitboard captures = attacks::pawnAttacks(from, ctx.us) & ctx.enemyNoKing;
            while (captures) {
                Square to = popLsb(captures);
                Bitboard toBb = squareBb(to);
                if (!(ctx.checkMask & toBb) || !(pinMask & toBb))
                    continue;

                if (rankOf(to) == promoRank)
                    addPromotionMoves(moves, from, to, PROMOTION_CAPTURE, noisyOnly);
                else
                    addMove(moves, Move(from, to, PIECE_TYPE_NB, CAPTURE), noisyOnly);
            }
        }

        if (ep != SQ_NONE && (attacks::pawnAttacks(from, ctx.us) & squareBb(ep)) &&
            legalEnPassant(ctx, from, ep)) {
            addMove(moves, Move(from, ep, PIECE_TYPE_NB, EN_PASSANT), noisyOnly);
        }
    }
}

void generateKnightMoves(const LegalContext& ctx, MoveList& moves, bool noisyOnly) {
    if (ctx.checkCount > 1)
        return;

    Bitboard knights = ctx.board.pieces(ctx.us, KNIGHT) & ~ctx.pinned;
    Bitboard targetMask = ~ctx.own & ~ctx.enemyKing & ctx.checkMask;
    if (noisyOnly)
        targetMask &= ctx.enemyNoKing;

    while (knights) {
        Square from = popLsb(knights);
        Bitboard targets = attacks::knightAttacks(from) & targetMask;
        while (targets) {
            Square to = popLsb(targets);
            MoveType type = (ctx.enemyNoKing & squareBb(to)) ? CAPTURE : NORMAL;
            addMove(moves, Move(from, to, PIECE_TYPE_NB, type), noisyOnly);
        }
    }
}

Bitboard sliderAttacks(PieceType pt, Square from, Bitboard occupied) {
    if (pt == BISHOP)
        return attacks::bishopAttacks(from, occupied);
    if (pt == ROOK)
        return attacks::rookAttacks(from, occupied);
    return attacks::queenAttacks(from, occupied);
}

void generateSliderMoves(const LegalContext& ctx, MoveList& moves, bool noisyOnly, PieceType pt) {
    if (ctx.checkCount > 1)
        return;

    Bitboard pieces = ctx.board.pieces(ctx.us, pt);
    Bitboard baseTargets = ~ctx.own & ~ctx.enemyKing & ctx.checkMask;
    if (noisyOnly)
        baseTargets &= ctx.enemyNoKing;

    while (pieces) {
        Square from = popLsb(pieces);
        Bitboard targets = sliderAttacks(pt, from, ctx.occ) & baseTargets;
        Bitboard fromBb = squareBb(from);
        if (ctx.pinned & fromBb)
            targets &= ctx.pinRays[from];

        while (targets) {
            Square to = popLsb(targets);
            MoveType type = (ctx.enemyNoKing & squareBb(to)) ? CAPTURE : NORMAL;
            addMove(moves, Move(from, to, PIECE_TYPE_NB, type), noisyOnly);
        }
    }
}

void generateKingMoves(const LegalContext& ctx, MoveList& moves, bool noisyOnly) {
    if (ctx.kingSq == SQ_NONE)
        return;

    Square from = ctx.kingSq;
    Bitboard fromBb = squareBb(from);
    Bitboard targets = attacks::kingAttacks(from) & ~ctx.own & ~ctx.enemyKing;
    while (targets) {
        Square to = popLsb(targets);
        Bitboard toBb = squareBb(to);
        Bitboard captured = ctx.enemyNoKing & toBb;
        Bitboard occupied = (ctx.occ & ~fromBb & ~captured) | toBb;
        if (isSquareAttackedBy(ctx.board, to, ctx.them, occupied, captured))
            continue;

        MoveType type = captured ? CAPTURE : NORMAL;
        addMove(moves, Move(from, to, PIECE_TYPE_NB, type), noisyOnly);
    }
}

void generateCastlingMoves(const LegalContext& ctx, MoveList& moves) {
    if (ctx.kingSq == SQ_NONE || ctx.checkCount != 0)
        return;

    int cr = ctx.board.castlingRights();
    if (ctx.us == WHITE) {
        if ((cr & WK) && ctx.board.pieceOn(E1) == W_KING && ctx.board.pieceOn(H1) == W_ROOK &&
            !(ctx.occ & (squareBb(F1) | squareBb(G1))) &&
            !isSquareAttackedBy(ctx.board, F1, ctx.them, ctx.occ) &&
            !isSquareAttackedBy(ctx.board, G1, ctx.them, ctx.occ)) {
            moves.add(Move(E1, G1, PIECE_TYPE_NB, CASTLING));
        }
        if ((cr & WQ) && ctx.board.pieceOn(E1) == W_KING && ctx.board.pieceOn(A1) == W_ROOK &&
            !(ctx.occ & (squareBb(D1) | squareBb(C1) | squareBb(B1))) &&
            !isSquareAttackedBy(ctx.board, D1, ctx.them, ctx.occ) &&
            !isSquareAttackedBy(ctx.board, C1, ctx.them, ctx.occ)) {
            moves.add(Move(E1, C1, PIECE_TYPE_NB, CASTLING));
        }
    } else {
        if ((cr & BK) && ctx.board.pieceOn(E8) == B_KING && ctx.board.pieceOn(H8) == B_ROOK &&
            !(ctx.occ & (squareBb(F8) | squareBb(G8))) &&
            !isSquareAttackedBy(ctx.board, F8, ctx.them, ctx.occ) &&
            !isSquareAttackedBy(ctx.board, G8, ctx.them, ctx.occ)) {
            moves.add(Move(E8, G8, PIECE_TYPE_NB, CASTLING));
        }
        if ((cr & BQ) && ctx.board.pieceOn(E8) == B_KING && ctx.board.pieceOn(A8) == B_ROOK &&
            !(ctx.occ & (squareBb(D8) | squareBb(C8) | squareBb(B8))) &&
            !isSquareAttackedBy(ctx.board, D8, ctx.them, ctx.occ) &&
            !isSquareAttackedBy(ctx.board, C8, ctx.them, ctx.occ)) {
            moves.add(Move(E8, C8, PIECE_TYPE_NB, CASTLING));
        }
    }
}

void generateLegalMoves(const Board& board, MoveList& moves, bool noisyOnly) {
    moves.clear();

    LegalContext ctx = buildLegalContext(board);
    generatePawnMoves(ctx, moves, noisyOnly);
    generateKnightMoves(ctx, moves, noisyOnly);
    generateSliderMoves(ctx, moves, noisyOnly, BISHOP);
    generateSliderMoves(ctx, moves, noisyOnly, ROOK);
    generateSliderMoves(ctx, moves, noisyOnly, QUEEN);
    generateKingMoves(ctx, moves, noisyOnly);
    if (!noisyOnly)
        generateCastlingMoves(ctx, moves);
}

} // namespace

void MoveGenerator::generateMoves(const Board& board, MoveList& moves) {
    ::chess::generateLegalMoves(board, moves, false);
}

void MoveGenerator::generateLegalMoves(Board& board, MoveList& moves) {
    ::chess::generateLegalMoves(static_cast<const Board&>(board), moves, false);
}

void MoveGenerator::generateLegalNoisyMoves(Board& board, MoveList& moves) {
    ::chess::generateLegalMoves(static_cast<const Board&>(board), moves, true);
}

bool MoveGenerator::hasLegalMove(Board& board) {
    MoveList moves;
    ::chess::generateLegalMoves(static_cast<const Board&>(board), moves, false);
    return moves.size() != 0;
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
