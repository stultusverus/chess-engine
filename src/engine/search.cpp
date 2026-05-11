#include "search.h"
#include "board.h"
#include <algorithm>
#include <chrono>
#include <fstream>
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
    stop_.store(false);
}

void Search::setInfinite(bool inf) {
    infinite_ = inf;
    stop_.store(false);
}

void Search::stop() { stop_.store(true); }

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

    startTime_ = std::chrono::steady_clock::now();

    for (int depth = 1; depth <= maxDepth && depth < MAX_PLY - 20 && !stop_.load(); depth++) {
        int alpha = -INF;
        int beta = INF;

        if (depth >= 4) {
            alpha = result.score - 30;
            beta = result.score + 30;
        }

        bool research = false;
        do {
            int score = alphaBeta(const_cast<Board&>(board), depth, alpha, beta, 0);
            if (stop_.load()) break;

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
        } while (research && !stop_.load());

        if (stop_.load()) break;

        result.depth = depth;
        result.bestMove = bestMoveRoot_;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime_).count();

        static std::ofstream logFile("engine.err", std::ios::app);
        logFile << "info depth " << depth
                << " score cp " << result.score
                << " time " << elapsed
                << " nodes " << nodes_
                << " pv " << moveToString(result.bestMove)
                << std::endl;

        if (shouldStop()) {
            break;
        }
    }

    result.nodes = nodes_;
    return result;
}

int Search::alphaBeta(Board& board, int depth, int alpha, int beta, int ply) {
    if (stop_.load()) return 0;
    nodes_++;
    if (nodes_ >= nodesLimit_) {
        nodesLimit_ += 1024;
        if (shouldStop()) return 0;
    }

    // TT probe
    uint64_t hash = board.hash();
    const TTEntry* ttEntry = tt_.probe(hash);
    Move ttMove;
    int ttScore = 0;
    const int originalAlpha = alpha;

    if (ttEntry) {
        ttMove = tt_.unpackMove(ttEntry->move);
        ttScore = ttEntry->score;

        // Mate score adjustment
        if (ttScore > MATE - MAX_PLY) ttScore -= ply;
        else if (ttScore < -MATE + MAX_PLY) ttScore += ply;

        if (depth > 0 && ttEntry->depth >= depth) {
            if (ttEntry->bound == static_cast<uint8_t>(Bound::EXACT))
                return ttScore;
            if (ttEntry->bound == static_cast<uint8_t>(Bound::LOWER) && ttScore >= beta)
                return ttScore;
            if (ttEntry->bound == static_cast<uint8_t>(Bound::UPPER) && ttScore <= alpha)
                return ttScore;
        }
    }

    if (depth >= 3 && ttMove.from == SQ_NONE) {
        alphaBeta(board, depth - 2, alpha, beta, ply);
        const TTEntry* newEntry = tt_.probe(hash);
        if (newEntry) ttMove = tt_.unpackMove(newEntry->move);
    }

    if (depth <= 0) return quiesce(board, alpha, beta, ply);

    // Null move pruning (skip when searching mate lines)
    if (depth >= 4 && !board.isInCheck() && beta < MATE - MAX_PLY) {
        Bitboard stmAll = board.pieces(board.sideToMove());
        Bitboard stmPawnKing = board.pieces(board.sideToMove(), KING) | board.pieces(board.sideToMove(), PAWN);
        if (stmAll != stmPawnKing) {
            NullUndo nullUndo;
            board.makeNullMove(nullUndo);
            int R = 3 + depth / 4;
            int score = -alphaBeta(board, depth - 1 - R, -beta, -beta + 1, ply + 1);
            board.unmakeNullMove(nullUndo);
            if (score >= beta) return beta;
        }
    }

    MoveList moves;
    gen_.generateMoves(board, moves);

    bool inCheck = board.isInCheck();
    if (inCheck) depth++;

    if (moves.size() == 0) {
        return inCheck ? -MATE + ply : 0;
    }

    // Try TT move first
    if (ttMove.from != SQ_NONE) {
        for (int i = 0; i < moves.size(); i++) {
            if (moves[i].from == ttMove.from && moves[i].to == ttMove.to) {
                std::swap(moves[0], moves[i]);
                break;
            }
        }
    }

    sortMoves(moves, board, ply);

    int bestScore = -INF;
    int movesMade = 0;
    Move bestMoveInNode;
    int staticEval = -INF;

    for (const Move& m : moves) {
        // Futility pruning
        if (depth <= 2 && !inCheck && movesMade >= 1 &&
            m.type != CAPTURE && m.type != EN_PASSANT &&
            m.type != PROMOTION && m.type != PROMOTION_CAPTURE) {
            if (staticEval == -INF) {
                staticEval = eval_.evaluate(board);
                if (board.sideToMove() == BLACK) staticEval = -staticEval;
            }
            int margin = (depth == 1) ? 200 : 600;
            if (staticEval + margin < alpha) continue;
        }

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

        if (stop_.load()) return 0;

        if (score > bestScore) {
            bestScore = score;
            bestMoveInNode = m;
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

    // Store in TT (only if we found at least one legal move)
    if (bestScore > -INF + 1) {
        Bound b;
        if (bestScore <= originalAlpha)
            b = Bound::UPPER;
        else if (bestScore >= beta)
            b = Bound::LOWER;
        else
            b = Bound::EXACT;

        int ttStoreScore = bestScore;
        if (ttStoreScore > MATE - MAX_PLY) ttStoreScore += ply;
        else if (ttStoreScore < -MATE + MAX_PLY) ttStoreScore -= ply;

        tt_.store(hash, static_cast<int16_t>(ttStoreScore), static_cast<int8_t>(depth), b, bestMoveInNode);
    }

    return bestScore;
}

int Search::quiesce(Board& board, int alpha, int beta, int ply) {
    if (stop_.load()) return 0;
    nodes_++;
    if (nodes_ >= nodesLimit_) {
        nodesLimit_ += 1024;
        if (shouldStop()) return 0;
    }

    int standPat = eval_.evaluate(board);
    if (board.sideToMove() == BLACK) standPat = -standPat;
    if (standPat >= beta) return beta;
    if (standPat > alpha) alpha = standPat;

    bool inCheck = board.isInCheck();

    // Delta pruning: if even capturing the queen can't reach alpha, prune
    int delta = Eval::QUEEN_VALUE + 100;
    if (standPat + delta < alpha && !inCheck) return alpha;

    MoveList moves;
    gen_.generateMoves(board, moves);

    MoveList searchMoves;
    if (inCheck) {
        searchMoves = moves;
    } else {
        for (const Move& m : moves) {
            if (m.type == CAPTURE || m.type == EN_PASSANT || m.type == PROMOTION_CAPTURE)
                searchMoves.add(m);
            else if (m.type == PROMOTION)
                searchMoves.add(m);
        }
    }

    sortMoves(searchMoves, board, ply);

    for (const Move& m : searchMoves) {
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

        if (stop_.load()) return 0;

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

bool Search::shouldStop() {
    if (stop_.load()) return true;
    if (!infinite_ && timeMs_ > 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime_).count();
        if (elapsed >= timeMs_) {
            stop_.store(true);
            return true;
        }
    }
    return false;
}

} // namespace chess
