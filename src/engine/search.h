#pragma once

#include "types.h"
#include "eval.h"
#include "movegen.h"
#include "tt.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <vector>

namespace chess {

constexpr int MATE = 999900;

class Board;
struct UndoInfo;

struct SearchResult {
    Move bestMove;
    int score;
    int depth;
    uint64_t nodes;
    int timeMs = 0;
    uint64_t nps = 0;
    int hashFull = 0;
    std::vector<Move> pv;
};

struct NodeScratch {
    MoveList moves;
    Move quietsTried[MAX_MOVES]{};
    Piece quietPiecesTried[MAX_MOVES]{};
    Move capturesTried[MAX_MOVES]{};
    Piece capturePiecesTried[MAX_MOVES]{};
    PieceType capturedTypesTried[MAX_MOVES]{};
    bool used[MAX_MOVES]{};
    Move badNoisy[MAX_MOVES]{};
    int badNoisyScores[MAX_MOVES]{};
};

class Search {
public:
    Search();

    void setTimeMs(int ms);
    void setTimeControlMs(int softMs, int hardMs);
    void setInfinite(bool inf);
    void setNodeLimit(uint64_t nodes);
    void setTimeStartOverride(std::chrono::steady_clock::time_point startTime);
    void clearTimeStartOverride();
    void setRootMoves(const std::vector<Move>& moves);
    void clearRootMoves();
    void setInfoCallback(std::function<void(const SearchResult&)> callback);
    void setTTSize(int mb) { tt_.setSize(mb); }
    void clearTT() { tt_.clear(); }
    void stop();
    bool isStopped() const { return stop_.load(); }

    SearchResult search(const Board& board, int maxDepth = 64);

private:
    int alphaBeta(Board& board, int depth, int alpha, int beta, int ply);
    int quiesce(Board& board, int alpha, int beta, int ply);

    int noisyMovePreScore(const Board& board, Move m) const;
    int noisyMoveScore(const Board& board, Move m, int see) const;
    int quietMoveScore(const Board& board, Move m, int ply) const;
    bool isKillerMove(Move m, int ply) const;
    bool isCounterMove(Move m, int ply) const;
    bool rootMoveAllowed(Move m) const;
    std::vector<Move> extractPv(Board board, Move bestMove, int maxDepth) const;
    int elapsedMs() const;
    bool softTimeExpired() const;

    void ageHistory();
    void updateKiller(Move m, int ply);
    void updateCounterMove(Move m, int ply);
    void updateQuietHistory(Move m, Piece movedPiece, int depth, int ply, int sign);
    void updateCaptureHistory(Move m, Piece movedPiece, PieceType capturedType, int depth, int sign);
    void updateHistoryValue(int& value, int bonus);
    PieceType capturedTypeForMove(const Board& board, Move m) const;
    void setContinuationContext(int ply, Piece movedPiece, Move m);
    void clearContinuationContext(int ply);
    bool shouldStop();

    // Time control
    int softTimeMs_ = 0;
    int hardTimeMs_ = 0;
    bool infinite_ = false;
    std::atomic<bool> stop_{false};
    std::chrono::steady_clock::time_point startTime_{};
    std::chrono::steady_clock::time_point startTimeOverride_{};
    bool useStartTimeOverride_ = false;
    uint64_t nodes_ = 0;
    uint64_t nodesLimit_ = 0;
    uint64_t maxNodes_ = 0;

    // Move ordering
    static constexpr int KILLER_SCORE = 50000;
    static constexpr int COUNTER_MOVE_SCORE = 49900;
    static constexpr int HISTORY_MAX = 16384;
    int killer1_[MAX_PLY]{};
    int killer2_[MAX_PLY]{};
    int quietHistory_[COLOR_NB][64][64]{};
    int continuationHistory_[PIECE_NB][64][PIECE_NB][64]{};
    int captureHistory_[PIECE_NB][64][PIECE_TYPE_NB]{};
    Move counterMove_[PIECE_NB][64]{};
    Piece continuationPieceByPly_[MAX_PLY]{};
    Square continuationToByPly_[MAX_PLY]{};
    Move bestMoveRoot_;
    std::vector<Move> rootMoves_;
    std::function<void(const SearchResult&)> infoCallback_;

    std::vector<NodeScratch> searchScratch_;
    std::vector<NodeScratch> qsearchScratch_;

    MoveGenerator gen_;
    Eval eval_;
    TranspositionTable tt_;
};

} // namespace chess
