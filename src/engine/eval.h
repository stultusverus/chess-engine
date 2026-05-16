#pragma once

#include "types.h"
#include "eval_params.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace chess {

class Board;

struct EvalTrace {
    int phase = 0;
    int material = 0;
    int pieceSquare = 0;
    int pawnStructure = 0;
    int mobility = 0;
    int bishopPair = 0;
    int rookFile = 0;
    int kingSafety = 0;
    int tempo = 0;
    int total = 0;

    struct Entry {
        std::string name;
        int value;
    };

    std::vector<Entry> entries() const {
        return {
            {"phase",        phase},
            {"material",     material},
            {"pieceSquare",  pieceSquare},
            {"pawnStructure",pawnStructure},
            {"mobility",     mobility},
            {"bishopPair",   bishopPair},
            {"rookFile",     rookFile},
            {"kingSafety",   kingSafety},
            {"tempo",        tempo},
            {"total",        total},
        };
    }
};

class Eval {
public:
    // Piece values exposed for SEE and other modules
    static constexpr int PAWN_VALUE   = 100;
    static constexpr int KNIGHT_VALUE = 320;
    static constexpr int BISHOP_VALUE = 330;
    static constexpr int ROOK_VALUE   = 500;
    static constexpr int QUEEN_VALUE  = 900;
    static constexpr int KING_VALUE   = 20000;

    static constexpr int pieceValue(PieceType pt) {
        constexpr int vals[] = {PAWN_VALUE, KNIGHT_VALUE, BISHOP_VALUE, ROOK_VALUE, QUEEN_VALUE, KING_VALUE};
        return vals[pt];
    }

    static constexpr int TOTAL_PHASE = 24;

    static int pieceSquareMg(Piece p, Square s);
    static int pieceSquareEg(Piece p, Square s);
    static int phaseValue(PieceType pt);

    // Evaluate position from white's perspective (positive = white advantage)
    int evaluate(const Board& board) const;

    // Parameterised evaluation: computes material, PST, and phase from the
    // supplied EvalParams instead of from the board's incremental state.
    // Other terms (pawn structure, mobility, etc.) use this instance's params_.
    // Does NOT use or populate caches — intended for tuning/loss computation.
    int evaluate(const Board& board, const EvalParams& params) const;

    // Access to tunable parameters (non-const for future tuning tools)
    EvalParams& params() { return params_; }
    const EvalParams& params() const { return params_; }
    // Trace: evaluate with per-term breakdown (bypasses caches for accuracy)
    EvalTrace trace(const Board& board) const;

private:
    struct PawnCacheEntry {
        uint64_t pawnHash = 0;
        int score = 0;
        bool valid = false;
    };

    struct EvalCacheEntry {
        uint64_t hash = 0;
        uint64_t pawnHash = 0;
        int score = 0;
        bool valid = false;
    };

    static constexpr int PAWN_CACHE_SIZE = 16384;
    static constexpr int EVAL_CACHE_SIZE = 32768;

    mutable std::array<PawnCacheEntry, PAWN_CACHE_SIZE> pawnCache_{};
    mutable std::array<EvalCacheEntry, EVAL_CACHE_SIZE> evalCache_{};
    EvalParams params_{};

    int material(const Board& board) const;
    int pieceSquare(const Board& board, int phase) const;
    int gamePhase(const Board& board) const;
    int tapered(int mgScore, int egScore, int phase) const;

    // Parameterised helpers: compute from explicit EvalParams, not board state
    int material(const Board& board, const EvalParams& params) const;
    int pieceSquare(const Board& board, const EvalParams& params, int phase) const;
    int gamePhase(const Board& board, const EvalParams& params) const;
    int cachedPawnStructure(const Board& board) const;
    int pawnStructure(const Board& board) const;
    int mobility(const Board& board, int phase) const;
    int bishopPair(const Board& board, int phase) const;
    int rookOnFile(const Board& board, int phase) const;
    int kingSafety(const Board& board, int phase) const;
    int tempo(const Board& board) const;
};

} // namespace chess
