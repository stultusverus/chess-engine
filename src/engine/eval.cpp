#include "eval.h"
#include "board.h"
#include "attacks.h"
#include <algorithm>

namespace chess {

// --- Helpers ---
static constexpr Bitboard fileBb(File f) {
    return 0x0101010101010101ULL << f;
}

// --- PeSTO Piece-Square Tables (from white's perspective, rank 1 at bottom) ---
// MG = middlegame, EG = endgame

static constexpr int pawnMG[64] = {
      0,   0,   0,   0,   0,   0,  0,   0,
     98, 134,  61,  95,  68, 126, 34, -11,
     -6,   7,  26,  31,  65,  56, 25, -20,
    -14,  13,   6,  21,  23,  12, 17, -23,
    -27,  -2,  -5,  12,  17,   6, 10, -25,
    -26,  -4,  -4, -10,   3,   3, 33, -12,
    -35,  -1, -20, -23, -15,  24, 38, -22,
      0,   0,   0,   0,   0,   0,  0,   0,
};

static constexpr int pawnEG[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8,  10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0,
};

static constexpr int knightMG[64] = {
    -167, -89, -34, -49,  61, -97, -15, -107,
     -73, -41,  72,  36,  23,  62,   7,  -17,
     -47,  60,  37,  65,  84, 129,  73,   44,
      -9,  17,  19,  53,  37,  69,  18,   22,
     -13,   4,  16,  13,  28,  19,  21,   -8,
     -23,  -9,  12,  10,  19,  17,  25,  -16,
     -29, -53, -12,  -3,  -1,  18, -14,  -19,
    -105, -21, -58, -33, -17, -28, -19,  -23,
};

static constexpr int knightEG[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64,
};

static constexpr int bishopMG[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21,
};

static constexpr int bishopEG[64] = {
    -14, -21, -11,  -8, -7,  -9, -17, -24,
     -8,  -4,   7, -12, -3, -13,  -4, -14,
      2,  -8,   0,  -1, -2,   6,   0,   4,
     -3,   9,  12,   9, 14,  10,   3,   2,
     -6,   3,  13,  19,  7,  10,  -3,  -9,
    -12,  -3,   8,  10, 13,   3,  -7, -15,
    -14, -18,  -7,  -1,  4,  -9, -15, -27,
    -23,  -9, -23,  -5, -9, -16,  -5, -17,
};

static constexpr int rookMG[64] = {
     32,  42,  32,  51, 63,  9,  31,  43,
     27,  32,  58,  62, 80, 67,  26,  44,
     -5,  19,  26,  36, 17, 45,  61,  16,
    -24, -11,   7,  26, 24, 35,  -8, -20,
    -36, -26, -12,  -1,  9, -7,   6, -23,
    -45, -25, -16, -17,  3,  0,  -5, -33,
    -44, -16, -20,  -9, -1, 11,  -6, -71,
    -19, -13,   1,  17, 16,  7, -37, -26,
};

static constexpr int rookEG[64] = {
    13, 10, 18, 15, 12,  12,   8,   5,
    11, 13, 13, 11, -3,   3,   8,   3,
     7,  7,  7,  5,  4,  -3,  -5,  -3,
     4,  3, 13,  1,  2,   1,  -1,   2,
     3,  5,  8,  4, -5,  -6,  -8, -11,
    -4,  0, -5, -1, -7, -12,  -8, -16,
    -6, -6,  0,  2, -9,  -9, -11,  -3,
    -9,  2,  3, -1, -5, -13,   4, -20,
};

static constexpr int queenMG[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50,
};

static constexpr int queenEG[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41,
};

static constexpr int kingMG[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14,
};

static constexpr int kingEG[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43,
};

// Tapered evaluation: phase = total game phase remaining (24 = start, 0 = endgame only)
// MG weight = phase / 24, EG weight = 1 - MG weight
static constexpr int phaseInc[PIECE_TYPE_NB] = {0, 1, 1, 2, 4, 0}; // Pawn, Knight, Bishop, Rook, Queen, King

static const int* mgTables[PIECE_TYPE_NB] = {
    pawnMG, knightMG, bishopMG, rookMG, queenMG, kingMG,
};

static const int* egTables[PIECE_TYPE_NB] = {
    pawnEG, knightEG, bishopEG, rookEG, queenEG, kingEG,
};

// --- Evaluation term constants ---
static constexpr int DOUBLED_PAWN_PENALTY   = 15;
static constexpr int ISOLATED_PAWN_PENALTY  = 15;
static constexpr int PASSED_PAWN_BONUS[8]   = {0, 0, 5, 10, 20, 35, 60, 0};

static constexpr int KNIGHT_MOBILITY_MG = 4;
static constexpr int KNIGHT_MOBILITY_EG = 4;
static constexpr int BISHOP_MOBILITY_MG = 3;
static constexpr int BISHOP_MOBILITY_EG = 3;
static constexpr int ROOK_MOBILITY_MG   = 2;
static constexpr int ROOK_MOBILITY_EG   = 3;
static constexpr int QUEEN_MOBILITY_MG  = 1;
static constexpr int QUEEN_MOBILITY_EG  = 2;
static constexpr int KING_MOBILITY_MG   = 0;
static constexpr int KING_MOBILITY_EG   = 2;

static constexpr int BISHOP_PAIR_MG = 30;
static constexpr int BISHOP_PAIR_EG = 50;

static constexpr int ROOK_OPEN_FILE_MG      = 20;
static constexpr int ROOK_OPEN_FILE_EG      = 25;
static constexpr int ROOK_SEMI_OPEN_FILE_MG = 10;
static constexpr int ROOK_SEMI_OPEN_FILE_EG = 15;

static constexpr int KING_SHIELD_BONUS = 10;
static constexpr int KING_OPEN_FILE_PENALTY = 15;
static constexpr int KING_ATTACK_KNIGHT    = 2;
static constexpr int KING_ATTACK_BISHOP    = 2;
static constexpr int KING_ATTACK_ROOK      = 3;
static constexpr int KING_ATTACK_QUEEN     = 5;
static constexpr int KING_ATTACK_PROXIMITY = 2;
static constexpr int TEMPO_BONUS = 15;

int Eval::pieceSquareMg(Piece p, Square s) {
    PieceType pt = typeOf(p);
    if (colorOf(p) == WHITE)
        return mgTables[pt][s];
    return -mgTables[pt][s ^ 56];
}

int Eval::pieceSquareEg(Piece p, Square s) {
    PieceType pt = typeOf(p);
    if (colorOf(p) == WHITE)
        return egTables[pt][s];
    return -egTables[pt][s ^ 56];
}

int Eval::phaseValue(PieceType pt) {
    return phaseInc[pt];
}

int Eval::material(const Board& board) const {
    return board.materialScore();
}

int Eval::pieceSquare(const Board& board, int phase) const {
    return tapered(board.pstMgScore(), board.pstEgScore(), phase);
}

int Eval::tapered(int mgScore, int egScore, int phase) const {
    return (mgScore * phase + egScore * (TOTAL_PHASE - phase)) / TOTAL_PHASE;
}

int Eval::cachedPawnStructure(const Board& board) const {
    size_t idx = board.pawnHash() & (PAWN_CACHE_SIZE - 1);
    PawnCacheEntry& entry = pawnCache_[idx];
    if (entry.valid && entry.pawnHash == board.pawnHash())
        return entry.score;

    int score = pawnStructure(board);
    entry.pawnHash = board.pawnHash();
    entry.score = score;
    entry.valid = true;
    return score;
}

int Eval::pawnStructure(const Board& board) const {
    int score = 0;

    Bitboard wPawns = board.pieces(WHITE, PAWN);
    Bitboard bPawns = board.pieces(BLACK, PAWN);

    // Doubled and isolated pawns
    for (int f = FILE_A; f <= FILE_H; f++) {
        int wCnt = popcount(wPawns & fileBb(File(f)));
        int bCnt = popcount(bPawns & fileBb(File(f)));

        if (wCnt > 1) score -= (wCnt - 1) * DOUBLED_PAWN_PENALTY;
        if (bCnt > 1) score += (bCnt - 1) * DOUBLED_PAWN_PENALTY;

        if (wCnt > 0) {
            bool hasNeighbor = (f > FILE_A && (wPawns & fileBb(File(f - 1)))) ||
                               (f < FILE_H && (wPawns & fileBb(File(f + 1))));
            if (!hasNeighbor) {
                int penalty = (f == FILE_A || f == FILE_H) ? ISOLATED_PAWN_PENALTY / 2 : ISOLATED_PAWN_PENALTY;
                score -= wCnt * penalty;
            }
        }
        if (bCnt > 0) {
            bool hasNeighbor = (f > FILE_A && (bPawns & fileBb(File(f - 1)))) ||
                               (f < FILE_H && (bPawns & fileBb(File(f + 1))));
            if (!hasNeighbor) {
                int penalty = (f == FILE_A || f == FILE_H) ? ISOLATED_PAWN_PENALTY / 2 : ISOLATED_PAWN_PENALTY;
                score += bCnt * penalty;
            }
        }
    }

    // Passed pawns
    Bitboard wp = wPawns;
    while (wp) {
        Square s = popLsb(wp);
        File f = fileOf(s);
        Rank r = rankOf(s);
        Bitboard front = 0;
        for (Rank rr = Rank(r + 1); rr <= RANK_8; rr = Rank(rr + 1)) {
            front |= squareBb(makeSquare(f, rr));
            if (f > FILE_A) front |= squareBb(makeSquare(File(f - 1), rr));
            if (f < FILE_H) front |= squareBb(makeSquare(File(f + 1), rr));
        }
        if (!(front & bPawns))
            score += PASSED_PAWN_BONUS[r];
    }

    Bitboard bp = bPawns;
    while (bp) {
        Square s = popLsb(bp);
        File f = fileOf(s);
        Rank r = rankOf(s);
        Bitboard front = 0;
        for (Rank rr = RANK_1; rr < r; rr = Rank(rr + 1)) {
            front |= squareBb(makeSquare(f, rr));
            if (f > FILE_A) front |= squareBb(makeSquare(File(f - 1), rr));
            if (f < FILE_H) front |= squareBb(makeSquare(File(f + 1), rr));
        }
        if (!(front & wPawns))
            score -= PASSED_PAWN_BONUS[RANK_8 - r];
    }

    return score;
}

int Eval::mobility(const Board& board, int phase) const {
    int mgScore = 0, egScore = 0;
    Bitboard occ = board.occupied();

    // White
    Bitboard wFriendly = board.pieces(WHITE);
    Bitboard wKnights = board.pieces(WHITE, KNIGHT);
    while (wKnights) {
        int moves = popcount(attacks::knightAttacks(popLsb(wKnights)) & ~wFriendly);
        mgScore += moves * KNIGHT_MOBILITY_MG;
        egScore += moves * KNIGHT_MOBILITY_EG;
    }
    Bitboard wBishops = board.pieces(WHITE, BISHOP);
    while (wBishops) {
        int moves = popcount(attacks::bishopAttacks(popLsb(wBishops), occ) & ~wFriendly);
        mgScore += moves * BISHOP_MOBILITY_MG;
        egScore += moves * BISHOP_MOBILITY_EG;
    }
    Bitboard wRooks = board.pieces(WHITE, ROOK);
    while (wRooks) {
        int moves = popcount(attacks::rookAttacks(popLsb(wRooks), occ) & ~wFriendly);
        mgScore += moves * ROOK_MOBILITY_MG;
        egScore += moves * ROOK_MOBILITY_EG;
    }
    Bitboard wQueens = board.pieces(WHITE, QUEEN);
    while (wQueens) {
        int moves = popcount(attacks::queenAttacks(popLsb(wQueens), occ) & ~wFriendly);
        mgScore += moves * QUEEN_MOBILITY_MG;
        egScore += moves * QUEEN_MOBILITY_EG;
    }
    Bitboard wKings = board.pieces(WHITE, KING);
    while (wKings) {
        int moves = popcount(attacks::kingAttacks(popLsb(wKings)) & ~wFriendly);
        mgScore += moves * KING_MOBILITY_MG;
        egScore += moves * KING_MOBILITY_EG;
    }

    // Black
    Bitboard bFriendly = board.pieces(BLACK);
    Bitboard bKnights = board.pieces(BLACK, KNIGHT);
    while (bKnights) {
        int moves = popcount(attacks::knightAttacks(popLsb(bKnights)) & ~bFriendly);
        mgScore -= moves * KNIGHT_MOBILITY_MG;
        egScore -= moves * KNIGHT_MOBILITY_EG;
    }
    Bitboard bBishops = board.pieces(BLACK, BISHOP);
    while (bBishops) {
        int moves = popcount(attacks::bishopAttacks(popLsb(bBishops), occ) & ~bFriendly);
        mgScore -= moves * BISHOP_MOBILITY_MG;
        egScore -= moves * BISHOP_MOBILITY_EG;
    }
    Bitboard bRooks = board.pieces(BLACK, ROOK);
    while (bRooks) {
        int moves = popcount(attacks::rookAttacks(popLsb(bRooks), occ) & ~bFriendly);
        mgScore -= moves * ROOK_MOBILITY_MG;
        egScore -= moves * ROOK_MOBILITY_EG;
    }
    Bitboard bQueens = board.pieces(BLACK, QUEEN);
    while (bQueens) {
        int moves = popcount(attacks::queenAttacks(popLsb(bQueens), occ) & ~bFriendly);
        mgScore -= moves * QUEEN_MOBILITY_MG;
        egScore -= moves * QUEEN_MOBILITY_EG;
    }
    Bitboard bKings = board.pieces(BLACK, KING);
    while (bKings) {
        int moves = popcount(attacks::kingAttacks(popLsb(bKings)) & ~bFriendly);
        mgScore -= moves * KING_MOBILITY_MG;
        egScore -= moves * KING_MOBILITY_EG;
    }

    return tapered(mgScore, egScore, phase);
}

int Eval::bishopPair(const Board& board, int phase) const {
    int mgScore = 0, egScore = 0;

    if (popcount(board.pieces(WHITE, BISHOP)) >= 2) {
        mgScore += BISHOP_PAIR_MG;
        egScore += BISHOP_PAIR_EG;
    }
    if (popcount(board.pieces(BLACK, BISHOP)) >= 2) {
        mgScore -= BISHOP_PAIR_MG;
        egScore -= BISHOP_PAIR_EG;
    }


    return tapered(mgScore, egScore, phase);
}

int Eval::rookOnFile(const Board& board, int phase) const {
    int mgScore = 0, egScore = 0;
    Bitboard wPawns = board.pieces(WHITE, PAWN);
    Bitboard bPawns = board.pieces(BLACK, PAWN);

    Bitboard wRooks = board.pieces(WHITE, ROOK);
    while (wRooks) {
        Square sq = popLsb(wRooks);
        File f = fileOf(sq);
        Bitboard fileMask = fileBb(f);
        bool hasWPawn = fileMask & wPawns;
        bool hasBPawn = fileMask & bPawns;
        if (!hasWPawn) {
            if (!hasBPawn) {
                mgScore += ROOK_OPEN_FILE_MG;
                egScore += ROOK_OPEN_FILE_EG;
            } else {
                mgScore += ROOK_SEMI_OPEN_FILE_MG;
                egScore += ROOK_SEMI_OPEN_FILE_EG;
            }
        }
    }

    Bitboard bRooks = board.pieces(BLACK, ROOK);
    while (bRooks) {
        Square sq = popLsb(bRooks);
        File f = fileOf(sq);
        Bitboard fileMask = fileBb(f);
        bool hasWPawn = fileMask & wPawns;
        bool hasBPawn = fileMask & bPawns;
        if (!hasBPawn) {
            if (!hasWPawn) {
                mgScore -= ROOK_OPEN_FILE_MG;
                egScore -= ROOK_OPEN_FILE_EG;
            } else {
                mgScore -= ROOK_SEMI_OPEN_FILE_MG;
                egScore -= ROOK_SEMI_OPEN_FILE_EG;
            }
        }
    }


    return tapered(mgScore, egScore, phase);
}

int Eval::kingSafety(const Board& board, int phase) const {
    int mgScore = 0;

    // White king shield + attack weight
    Square wKing = board.kingSquare(WHITE);
    if (wKing == SQ_NONE) return 0;
    {
        int kf = fileOf(wKing);
        int kr = rankOf(wKing);
        int fStart = std::max(int(FILE_A), kf - 1);
        int fEnd   = std::min(int(FILE_H), kf + 1);

        int shield = 0;
        int rStart = kr + 1;
        int rEnd   = std::min(int(RANK_8), kr + 2);
        for (int f = fStart; f <= fEnd; f++) {
            for (int r = rStart; r <= rEnd; r++) {
                if (board.pieces(WHITE, PAWN) & squareBb(makeSquare(File(f), Rank(r))))
                    shield++;
            }
        }
        mgScore += shield * KING_SHIELD_BONUS;

        // Open file penalty near king (when opponent has major pieces)
        Bitboard enemyMajor = board.pieces(BLACK, QUEEN) | board.pieces(BLACK, ROOK);
        if (enemyMajor) {
            for (int f = fStart; f <= fEnd; f++) {
                if (!(board.pieces(WHITE, PAWN) & fileBb(File(f))))
                    mgScore -= KING_OPEN_FILE_PENALTY;
            }
        }

        // Attack units on king zone
        {
            Bitboard kingZone = attacks::kingAttacks(wKing) | squareBb(wKing);
            Bitboard occ = board.occupied();
            int attackUnits = 0;

            Bitboard pieces = board.pieces(BLACK, KNIGHT);
            while (pieces) {
                if (attacks::knightAttacks(popLsb(pieces)) & kingZone)
                    attackUnits += KING_ATTACK_KNIGHT;
            }
            pieces = board.pieces(BLACK, BISHOP);
            while (pieces) {
                if (attacks::bishopAttacks(popLsb(pieces), occ) & kingZone)
                    attackUnits += KING_ATTACK_BISHOP;
            }
            pieces = board.pieces(BLACK, ROOK);
            while (pieces) {
                if (attacks::rookAttacks(popLsb(pieces), occ) & kingZone)
                    attackUnits += KING_ATTACK_ROOK;
            }
            pieces = board.pieces(BLACK, QUEEN);
            while (pieces) {
                Square qsq = popLsb(pieces);
                if (attacks::queenAttacks(qsq, occ) & kingZone)
                    attackUnits += KING_ATTACK_QUEEN;
                int dist = squareDistance(wKing, qsq);
                if (dist < 5) attackUnits += (5 - dist) * KING_ATTACK_PROXIMITY;
            }

            if (attackUnits > 1)
                mgScore -= (attackUnits * attackUnits) / 4;
        }
    }

    // Black king shield + attack weight
    Square bKing = board.kingSquare(BLACK);
    if (bKing == SQ_NONE) return 0;
    {
        int kf = fileOf(bKing);
        int kr = rankOf(bKing);
        int fStart = std::max(int(FILE_A), kf - 1);
        int fEnd   = std::min(int(FILE_H), kf + 1);

        int shield = 0;
        int rStart = std::max(int(RANK_1), kr - 2);
        int rEnd   = kr - 1;
        for (int f = fStart; f <= fEnd; f++) {
            for (int r = rStart; r <= rEnd; r++) {
                if (board.pieces(BLACK, PAWN) & squareBb(makeSquare(File(f), Rank(r))))
                    shield++;
            }
        }
        mgScore -= shield * KING_SHIELD_BONUS;

        // Open file penalty near king (when opponent has major pieces)
        Bitboard enemyMajor = board.pieces(WHITE, QUEEN) | board.pieces(WHITE, ROOK);
        if (enemyMajor) {
            for (int f = fStart; f <= fEnd; f++) {
                if (!(board.pieces(BLACK, PAWN) & fileBb(File(f))))
                    mgScore += KING_OPEN_FILE_PENALTY;
            }
        }

        // Attack units on king zone
        {
            Bitboard kingZone = attacks::kingAttacks(bKing) | squareBb(bKing);
            Bitboard occ = board.occupied();
            int attackUnits = 0;

            Bitboard pieces = board.pieces(WHITE, KNIGHT);
            while (pieces) {
                if (attacks::knightAttacks(popLsb(pieces)) & kingZone)
                    attackUnits += KING_ATTACK_KNIGHT;
            }
            pieces = board.pieces(WHITE, BISHOP);
            while (pieces) {
                if (attacks::bishopAttacks(popLsb(pieces), occ) & kingZone)
                    attackUnits += KING_ATTACK_BISHOP;
            }
            pieces = board.pieces(WHITE, ROOK);
            while (pieces) {
                if (attacks::rookAttacks(popLsb(pieces), occ) & kingZone)
                    attackUnits += KING_ATTACK_ROOK;
            }
            pieces = board.pieces(WHITE, QUEEN);
            while (pieces) {
                Square qsq = popLsb(pieces);
                if (attacks::queenAttacks(qsq, occ) & kingZone)
                    attackUnits += KING_ATTACK_QUEEN;
                int dist = squareDistance(bKing, qsq);
                if (dist < 5) attackUnits += (5 - dist) * KING_ATTACK_PROXIMITY;
            }

            if (attackUnits > 1)
                mgScore += (attackUnits * attackUnits) / 4;
        }
    }

    // King safety is mainly a middlegame concern
    return (mgScore * phase) / TOTAL_PHASE;
}

int Eval::tempo(const Board& board) const {
    return board.sideToMove() == WHITE ? TEMPO_BONUS : -TEMPO_BONUS;
}

int Eval::gamePhase(const Board& board) const {
    return board.gamePhaseScore();
}

int Eval::evaluate(const Board& board) const {
    size_t idx = board.hash() & (EVAL_CACHE_SIZE - 1);
    EvalCacheEntry& entry = evalCache_[idx];
    if (entry.valid && entry.hash == board.hash() && entry.pawnHash == board.pawnHash())
        return entry.score;

    int phase = gamePhase(board);
    if (phase > TOTAL_PHASE) phase = TOTAL_PHASE;
    int score = material(board) + pieceSquare(board, phase) +
                cachedPawnStructure(board) + mobility(board, phase) +
                bishopPair(board, phase) + rookOnFile(board, phase) +
                kingSafety(board, phase) + tempo(board);

    entry.hash = board.hash();
    entry.pawnHash = board.pawnHash();
    entry.score = score;
    entry.valid = true;
    return score;
}

} // namespace chess
