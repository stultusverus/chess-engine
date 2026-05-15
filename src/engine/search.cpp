#include "search.h"
#include "board.h"
#include "see.h"
#include <algorithm>
#include <chrono>

namespace chess {

static constexpr int INF = 1000000;

namespace {

bool isCaptureMove(Move m) {
    return m.type == CAPTURE || m.type == PROMOTION_CAPTURE || m.type == EN_PASSANT;
}

bool isQuietHistoryMove(Move m) {
    return !isCaptureMove(m) && m.type != PROMOTION && m.type != PROMOTION_CAPTURE;
}

bool isNoisyOrPromotion(Move m) {
    return isCaptureMove(m) || m.type == PROMOTION;
}

bool sameMove(Move a, Move b) {
    return a.from == b.from && a.to == b.to && a.promotion == b.promotion;
}

bool hasNonPawnMaterial(const Board& board, Color side) {
    Bitboard pieces = board.pieces(side);
    Bitboard pawnsAndKing = board.pieces(side, PAWN) | board.pieces(side, KING);
    return pieces != pawnsAndKing;
}

} // namespace

Search::Search() {
    tt_.setSize(64);
    searchScratch_.resize(MAX_PLY);
    qsearchScratch_.resize(MAX_PLY);
    ageHistory();
}

void Search::setTimeMs(int ms) {
    setTimeControlMs(ms, ms);
}

void Search::setTimeControlMs(int softMs, int hardMs, bool adaptiveTimeManagement) {
    softTimeMs_ = std::max(0, softMs);
    hardTimeMs_ = std::max(softTimeMs_, hardMs);
    adaptiveTimeManagement_ = adaptiveTimeManagement;
    infinite_ = false;
    stop_.store(false);
    maxNodes_ = 0;
    clearTimeStartOverride();
}

void Search::setInfinite(bool inf) {
    infinite_ = inf;
    softTimeMs_ = 0;
    hardTimeMs_ = 0;
    adaptiveTimeManagement_ = false;
    stop_.store(false);
    maxNodes_ = 0;
    clearTimeStartOverride();
}

void Search::setNodeLimit(uint64_t nodes) {
    maxNodes_ = nodes;
    softTimeMs_ = 0;
    hardTimeMs_ = 0;
    infinite_ = false;
    adaptiveTimeManagement_ = false;
    stop_.store(false);
    clearTimeStartOverride();
}

void Search::setTimeStartOverride(std::chrono::steady_clock::time_point startTime) {
    startTimeOverride_ = startTime;
    useStartTimeOverride_ = true;
}

void Search::clearTimeStartOverride() {
    useStartTimeOverride_ = false;
}

void Search::setRootMoves(const std::vector<Move>& moves) {
    rootMoves_ = moves;
}

void Search::clearRootMoves() {
    rootMoves_.clear();
}

void Search::setInfoCallback(std::function<void(const SearchResult&)> callback) {
    infoCallback_ = std::move(callback);
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
    tt_.newSearch();
    std::fill(std::begin(continuationPieceByPly_), std::end(continuationPieceByPly_), NO_PIECE);
    std::fill(std::begin(continuationToByPly_), std::end(continuationToByPly_), SQ_NONE);
    ageHistory();

    prevBestMove_ = Move();
    prevScore_ = 0;
    stableIterations_ = 0;
    originalSoftTimeMs_ = softTimeMs_;

    startTime_ = useStartTimeOverride_ ? startTimeOverride_ : std::chrono::steady_clock::now();
    Board rootBoard = board;

    for (int depth = 1; depth <= maxDepth && depth < MAX_PLY - 20; depth++) {
        if (stop_.load() && depth > 1) break;
        int alpha = -INF;
        int beta = INF;

        if (depth >= 4) {
            alpha = result.score - 30;
            beta = result.score + 30;
        }

        bool research = false;
        do {
            int score = alphaBeta(rootBoard, depth, alpha, beta, 0);
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

        if (stop_.load()) {
            if (result.bestMove.from == SQ_NONE && bestMoveRoot_.from != SQ_NONE)
                result.bestMove = bestMoveRoot_;
            break;
        }

        result.depth = depth;
        result.bestMove = bestMoveRoot_;
        result.nodes = nodes_;
        result.timeMs = elapsedMs();
        result.nps = result.timeMs > 0 ? nodes_ * 1000ULL / static_cast<uint64_t>(result.timeMs) : nodes_;
        result.hashFull = tt_.hashFull();
        result.pv = extractPv(board, result.bestMove, depth);

        // --- Time management: stability-based adjustments ---

        // Track PV and score stability (side-relative)
        bool pvChanged = (depth >= 2 && !sameMove(bestMoveRoot_, prevBestMove_));
        int sideRelScore = (rootBoard.sideToMove() == WHITE) ? result.score : -result.score;
        int sideRelPrev = (rootBoard.sideToMove() == WHITE) ? prevScore_ : -prevScore_;
        int scoreDrop = (depth >= 2) ? (sideRelPrev - sideRelScore) : 0;

        if (depth >= 2) {
            if (pvChanged) {
                stableIterations_ = 0;
            } else {
                stableIterations_++;
            }
        }
        prevBestMove_ = bestMoveRoot_;
        prevScore_ = result.score;

        // Adjust soft time based on stability (adaptive-time searches only)
        if (adaptiveTimeManagement_ && hardTimeMs_ > 0 && depth >= 4) {
            // Derive adjustment from the original soft time to avoid compounding
            int adjustedSoft = originalSoftTimeMs_;
            // Stable position: reduce time after 2+ stable iterations
            if (stableIterations_ >= 2) {
                adjustedSoft = originalSoftTimeMs_ * 3 / 4;
            }
            // Score dropping: allocate more time
            if (scoreDrop > 50) {
                adjustedSoft = originalSoftTimeMs_ * 3 / 2;
            }
            // Respect hard cap
            adjustedSoft = std::min(adjustedSoft, hardTimeMs_);
            if (adjustedSoft != softTimeMs_) {
                softTimeMs_ = adjustedSoft;
            }
        }

        if (infoCallback_) {
            infoCallback_(result);
        }

        if (softTimeExpired() || shouldStop()) {
            break;
        }
    }

    result.nodes = nodes_;
    result.timeMs = elapsedMs();
    result.nps = result.timeMs > 0 ? nodes_ * 1000ULL / static_cast<uint64_t>(result.timeMs) : nodes_;
    result.hashFull = tt_.hashFull();
    if (result.pv.empty() && result.bestMove.from != SQ_NONE)
        result.pv = extractPv(board, result.bestMove, result.depth);
    return result;
}

int Search::alphaBeta(Board& board, int depth, int alpha, int beta, int ply) {
    if (stop_.load()) return 0;
    if (ply >= MAX_PLY - 1) return quiesce(board, alpha, beta, ply);
    const bool restrictedRoot = ply == 0 && !rootMoves_.empty();
    nodes_++;
    if (nodes_ >= nodesLimit_) {
        nodesLimit_ += 1024;
        if (shouldStop()) return 0;
    }

    // 50-move rule and repetition draw
    if (board.halfMoveClock() >= 100 || board.isRepetition())
        return 0;

    // TT probe
    uint64_t hash = board.hash();
    const TTEntry* ttEntry = tt_.probe(hash);
    Move ttMove;
    int ttScore = 0;
    int staticEval = TT_STATIC_EVAL_NONE;
    const int originalAlpha = alpha;

    if (ttEntry) {
        ttMove = tt_.unpackMove(ttEntry->move);
        ttScore = ttEntry->scoreValue();
        if (ttEntry->hasStaticEval())
            staticEval = ttEntry->staticEvalValue();

        // Mate score adjustment
        if (ttScore > MATE - MAX_PLY) ttScore -= ply;
        else if (ttScore < -MATE + MAX_PLY) ttScore += ply;

        if (!restrictedRoot && depth > 0 && ttEntry->depth >= depth) {
            if (ttEntry->bound() == Bound::EXACT) {
                if (ply == 0 && ttMove.from != SQ_NONE) bestMoveRoot_ = ttMove;
                return ttScore;
            }
            if (ttEntry->bound() == Bound::LOWER && ttScore >= beta) {
                if (ply == 0 && ttMove.from != SQ_NONE) bestMoveRoot_ = ttMove;
                return ttScore;
            }
            if (ttEntry->bound() == Bound::UPPER && ttScore <= alpha)
                return ttScore;
        }
    }

    bool pvNode = beta - alpha > 1;
    if (depth >= 5 && pvNode && ttMove.from == SQ_NONE) {
        alphaBeta(board, depth - 2, alpha, beta, ply);
        const TTEntry* newEntry = tt_.probe(hash);
        if (newEntry) {
            ttMove = tt_.unpackMove(newEntry->move);
        }
    }

    if (depth <= 0) return quiesce(board, alpha, beta, ply);
    bool inCheck = board.isInCheck();
    if (inCheck) depth++;

    auto currentStaticEval = [&]() {
        if (staticEval == TT_STATIC_EVAL_NONE) {
            staticEval = eval_.evaluate(board);
            if (board.sideToMove() == BLACK) staticEval = -staticEval;
        }
        return staticEval;
    };

    // Null move pruning (skip when searching mate lines)
    if (depth >= 4 && !inCheck && beta < MATE - MAX_PLY) {
        Bitboard stmAll = board.pieces(board.sideToMove());
        Bitboard stmPawnKing = board.pieces(board.sideToMove(), KING) | board.pieces(board.sideToMove(), PAWN);
        if (stmAll != stmPawnKing) {
            NullUndo nullUndo;
            board.makeNullMove(nullUndo);
            clearContinuationContext(ply + 1);
            int R = 3 + depth / 4;
            int score = -alphaBeta(board, depth - 1 - R, -beta, -beta + 1, ply + 1);
            board.unmakeNullMove(nullUndo);
            if (score >= beta) return beta;
        }
    }

    NodeScratch& st = searchScratch_[ply];
    st.moves.clear();
    std::fill_n(st.used, MAX_MOVES, false);
    gen_.generateLegalMoves(board, st.moves);

    if (ply == 0 && !rootMoves_.empty()) {
        MoveList filtered;
        for (const Move& m : st.moves) {
            if (rootMoveAllowed(m))
                filtered.add(m);
        }
        st.moves = filtered;
    }

    if (st.moves.size() == 0) {
        return inCheck ? -MATE + ply : 0;
    }

    if (ply > 0 && !pvNode && !inCheck &&
        alpha > -MATE + MAX_PLY && beta < MATE - MAX_PLY) {
        int eval = currentStaticEval();
        if (depth <= 3 && hasNonPawnMaterial(board, board.sideToMove())) {
            int margin = 120 * depth;
            if (eval - margin >= beta)
                return eval - margin;
        }

        if (depth <= 2) {
            int margin = 300 + 100 * depth;
            if (eval + margin <= alpha) {
                // Use quiescence from the quiescence scratch pool; searchScratch_[ply] is
                // preserved because quiesce() operates on qsearchScratch_.
                int qScore = quiesce(board, alpha, beta, ply);
                if (qScore <= alpha)
                    return qScore;
            }
        }
    }

    int bestScore = -INF;
    int movesMade = 0;
    Move bestMoveInNode;
    int quietsTriedCount = 0;
    int capturesTriedCount = 0;
    int badNoisyCount = 0;
    int badNoisyIndex = 0;
    int stage = 0;

    auto selectBestIndex = [&](auto predicate, auto scoreFor) {
        int best = -1;
        int bestScore = -INF;
        for (int i = 0; i < st.moves.size(); i++) {
            if (st.used[i] || !predicate(st.moves[i]))
                continue;
            int score = scoreFor(st.moves[i]);
            if (best == -1 || score > bestScore) {
                best = i;
                bestScore = score;
            }
        }
        if (best != -1)
            st.used[best] = true;
        return best;
    };

    while (true) {
        Move m;
        int moveOrderingScore = 0;
        bool haveMove = false;

        while (!haveMove) {
            if (stage == 0) {
                stage++;
                if (ttMove.from == SQ_NONE)
                    continue;
                for (int i = 0; i < st.moves.size(); i++) {
                    if (!st.used[i] && sameMove(st.moves[i], ttMove)) {
                        st.used[i] = true;
                        m = st.moves[i];
                        moveOrderingScore = isQuietHistoryMove(m) ? quietMoveScore(board, m, ply) : 0;
                        haveMove = true;
                        break;
                    }
                }
            } else if (stage == 1) {
                int idx = selectBestIndex(
                    [](Move move) { return isNoisyOrPromotion(move); },
                    [&](Move move) { return noisyMovePreScore(board, move); });
                if (idx == -1) {
                    stage++;
                    continue;
                }

                Move candidate = st.moves[idx];
                if (isCaptureMove(candidate)) {
                    int see = staticExchangeEval(board, candidate);
                    int score = noisyMoveScore(board, candidate, see);
                    if (see < 0) {
                        if (badNoisyCount < MAX_MOVES) {
                            st.badNoisy[badNoisyCount] = candidate;
                            st.badNoisyScores[badNoisyCount] = score;
                            badNoisyCount++;
                        }
                        continue;
                    }
                    moveOrderingScore = score;
                } else {
                    moveOrderingScore = noisyMovePreScore(board, candidate);
                }
                m = candidate;
                haveMove = true;
            } else if (stage == 2) {
                int idx = selectBestIndex(
                    [&](Move move) {
                        return isQuietHistoryMove(move) &&
                               (isKillerMove(move, ply) || isCounterMove(move, ply));
                    },
                    [&](Move move) { return quietMoveScore(board, move, ply); });
                if (idx == -1) {
                    stage++;
                    continue;
                }
                m = st.moves[idx];
                moveOrderingScore = quietMoveScore(board, m, ply);
                haveMove = true;
            } else if (stage == 3) {
                int idx = selectBestIndex(
                    [](Move move) { return isQuietHistoryMove(move); },
                    [&](Move move) { return quietMoveScore(board, move, ply); });
                if (idx == -1) {
                    stage++;
                    continue;
                }
                m = st.moves[idx];
                moveOrderingScore = quietMoveScore(board, m, ply);
                haveMove = true;
            } else if (stage == 4) {
                if (badNoisyIndex >= badNoisyCount) {
                    stage++;
                    continue;
                }
                int best = badNoisyIndex;
                for (int i = badNoisyIndex + 1; i < badNoisyCount; i++) {
                    if (st.badNoisyScores[i] > st.badNoisyScores[best])
                        best = i;
                }
                std::swap(st.badNoisy[badNoisyIndex], st.badNoisy[best]);
                std::swap(st.badNoisyScores[badNoisyIndex], st.badNoisyScores[best]);
                m = st.badNoisy[badNoisyIndex];
                moveOrderingScore = st.badNoisyScores[badNoisyIndex];
                badNoisyIndex++;
                haveMove = true;
            } else {
                break;
            }
        }

        if (!haveMove)
            break;

        // Futility pruning
        if (depth <= 2 && !inCheck && movesMade >= 1 &&
            !isCaptureMove(m) && m.type != PROMOTION) {
            int margin = (depth == 1) ? 200 : 600;
            if (currentStaticEval() + margin < alpha) continue;
        }

        Piece movedPiece = board.pieceOn(m.from);
        PieceType capturedType = capturedTypeForMove(board, m);
        UndoInfo undo;
        if (!board.makeMove(m, undo)) continue;
        bool givesCheck = board.isInCheck();

        if (isQuietHistoryMove(m)) {
            setContinuationContext(ply + 1, movedPiece, m);
            if (quietsTriedCount < MAX_MOVES) {
                st.quietsTried[quietsTriedCount] = m;
                st.quietPiecesTried[quietsTriedCount] = movedPiece;
                quietsTriedCount++;
            }
        } else {
            clearContinuationContext(ply + 1);
            if (isCaptureMove(m) && capturedType != PIECE_TYPE_NB && capturesTriedCount < MAX_MOVES) {
                st.capturesTried[capturesTriedCount] = m;
                st.capturePiecesTried[capturesTriedCount] = movedPiece;
                st.capturedTypesTried[capturesTriedCount] = capturedType;
                capturesTriedCount++;
            }
        }

        int score;
        if (movesMade == 0) {
            score = -alphaBeta(board, depth - 1, -beta, -alpha, ply + 1);
        } else {
            // Late-move reductions (LMR): reduce moves beyond the first few
            int reduction = 0;
            if (depth >= 3 && isQuietHistoryMove(m) && !inCheck && !givesCheck &&
                movesMade >= (pvNode ? 4 : 3)) {
                reduction = 1;
                if (depth >= 6) reduction++;
                if (movesMade >= 8) reduction++;
                if (!pvNode) reduction++;
                if (moveOrderingScore >= KILLER_SCORE || moveOrderingScore > HISTORY_MAX / 2)
                    reduction--;
                else if (moveOrderingScore < -HISTORY_MAX / 4)
                    reduction++;
                reduction = std::clamp(reduction, 0, depth - 2);
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
                    if (isQuietHistoryMove(m)) {
                        updateKiller(m, ply);
                        updateCounterMove(m, ply);
                        updateQuietHistory(m, movedPiece, depth, ply, 1);
                        for (int i = 0; i < quietsTriedCount; i++) {
                            if (!sameMove(st.quietsTried[i], m))
                                updateQuietHistory(st.quietsTried[i], st.quietPiecesTried[i], depth, ply, -1);
                        }
                    } else if (isCaptureMove(m) && capturedType != PIECE_TYPE_NB) {
                        updateCaptureHistory(m, movedPiece, capturedType, depth, 1);
                        for (int i = 0; i < capturesTriedCount; i++) {
                            if (!sameMove(st.capturesTried[i], m)) {
                                updateCaptureHistory(st.capturesTried[i], st.capturePiecesTried[i],
                                                     st.capturedTypesTried[i], depth, -1);
                            }
                        }
                    }
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

        tt_.store(hash, ttStoreScore, static_cast<int8_t>(depth), b, bestMoveInNode,
                  staticEval);
    }

    return bestScore;
}

int Search::quiesce(Board& board, int alpha, int beta, int ply) {
    if (stop_.load()) return 0;
    if (ply >= MAX_PLY - 1) {
        if (board.isInCheck()) return alpha;
        int standPat = eval_.evaluate(board);
        if (board.sideToMove() == BLACK) standPat = -standPat;
        if (standPat >= beta) return beta;
        if (standPat > alpha) alpha = standPat;
        return alpha;
    }

    nodes_++;
    if (nodes_ >= nodesLimit_) {
        nodesLimit_ += 1024;
        if (shouldStop()) return 0;
    }

    // 50-move rule and repetition draw
    if (board.halfMoveClock() >= 100 || board.isRepetition())
        return 0;

    bool inCheck = board.isInCheck();
    int standPat = 0;

    if (!inCheck) {
        standPat = eval_.evaluate(board);
        if (board.sideToMove() == BLACK) standPat = -standPat;
        if (standPat >= beta) return beta;
        if (standPat > alpha) alpha = standPat;

        // Delta pruning: if even capturing the queen can't reach alpha, prune
        int delta = Eval::QUEEN_VALUE + 100;
        if (standPat + delta < alpha) return alpha;
    }

    NodeScratch& st = qsearchScratch_[ply];
    st.moves.clear();
    std::fill_n(st.used, MAX_MOVES, false);
    if (inCheck) {
        gen_.generateLegalMoves(board, st.moves);
    } else {
        gen_.generateLegalNoisyMoves(board, st.moves);
    }

    if (st.moves.size() == 0) {
        if (inCheck) return -MATE + ply;
        return gen_.hasLegalMove(board) ? alpha : 0;
    }

    while (true) {
        int best = -1;
        int bestScore = -INF;
        for (int i = 0; i < st.moves.size(); i++) {
            if (st.used[i])
                continue;
            int score = isNoisyOrPromotion(st.moves[i])
                ? noisyMovePreScore(board, st.moves[i])
                : quietMoveScore(board, st.moves[i], ply);
            if (best == -1 || score > bestScore) {
                best = i;
                bestScore = score;
            }
        }
        if (best == -1)
            break;

        st.used[best] = true;
        Move m = st.moves[best];

        // Static exchange evaluation (SEE): skip captures that lose material by force.
        if (isCaptureMove(m) && !inCheck && staticExchangeEval(board, m) < 0)
            continue;

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

int Search::noisyMovePreScore(const Board& board, Move m) const {
    int score = 0;
    if (isCaptureMove(m)) {
        Piece movedPiece = board.pieceOn(m.from);
        PieceType capturedType = capturedTypeForMove(board, m);
        int victimValue = capturedType != PIECE_TYPE_NB ? Eval::pieceValue(capturedType) : 0;
        int attackerValue = movedPiece != NO_PIECE ? Eval::pieceValue(typeOf(movedPiece)) : 0;
        int historyScore = 0;
        if (movedPiece != NO_PIECE && capturedType != PIECE_TYPE_NB)
            historyScore = captureHistory_[movedPiece][m.to][capturedType];
        score += 100000 + victimValue * 16 - attackerValue + historyScore;
    }
    if ((m.type == PROMOTION || m.type == PROMOTION_CAPTURE) && m.promotion != PIECE_TYPE_NB) {
        score += 90000 + Eval::pieceValue(m.promotion);
    }

    return score;
}

int Search::noisyMoveScore(const Board& board, Move m, int see) const {
    int score = (see >= 0 ? 200000 : 0) + see * 16;
    if ((m.type == PROMOTION || m.type == PROMOTION_CAPTURE) && m.promotion != PIECE_TYPE_NB)
        score += 90000 + Eval::pieceValue(m.promotion);

    Piece movedPiece = board.pieceOn(m.from);
    PieceType capturedType = capturedTypeForMove(board, m);
    if (movedPiece != NO_PIECE && capturedType != PIECE_TYPE_NB)
        score += captureHistory_[movedPiece][m.to][capturedType];
    return score;
}

int Search::quietMoveScore(const Board& board, Move m, int ply) const {
    int score = 0;
    Piece movedPiece = board.pieceOn(m.from);
    if (movedPiece != NO_PIECE) {
        score += quietHistory_[colorOf(movedPiece)][m.from][m.to];
        if (ply > 0 && ply < MAX_PLY) {
            Piece previousPiece = continuationPieceByPly_[ply];
            Square previousTo = continuationToByPly_[ply];
            if (previousPiece != NO_PIECE && previousTo != SQ_NONE)
                score += continuationHistory_[previousPiece][previousTo][movedPiece][m.to] / 2;
        }
    }

    if (isKillerMove(m, ply)) {
        int packed = m.from | (m.to << 6);
        score += (ply < MAX_PLY && packed == killer1_[ply]) ? KILLER_SCORE : KILLER_SCORE - 1;
    } else if (isCounterMove(m, ply)) {
        score += COUNTER_MOVE_SCORE;
    }

    return score;
}

bool Search::isKillerMove(Move m, int ply) const {
    if (ply < 0 || ply >= MAX_PLY) return false;
    int packed = m.from | (m.to << 6);
    return packed == killer1_[ply] || packed == killer2_[ply];
}

bool Search::isCounterMove(Move m, int ply) const {
    if (ply <= 0 || ply >= MAX_PLY) return false;
    Piece previousPiece = continuationPieceByPly_[ply];
    Square previousTo = continuationToByPly_[ply];
    if (previousPiece == NO_PIECE || previousTo == SQ_NONE)
        return false;
    return sameMove(counterMove_[previousPiece][previousTo], m);
}

bool Search::rootMoveAllowed(Move m) const {
    for (const Move& allowed : rootMoves_) {
        if (allowed.from == m.from && allowed.to == m.to &&
            (allowed.promotion == PIECE_TYPE_NB || allowed.promotion == m.promotion)) {
            return true;
        }
    }
    return false;
}

std::vector<Move> Search::extractPv(Board board, Move bestMove, int maxDepth) const {
    std::vector<Move> pv;
    Move next = bestMove;
    uint64_t seenHashes[MAX_PLY]{};
    int seenCount = 0;
    seenHashes[seenCount++] = board.hash();

    for (int i = 0; i < maxDepth && next.from != SQ_NONE && next.to != SQ_NONE; i++) {
        UndoInfo undo;
        if (!board.makeMove(next, undo))
            break;

        pv.push_back(undo.move);

        if (board.halfMoveClock() >= 100 || board.isRepetition())
            break;

        uint64_t hash = board.hash();
        bool alreadySeen = false;
        for (int j = 0; j < seenCount; j++) {
            if (seenHashes[j] == hash) {
                alreadySeen = true;
                break;
            }
        }
        if (alreadySeen)
            break;
        if (seenCount < MAX_PLY)
            seenHashes[seenCount++] = hash;

        const TTEntry* entry = tt_.probe(hash);
        if (!entry)
            break;
        next = tt_.unpackMove(entry->move);
    }

    return pv;
}

int Search::elapsedMs() const {
    return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime_).count());
}

bool Search::softTimeExpired() const {
    return !infinite_ && softTimeMs_ > 0 && elapsedMs() >= softTimeMs_;
}

void Search::ageHistory() {
    for (int c = 0; c < COLOR_NB; c++)
        for (int from = 0; from < 64; from++)
            for (int to = 0; to < 64; to++)
                quietHistory_[c][from][to] /= 2;

    for (int previousPiece = 0; previousPiece < PIECE_NB; previousPiece++)
        for (int previousTo = 0; previousTo < 64; previousTo++)
            for (int movedPiece = 0; movedPiece < PIECE_NB; movedPiece++)
                for (int to = 0; to < 64; to++)
                    continuationHistory_[previousPiece][previousTo][movedPiece][to] /= 2;

    for (int movedPiece = 0; movedPiece < PIECE_NB; movedPiece++)
        for (int to = 0; to < 64; to++)
            for (int capturedType = 0; capturedType < PIECE_TYPE_NB; capturedType++)
                captureHistory_[movedPiece][to][capturedType] /= 2;
}

void Search::updateKiller(Move m, int ply) {
    if (ply < 0 || ply >= MAX_PLY) return;
    int packed = m.from | (m.to << 6);
    if (packed != killer1_[ply]) {
        killer2_[ply] = killer1_[ply];
        killer1_[ply] = packed;
    }
}

void Search::updateCounterMove(Move m, int ply) {
    if (ply <= 0 || ply >= MAX_PLY) return;
    Piece previousPiece = continuationPieceByPly_[ply];
    Square previousTo = continuationToByPly_[ply];
    if (previousPiece == NO_PIECE || previousTo == SQ_NONE)
        return;
    counterMove_[previousPiece][previousTo] = m;
}

void Search::updateQuietHistory(Move m, Piece movedPiece, int depth, int ply, int sign) {
    if (movedPiece == NO_PIECE) return;

    int bonus = std::clamp(depth * depth, 1, HISTORY_MAX);
    if (sign < 0) bonus = -bonus;
    updateHistoryValue(quietHistory_[colorOf(movedPiece)][m.from][m.to], bonus);

    if (ply > 0 && ply < MAX_PLY) {
        Piece previousPiece = continuationPieceByPly_[ply];
        Square previousTo = continuationToByPly_[ply];
        if (previousPiece != NO_PIECE && previousTo != SQ_NONE) {
            updateHistoryValue(continuationHistory_[previousPiece][previousTo][movedPiece][m.to],
                               bonus);
        }
    }
}

void Search::updateCaptureHistory(Move m, Piece movedPiece, PieceType capturedType, int depth, int sign) {
    if (movedPiece == NO_PIECE || capturedType == PIECE_TYPE_NB) return;

    int bonus = std::clamp(depth * depth, 1, HISTORY_MAX);
    if (sign < 0) bonus = -bonus;
    updateHistoryValue(captureHistory_[movedPiece][m.to][capturedType], bonus);
}

void Search::updateHistoryValue(int& value, int bonus) {
    bonus = std::clamp(bonus, -HISTORY_MAX, HISTORY_MAX);
    value += bonus - value * std::abs(bonus) / HISTORY_MAX;
    value = std::clamp(value, -HISTORY_MAX, HISTORY_MAX);
}

PieceType Search::capturedTypeForMove(const Board& board, Move m) const {
    if (m.type == EN_PASSANT)
        return PAWN;
    if (!isCaptureMove(m))
        return PIECE_TYPE_NB;

    Piece captured = board.pieceOn(m.to);
    if (captured == NO_PIECE)
        return PIECE_TYPE_NB;
    return typeOf(captured);
}

void Search::setContinuationContext(int ply, Piece movedPiece, Move m) {
    if (ply < 0 || ply >= MAX_PLY) return;
    continuationPieceByPly_[ply] = movedPiece;
    continuationToByPly_[ply] = m.to;
}

void Search::clearContinuationContext(int ply) {
    if (ply < 0 || ply >= MAX_PLY) return;
    continuationPieceByPly_[ply] = NO_PIECE;
    continuationToByPly_[ply] = SQ_NONE;
}

bool Search::shouldStop() {
    if (stop_.load()) return true;
    if (maxNodes_ > 0 && nodes_ >= maxNodes_) {
        stop_.store(true);
        return true;
    }
    if (!infinite_ && hardTimeMs_ > 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime_).count();
        if (elapsed >= hardTimeMs_) {
            stop_.store(true);
            return true;
        }
    }
    return false;
}

} // namespace chess
