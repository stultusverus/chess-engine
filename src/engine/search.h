#pragma once

#include "types.h"
#include "eval.h"
#include "movegen.h"
#include "tt.h"

namespace chess {

class Board;
struct UndoInfo;

struct SearchResult {
    Move bestMove;
    int score;
    int depth;
    uint64_t nodes;
};

class Search {
public:
    Search();

    void setTimeMs(int ms);
    void setInfinite(bool inf);
    void setTTSize(int mb) { tt_.setSize(mb); }
    void clearTT() { tt_.clear(); }
    void stop();
    bool isStopped() const { return stop_; }

    SearchResult search(const Board& board, int maxDepth = 64);

private:
    int alphaBeta(Board& board, int depth, int alpha, int beta, int ply);
    int quiesce(Board& board, int alpha, int beta, int ply);

    int scoreMove(const Board& board, Move m, int ply) const;
    void sortMoves(MoveList& moves, const Board& board, int ply);

    void ageHistory();
    void updateKiller(Move m, int ply);
    void updateHistory(Move m, int depth);

    // Time control
    int timeMs_ = 0;
    bool infinite_ = false;
    bool stop_ = false;
    uint64_t nodes_ = 0;
    uint64_t nodesLimit_ = 0;

    // Move ordering
    static constexpr int KILLER_SCORE = 1000;
    static constexpr int HISTORY_MAX = 16384;
    int killer1_[MAX_PLY]{};
    int killer2_[MAX_PLY]{};
    int history_[64][64]{};
    Move bestMoveRoot_;

    MoveGenerator gen_;
    Eval eval_;
    TranspositionTable tt_;
};

} // namespace chess
