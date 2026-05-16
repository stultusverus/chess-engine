#include "eval.h"
#include "board.h"
#include "attacks.h"
#include <algorithm>

namespace chess {

// --- Helpers ---
static constexpr Bitboard fileBb(File f) {
    return 0x0101010101010101ULL << f;
}

// Default params instance for static helpers
static const EvalParams& defaultParams() {
    static const EvalParams p;
    return p;
}

int Eval::pieceSquareMg(Piece p, Square s) {
    const EvalParams& par = defaultParams();
    PieceType pt = typeOf(p);
    if (colorOf(p) == WHITE)
        return par.mgTable(pt)[s];
    return -par.mgTable(pt)[s ^ 56];
}

int Eval::pieceSquareEg(Piece p, Square s) {
    const EvalParams& par = defaultParams();
    PieceType pt = typeOf(p);
    if (colorOf(p) == WHITE)
        return par.egTable(pt)[s];
    return -par.egTable(pt)[s ^ 56];
}

int Eval::phaseValue(PieceType pt) {
    return defaultParams().phaseInc[pt];
}

int Eval::material(const Board& board) const {
    return board.materialScore();
}

int Eval::material(const Board& board, const EvalParams& params) const {
    int score = 0;
    for (int pt = PAWN; pt <= QUEEN; pt++) {
        PieceType ptype = PieceType(pt);
        int val = params.pieceValue(ptype);
        score += popcount(board.pieces(WHITE, ptype)) * val;
        score -= popcount(board.pieces(BLACK, ptype)) * val;
    }
    return score;
}

int Eval::pieceSquare(const Board& board, int phase) const {
    return tapered(board.pstMgScore(), board.pstEgScore(), phase);
}

int Eval::pieceSquare(const Board& board, const EvalParams& params, int phase) const {
    int mg = 0, eg = 0;
    for (int pt = PAWN; pt <= KING; pt++) {
        PieceType ptype = PieceType(pt);
        const int* mgTable = params.mgTable(ptype);
        const int* egTable = params.egTable(ptype);
        Bitboard wPieces = board.pieces(WHITE, ptype);
        while (wPieces) {
            Square s = popLsb(wPieces);
            mg += mgTable[s];
            eg += egTable[s];
        }
        Bitboard bPieces = board.pieces(BLACK, ptype);
        while (bPieces) {
            Square s = popLsb(bPieces);
            mg -= mgTable[s ^ 56];
            eg -= egTable[s ^ 56];
        }
    }
    int totalPhase = params.totalPhase;
    return (mg * phase + eg * (totalPhase - phase)) / totalPhase;
}

int Eval::tapered(int mgScore, int egScore, int phase) const {
    return (mgScore * phase + egScore * (params_.totalPhase - phase)) / params_.totalPhase;
}

int Eval::gamePhase(const Board& board) const {
    return board.gamePhaseScore();
}

int Eval::gamePhase(const Board& board, const EvalParams& params) const {
    int phase = 0;
    for (int pt = KNIGHT; pt <= QUEEN; pt++) {
        PieceType ptype = PieceType(pt);
        int inc = params.phaseInc[pt];
        phase += popcount(board.pieces(WHITE, ptype)) * inc;
        phase += popcount(board.pieces(BLACK, ptype)) * inc;
    }
    return std::min(phase, params.totalPhase);
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

        if (wCnt > 1) score -= (wCnt - 1) * params_.doubledPawnPenalty;
        if (bCnt > 1) score += (bCnt - 1) * params_.doubledPawnPenalty;

        if (wCnt > 0) {
            bool hasNeighbor = (f > FILE_A && (wPawns & fileBb(File(f - 1)))) ||
                               (f < FILE_H && (wPawns & fileBb(File(f + 1))));
            if (!hasNeighbor) {
                int penalty = (f == FILE_A || f == FILE_H) ? params_.isolatedPawnPenalty / 2 : params_.isolatedPawnPenalty;
                score -= wCnt * penalty;
            }
        }
        if (bCnt > 0) {
            bool hasNeighbor = (f > FILE_A && (bPawns & fileBb(File(f - 1)))) ||
                               (f < FILE_H && (bPawns & fileBb(File(f + 1))));
            if (!hasNeighbor) {
                int penalty = (f == FILE_A || f == FILE_H) ? params_.isolatedPawnPenalty / 2 : params_.isolatedPawnPenalty;
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
            score += params_.passedPawnBonus[r];
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
            score -= params_.passedPawnBonus[RANK_8 - r];
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
        mgScore += moves * params_.knightMobilityMg;
        egScore += moves * params_.knightMobilityEg;
    }
    Bitboard wBishops = board.pieces(WHITE, BISHOP);
    while (wBishops) {
        int moves = popcount(attacks::bishopAttacks(popLsb(wBishops), occ) & ~wFriendly);
        mgScore += moves * params_.bishopMobilityMg;
        egScore += moves * params_.bishopMobilityEg;
    }
    Bitboard wRooks = board.pieces(WHITE, ROOK);
    while (wRooks) {
        int moves = popcount(attacks::rookAttacks(popLsb(wRooks), occ) & ~wFriendly);
        mgScore += moves * params_.rookMobilityMg;
        egScore += moves * params_.rookMobilityEg;
    }
    Bitboard wQueens = board.pieces(WHITE, QUEEN);
    while (wQueens) {
        int moves = popcount(attacks::queenAttacks(popLsb(wQueens), occ) & ~wFriendly);
        mgScore += moves * params_.queenMobilityMg;
        egScore += moves * params_.queenMobilityEg;
    }
    Bitboard wKings = board.pieces(WHITE, KING);
    while (wKings) {
        int moves = popcount(attacks::kingAttacks(popLsb(wKings)) & ~wFriendly);
        mgScore += moves * params_.kingMobilityMg;
        egScore += moves * params_.kingMobilityEg;
    }

    // Black
    Bitboard bFriendly = board.pieces(BLACK);
    Bitboard bKnights = board.pieces(BLACK, KNIGHT);
    while (bKnights) {
        int moves = popcount(attacks::knightAttacks(popLsb(bKnights)) & ~bFriendly);
        mgScore -= moves * params_.knightMobilityMg;
        egScore -= moves * params_.knightMobilityEg;
    }
    Bitboard bBishops = board.pieces(BLACK, BISHOP);
    while (bBishops) {
        int moves = popcount(attacks::bishopAttacks(popLsb(bBishops), occ) & ~bFriendly);
        mgScore -= moves * params_.bishopMobilityMg;
        egScore -= moves * params_.bishopMobilityEg;
    }
    Bitboard bRooks = board.pieces(BLACK, ROOK);
    while (bRooks) {
        int moves = popcount(attacks::rookAttacks(popLsb(bRooks), occ) & ~bFriendly);
        mgScore -= moves * params_.rookMobilityMg;
        egScore -= moves * params_.rookMobilityEg;
    }
    Bitboard bQueens = board.pieces(BLACK, QUEEN);
    while (bQueens) {
        int moves = popcount(attacks::queenAttacks(popLsb(bQueens), occ) & ~bFriendly);
        mgScore -= moves * params_.queenMobilityMg;
        egScore -= moves * params_.queenMobilityEg;
    }
    Bitboard bKings = board.pieces(BLACK, KING);
    while (bKings) {
        int moves = popcount(attacks::kingAttacks(popLsb(bKings)) & ~bFriendly);
        mgScore -= moves * params_.kingMobilityMg;
        egScore -= moves * params_.kingMobilityEg;
    }

    return tapered(mgScore, egScore, phase);
}

int Eval::bishopPair(const Board& board, int phase) const {
    int mgScore = 0, egScore = 0;

    if (popcount(board.pieces(WHITE, BISHOP)) >= 2) {
        mgScore += params_.bishopPairMg;
        egScore += params_.bishopPairEg;
    }
    if (popcount(board.pieces(BLACK, BISHOP)) >= 2) {
        mgScore -= params_.bishopPairMg;
        egScore -= params_.bishopPairEg;
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
                mgScore += params_.rookOpenFileMg;
                egScore += params_.rookOpenFileEg;
            } else {
                mgScore += params_.rookSemiOpenFileMg;
                egScore += params_.rookSemiOpenFileEg;
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
                mgScore -= params_.rookOpenFileMg;
                egScore -= params_.rookOpenFileEg;
            } else {
                mgScore -= params_.rookSemiOpenFileMg;
                egScore -= params_.rookSemiOpenFileEg;
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
        mgScore += shield * params_.kingShieldBonus;

        // Open file penalty near king (when opponent has major pieces)
        Bitboard enemyMajor = board.pieces(BLACK, QUEEN) | board.pieces(BLACK, ROOK);
        if (enemyMajor) {
            for (int f = fStart; f <= fEnd; f++) {
                if (!(board.pieces(WHITE, PAWN) & fileBb(File(f))))
                    mgScore -= params_.kingOpenFilePenalty;
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
                    attackUnits += params_.kingAttackKnight;
            }
            pieces = board.pieces(BLACK, BISHOP);
            while (pieces) {
                if (attacks::bishopAttacks(popLsb(pieces), occ) & kingZone)
                    attackUnits += params_.kingAttackBishop;
            }
            pieces = board.pieces(BLACK, ROOK);
            while (pieces) {
                if (attacks::rookAttacks(popLsb(pieces), occ) & kingZone)
                    attackUnits += params_.kingAttackRook;
            }
            pieces = board.pieces(BLACK, QUEEN);
            while (pieces) {
                Square qsq = popLsb(pieces);
                if (attacks::queenAttacks(qsq, occ) & kingZone)
                    attackUnits += params_.kingAttackQueen;
                int dist = squareDistance(wKing, qsq);
                if (dist < 5) attackUnits += (5 - dist) * params_.kingAttackProximity;
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
        mgScore -= shield * params_.kingShieldBonus;

        // Open file penalty near king (when opponent has major pieces)
        Bitboard enemyMajor = board.pieces(WHITE, QUEEN) | board.pieces(WHITE, ROOK);
        if (enemyMajor) {
            for (int f = fStart; f <= fEnd; f++) {
                if (!(board.pieces(BLACK, PAWN) & fileBb(File(f))))
                    mgScore += params_.kingOpenFilePenalty;
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
                    attackUnits += params_.kingAttackKnight;
            }
            pieces = board.pieces(WHITE, BISHOP);
            while (pieces) {
                if (attacks::bishopAttacks(popLsb(pieces), occ) & kingZone)
                    attackUnits += params_.kingAttackBishop;
            }
            pieces = board.pieces(WHITE, ROOK);
            while (pieces) {
                if (attacks::rookAttacks(popLsb(pieces), occ) & kingZone)
                    attackUnits += params_.kingAttackRook;
            }
            pieces = board.pieces(WHITE, QUEEN);
            while (pieces) {
                Square qsq = popLsb(pieces);
                if (attacks::queenAttacks(qsq, occ) & kingZone)
                    attackUnits += params_.kingAttackQueen;
                int dist = squareDistance(bKing, qsq);
                if (dist < 5) attackUnits += (5 - dist) * params_.kingAttackProximity;
            }

            if (attackUnits > 1)
                mgScore += (attackUnits * attackUnits) / 4;
        }
    }

    // King safety is mainly a middlegame concern
    return (mgScore * phase) / params_.totalPhase;
}

int Eval::tempo(const Board& board) const {
    return board.sideToMove() == WHITE ? params_.tempoBonus : -params_.tempoBonus;
}

int Eval::evaluate(const Board& board) const {
    size_t idx = board.hash() & (EVAL_CACHE_SIZE - 1);
    EvalCacheEntry& entry = evalCache_[idx];
    if (entry.valid && entry.hash == board.hash() && entry.pawnHash == board.pawnHash())
        return entry.score;

    int phase = gamePhase(board);
    if (phase > params_.totalPhase) phase = params_.totalPhase;
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

int Eval::evaluate(const Board& board, const EvalParams& params) {
    EvalParams saved = params_;
    params_ = params;

    int phase = gamePhase(board, params);
    int score = material(board, params) + pieceSquare(board, params, phase) +
                pawnStructure(board) + mobility(board, phase) +
                bishopPair(board, phase) + rookOnFile(board, phase) +
                kingSafety(board, phase) + tempo(board);

    params_ = saved;
    return score;
}

EvalTrace Eval::trace(const Board& board) const {
    EvalTrace t;
    int phase = gamePhase(board);
    if (phase > TOTAL_PHASE) phase = TOTAL_PHASE;
    t.phase = phase;
    t.material = material(board);
    t.pieceSquare = pieceSquare(board, phase);
    t.pawnStructure = pawnStructure(board); // bypass cache for accurate trace
    t.mobility = mobility(board, phase);
    t.bishopPair = bishopPair(board, phase);
    t.rookFile = rookOnFile(board, phase);
    t.kingSafety = kingSafety(board, phase);
    t.tempo = tempo(board);
    t.total = t.material + t.pieceSquare + t.pawnStructure +
              t.mobility + t.bishopPair + t.rookFile +
              t.kingSafety + t.tempo;
    return t;
}

void Eval::clearCaches() {
    for (auto& entry : pawnCache_)
        entry.valid = false;
    for (auto& entry : evalCache_)
        entry.valid = false;
}

} // namespace chess
