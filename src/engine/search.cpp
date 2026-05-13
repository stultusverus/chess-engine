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
    ageHistory();
}

void Search::setTimeMs(int ms) {
    setTimeControlMs(ms, ms);
}

void Search::setTimeControlMs(int softMs, int hardMs) {
    softTimeMs_ = std::max(0, softMs);
    hardTimeMs_ = std::max(softTimeMs_, hardMs);
    infinite_ = false;
    stop_.store(false);
    maxNodes_ = 0;
}

void Search::setInfinite(bool inf) {
    infinite_ = inf;
    softTimeMs_ = 0;
    hardTimeMs_ = 0;
    stop_.store(false);
    maxNodes_ = 0;
}

void Search::setNodeLimit(uint64_t nodes) {
    maxNodes_ = nodes;
    softTimeMs_ = 0;
    hardTimeMs_ = 0;
    infinite_ = false;
    stop_.store(false);
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
    std::fill(std::begin(ttMoveByPly_), std::end(ttMoveByPly_), Move());
    std::fill(std::begin(continuationPieceByPly_), std::end(continuationPieceByPly_), NO_PIECE);
    std::fill(std::begin(continuationToByPly_), std::end(continuationToByPly_), SQ_NONE);
    ageHistory();

    startTime_ = std::chrono::steady_clock::now();
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

        if (stop_.load()) break;

        result.depth = depth;
        result.bestMove = bestMoveRoot_;
        result.nodes = nodes_;
        result.timeMs = elapsedMs();
        result.nps = result.timeMs > 0 ? nodes_ * 1000ULL / static_cast<uint64_t>(result.timeMs) : nodes_;
        result.hashFull = tt_.hashFull();
        result.pv = extractPv(board, result.bestMove, depth);

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
    const int originalAlpha = alpha;
    if (ply >= 0 && ply < MAX_PLY) ttMoveByPly_[ply] = Move();

    if (ttEntry) {
        ttMove = tt_.unpackMove(ttEntry->move);
        if (ply >= 0 && ply < MAX_PLY) ttMoveByPly_[ply] = ttMove;
        ttScore = ttEntry->score;

        // Mate score adjustment
        if (ttScore > MATE - MAX_PLY) ttScore -= ply;
        else if (ttScore < -MATE + MAX_PLY) ttScore += ply;

        if (!restrictedRoot && depth > 0 && ttEntry->depth >= depth) {
            if (ttEntry->bound == static_cast<uint8_t>(Bound::EXACT)) {
                if (ply == 0 && ttMove.from != SQ_NONE) bestMoveRoot_ = ttMove;
                return ttScore;
            }
            if (ttEntry->bound == static_cast<uint8_t>(Bound::LOWER) && ttScore >= beta) {
                if (ply == 0 && ttMove.from != SQ_NONE) bestMoveRoot_ = ttMove;
                return ttScore;
            }
            if (ttEntry->bound == static_cast<uint8_t>(Bound::UPPER) && ttScore <= alpha)
                return ttScore;
        }
    }

    if (depth >= 3 && ttMove.from == SQ_NONE) {
        alphaBeta(board, depth - 2, alpha, beta, ply);
        const TTEntry* newEntry = tt_.probe(hash);
        if (newEntry) {
            ttMove = tt_.unpackMove(newEntry->move);
            if (ply >= 0 && ply < MAX_PLY) ttMoveByPly_[ply] = ttMove;
        }
    }

    if (depth <= 0) return quiesce(board, alpha, beta, ply);
    bool inCheck = board.isInCheck();
    bool pvNode = beta - alpha > 1;
    if (inCheck) depth++;

    int staticEval = -INF;
    auto currentStaticEval = [&]() {
        if (staticEval == -INF) {
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

    MoveList moves;
    gen_.generateLegalMoves(board, moves);

    if (ply == 0 && !rootMoves_.empty()) {
        MoveList filtered;
        for (const Move& m : moves) {
            if (rootMoveAllowed(m))
                filtered.add(m);
        }
        moves = filtered;
    }

    if (moves.size() == 0) {
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
                int qScore = quiesce(board, alpha, beta, ply);
                if (qScore <= alpha)
                    return qScore;
            }
        }
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
    Move quietsTried[MAX_MOVES]{};
    Piece quietPiecesTried[MAX_MOVES]{};
    int quietsTriedCount = 0;
    Move capturesTried[MAX_MOVES]{};
    Piece capturePiecesTried[MAX_MOVES]{};
    PieceType capturedTypesTried[MAX_MOVES]{};
    int capturesTriedCount = 0;

    for (const Move& m : moves) {
        // Futility pruning
        if (depth <= 2 && !inCheck && movesMade >= 1 &&
            !isCaptureMove(m) && m.type != PROMOTION) {
            int margin = (depth == 1) ? 200 : 600;
            if (currentStaticEval() + margin < alpha) continue;
        }

        Piece movedPiece = board.pieceOn(m.from);
        PieceType capturedType = capturedTypeForMove(board, m);
        int moveOrderingScore = isQuietHistoryMove(m) ? scoreMove(board, m, ply) : 0;
        UndoInfo undo;
        if (!board.makeMove(m, undo)) continue;
        bool givesCheck = board.isInCheck();

        if (isQuietHistoryMove(m)) {
            setContinuationContext(ply + 1, movedPiece, m);
            if (quietsTriedCount < MAX_MOVES) {
                quietsTried[quietsTriedCount] = m;
                quietPiecesTried[quietsTriedCount] = movedPiece;
                quietsTriedCount++;
            }
        } else {
            clearContinuationContext(ply + 1);
            if (isCaptureMove(m) && capturedType != PIECE_TYPE_NB && capturesTriedCount < MAX_MOVES) {
                capturesTried[capturesTriedCount] = m;
                capturePiecesTried[capturesTriedCount] = movedPiece;
                capturedTypesTried[capturesTriedCount] = capturedType;
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
                        updateQuietHistory(m, movedPiece, depth, ply, 1);
                        for (int i = 0; i < quietsTriedCount; i++) {
                            if (!sameMove(quietsTried[i], m))
                                updateQuietHistory(quietsTried[i], quietPiecesTried[i], depth, ply, -1);
                        }
                    } else if (isCaptureMove(m) && capturedType != PIECE_TYPE_NB) {
                        updateCaptureHistory(m, movedPiece, capturedType, depth, 1);
                        for (int i = 0; i < capturesTriedCount; i++) {
                            if (!sameMove(capturesTried[i], m)) {
                                updateCaptureHistory(capturesTried[i], capturePiecesTried[i],
                                                     capturedTypesTried[i], depth, -1);
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

        tt_.store(hash, ttStoreScore, static_cast<int8_t>(depth), b, bestMoveInNode);
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

    MoveList searchMoves;
    if (inCheck) {
        gen_.generateLegalMoves(board, searchMoves);
    } else {
        gen_.generateLegalNoisyMoves(board, searchMoves);
    }

    if (searchMoves.size() == 0) {
        if (inCheck) return -MATE + ply;
        return gen_.hasLegalMove(board) ? alpha : 0;
    }

    sortMoves(searchMoves, board, ply);

    for (const Move& m : searchMoves) {
        // Static exchange evaluation (SEE): skip captures that lose material by force.
        if ((m.type == CAPTURE || m.type == PROMOTION_CAPTURE || m.type == EN_PASSANT) && !inCheck) {
            if (staticExchangeEval(board, m) < 0)
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

    if (ply >= 0 && ply < MAX_PLY) {
        const Move& ttMove = ttMoveByPly_[ply];
        if (ttMove.from == m.from && ttMove.to == m.to &&
            (ttMove.promotion == PIECE_TYPE_NB || ttMove.promotion == m.promotion)) {
            return 10000000;
        }
    }

    if (isCaptureMove(m)) {
        int see = staticExchangeEval(board, m);
        Piece movedPiece = board.pieceOn(m.from);
        PieceType capturedType = capturedTypeForMove(board, m);
        int historyScore = 0;
        if (movedPiece != NO_PIECE && capturedType != PIECE_TYPE_NB)
            historyScore = captureHistory_[movedPiece][m.to][capturedType];
        score = (see >= 0 ? 100000 : 0) + see * 16 + historyScore;
        if (m.type == PROMOTION_CAPTURE && m.promotion != PIECE_TYPE_NB)
            score += Eval::pieceValue(m.promotion);
    } else if (m.type == PROMOTION) {
        score = 90000 + Eval::pieceValue(m.promotion);
    } else {
        int packed = m.from | (m.to << 6);
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
        if (ply < MAX_PLY && packed == killer1_[ply])
            score += KILLER_SCORE;
        else if (ply < MAX_PLY && packed == killer2_[ply])
            score += KILLER_SCORE - 1;
    }

    return score;
}

void Search::sortMoves(MoveList& moves, const Board& board, int ply) {
    int scores[MAX_MOVES]{};
    for (int i = 0; i < moves.size(); i++)
        scores[i] = scoreMove(board, moves[i], ply);

    for (int i = 1; i < moves.size(); i++) {
        Move move = moves[i];
        int score = scores[i];
        int j = i - 1;
        while (j >= 0 && scores[j] < score) {
            moves[j + 1] = moves[j];
            scores[j + 1] = scores[j];
            j--;
        }
        moves[j + 1] = move;
        scores[j + 1] = score;
    }
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
