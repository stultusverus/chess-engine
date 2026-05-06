#include "search.h"
#include "board.h"
#include <algorithm>
#include <chrono>
#include <iostream>

namespace chess {

static constexpr int INF = 1000000;
static constexpr int MATE = INF - 100;

Search::Search() {
    ageHistory();
}

void Search::setTimeMs(int ms) {
    timeMs_ = ms;
    infinite_ = false;
    stop_ = false;
}

void Search::setInfinite(bool inf) {
    infinite_ = inf;
    stop_ = false;
}

void Search::stop() { stop_ = true; }

SearchResult Search::search(const Board& board, int maxDepth) {
    SearchResult result;
    result.bestMove = Move();
    result.score = 0;
    result.depth = 0;
    result.nodes = 0;

    nodes_ = 0;
    bestMoveRoot_ = Move();
    nodesLimit_ = 1024;
    ageHistory();

    auto startTime = std::chrono::steady_clock::now();

    for (int depth = 1; depth <= maxDepth && depth < MAX_PLY - 20; depth++) {
        int alpha = -INF;
        int beta = INF;

        if (depth >= 4) {
            alpha = result.score - 30;
            beta = result.score + 30;
        }

        bool research = false;
        do {
            int score = alphaBeta(const_cast<Board&>(board), depth, alpha, beta, 0);
            if (stop_) break;

            if (score <= alpha) {
                alpha = -INF;
                research = true;
            } else if (score >= beta) {
                beta = INF;
                research = true;
            } else {
                research = false;
                result.score = score;
            }
        } while (research && !stop_);

        if (stop_) break;

        result.depth = depth;
        result.bestMove = bestMoveRoot_;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();

        std::cerr << "info depth " << depth
                  << " score cp " << result.score
                  << " time " << elapsed
                  << " nodes " << nodes_
                  << " pv " << moveToString(result.bestMove)
                  << std::endl;

        if (!infinite_ && timeMs_ > 0 && elapsed >= timeMs_ / 4) {
            stop_ = true;
            break;
        }
    }

    result.nodes = nodes_;
    return result;
}

int Search::alphaBeta(Board& board, int depth, int alpha, int beta, int ply) {
    nodes_++;
    if (nodes_ >= nodesLimit_) {
        nodesLimit_ += 1024;
        if (stop_) return 0;
    }

    if (depth <= 0) return quiesce(board, alpha, beta, ply);

    MoveList moves;
    gen_.generateMoves(board, moves);

    if (moves.size() == 0) {
        return board.isInCheck() ? -MATE + ply : 0;
    }

    sortMoves(moves, board, ply);

    int bestScore = -INF;
    int movesMade = 0;

    for (const Move& m : moves) {
        UndoInfo undo;
        if (!board.makeMove(m, undo)) continue;

        int score;
        if (movesMade == 0) {
            score = -alphaBeta(board, depth - 1, -beta, -alpha, ply + 1);
        } else {
            // Late-move reductions (LMR): reduce moves beyond the first few
            int reduction = 0;
            if (depth >= 3 && movesMade >= 4 && !(m.type == CAPTURE || m.type == EN_PASSANT || m.type == PROMOTION || m.type == PROMOTION_CAPTURE)) {
                reduction = 1;
                if (movesMade >= 8) reduction = 2;
            }
            score = -alphaBeta(board, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1);
            if (score > alpha && reduction > 0) {
                // Re-search at full depth
                score = -alphaBeta(board, depth - 1, -alpha - 1, -alpha, ply + 1);
            }
            if (score > alpha && score < beta) {
                score = -alphaBeta(board, depth - 1, -beta, -alpha, ply + 1);
            }
        }

        board.unmakeMove(m, undo);
        movesMade++;

        if (stop_) return 0;

        if (score > bestScore) {
            bestScore = score;
            if (score > alpha) {
                alpha = score;
                if (ply == 0) bestMoveRoot_ = m;
                if (alpha >= beta) {
                    updateKiller(m, ply);
                    updateHistory(m, depth);
                    break;
                }
            }
        }
    }

    return bestScore;
}

int Search::quiesce(Board& board, int alpha, int beta, int ply) {
    nodes_++;
    if (nodes_ >= nodesLimit_) {
        nodesLimit_ += 1024;
        if (stop_) return 0;
    }

    int standPat = eval_.evaluate(board);
    if (standPat >= beta) return beta;
    if (standPat > alpha) alpha = standPat;

    // Delta pruning: if even capturing the queen can't reach alpha, prune
    int delta = Eval::QUEEN_VALUE + 100;
    if (standPat + delta < alpha && !board.isInCheck()) return alpha;

    MoveList moves;
    gen_.generateMoves(board, moves);

    MoveList captures;
    for (const Move& m : moves) {
        if (m.type == CAPTURE || m.type == EN_PASSANT || m.type == PROMOTION_CAPTURE)
            captures.add(m);
        else if (m.type == PROMOTION)
            captures.add(m);
    }

    sortMoves(captures, board, ply);

    for (const Move& m : captures) {
        // Static exchange evaluation (SEE): skip losing captures in qsearch
        if (m.type == CAPTURE) {
            PieceType victim = typeOf(board.pieceOn(m.to));
            PieceType attacker = typeOf(board.pieceOn(m.from));
            if (Eval::pieceValue(victim) < Eval::pieceValue(attacker) && standPat + Eval::pieceValue(victim) + 200 < alpha)
                continue;
        }

        UndoInfo undo;
        if (!board.makeMove(m, undo)) continue;

        int score = -quiesce(board, -beta, -alpha, ply + 1);
        board.unmakeMove(m, undo);

        if (stop_) return 0;

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

int Search::scoreMove(const Board& board, Move m, int ply) const {
    int score = 0;

    if (m.type == CAPTURE || m.type == PROMOTION_CAPTURE) {
        PieceType victim = typeOf(board.pieceOn(m.to));
        PieceType attacker = typeOf(board.pieceOn(m.from));
        score = 100000 + Eval::pieceValue(victim) - Eval::pieceValue(attacker) / 10;
        if (m.type == PROMOTION_CAPTURE) score += Eval::pieceValue(m.promotion);
    } else if (m.type == EN_PASSANT) {
        score = 100000 + Eval::pieceValue(PAWN);
    } else if (m.type == PROMOTION) {
        score = 90000 + Eval::pieceValue(m.promotion);
    } else {
        int packed = m.from | (m.to << 6);
        if (packed == killer1_[ply])
            score = KILLER_SCORE;
        else if (packed == killer2_[ply])
            score = KILLER_SCORE - 1;
        if (score == 0)
            score = history_[m.from][m.to];
    }

    return score;
}

void Search::sortMoves(MoveList& moves, const Board& board, int ply) {
    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
        return scoreMove(board, a, ply) > scoreMove(board, b, ply);
    });
}

void Search::ageHistory() {
    for (int i = 0; i < 64; i++)
        for (int j = 0; j < 64; j++)
            history_[i][j] /= 2;
}

void Search::updateKiller(Move m, int ply) {
    int packed = m.from | (m.to << 6);
    if (packed != killer1_[ply]) {
        killer2_[ply] = killer1_[ply];
        killer1_[ply] = packed;
    }
}

void Search::updateHistory(Move m, int depth) {
    history_[m.from][m.to] += depth * depth;
    if (history_[m.from][m.to] > HISTORY_MAX)
        history_[m.from][m.to] = HISTORY_MAX;
}

} // namespace chess
