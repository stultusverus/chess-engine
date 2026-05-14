#include "board.h"
#include "attacks.h"
#include "eval.h"
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <string>
#include <cctype>

namespace chess {

// --- Zobrist keys ---
namespace {
    std::optional<int> parseIntStrict(const std::string& s) {
        if (s.empty()) return std::nullopt;
        char* end = nullptr;
        errno = 0;
        long v = std::strtol(s.c_str(), &end, 10);
        if (errno != 0 || end == s.c_str() || *end != '\0') return std::nullopt;
        if (v < INT_MIN || v > INT_MAX) return std::nullopt;
        return static_cast<int>(v);
    }

    bool kingsAdjacent(Square a, Square b) {
        int df = std::abs(int(fileOf(a)) - int(fileOf(b)));
        int dr = std::abs(int(rankOf(a)) - int(rankOf(b)));
        return df <= 1 && dr <= 1;
    }

    bool hasEnPassantCapture(const std::array<Piece, 64>& mailbox, Color us, Square ep) {
        if (ep == SQ_NONE) return false;

        if (fileOf(ep) > FILE_A) {
            Square attacker = Square(us == WHITE ? ep - 9 : ep + 7);
            if (attacker >= A1 && attacker <= H8 && mailbox[attacker] == makePiece(us, PAWN))
                return true;
        }
        if (fileOf(ep) < FILE_H) {
            Square attacker = Square(us == WHITE ? ep - 7 : ep + 9);
            if (attacker >= A1 && attacker <= H8 && mailbox[attacker] == makePiece(us, PAWN))
                return true;
        }
        return false;
    }

    uint64_t zobristPieces_[PIECE_NB][64];
    uint64_t zobristPawns_[COLOR_NB][64];
    uint64_t zobristCastle_[16];
    uint64_t zobristEp_[8];
    uint64_t zobristSide_;
    bool zobristInitialized_ = false;

    uint64_t prng(uint64_t& state) {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 0x2545F4914F6CDD1DULL;
    }

    // Castling rights on each square (cleared when square is vacated)
    constexpr int castleRightsMask[64] = {
        WQ, 0, 0, 0, WK|WQ, 0, 0, WK,
        0,  0, 0, 0, 0,      0, 0, 0,
        0,  0, 0, 0, 0,      0, 0, 0,
        0,  0, 0, 0, 0,      0, 0, 0,
        0,  0, 0, 0, 0,      0, 0, 0,
        0,  0, 0, 0, 0,      0, 0, 0,
        0,  0, 0, 0, 0,      0, 0, 0,
        BQ, 0, 0, 0, BK|BQ,  0, 0, BK,
    };
}

void Board::initZobrist() {
    uint64_t state = 0x123456789ABCDEF0ULL;
    for (int p = 0; p < PIECE_NB; p++)
        for (int s = 0; s < 64; s++)
            zobristPieces_[p][s] = prng(state);
    for (int c = 0; c < COLOR_NB; c++)
        for (int s = 0; s < 64; s++)
            zobristPawns_[c][s] = prng(state);
    zobristSide_ = prng(state);
    for (int i = 0; i < 16; i++)
        zobristCastle_[i] = prng(state);
    for (int f = 0; f < 8; f++)
        zobristEp_[f] = prng(state);
    zobristInitialized_ = true;
}

// --- Board ---

Board::Board() {
    if (!zobristInitialized_) initZobrist();
    setFen(STARTPOS_FEN);
}

Board::Board(const std::string& fen) {
    if (!zobristInitialized_) initZobrist();
    if (!setFen(fen))
        setFen(STARTPOS_FEN);
}

bool Board::setFen(const std::string& fen) {
    auto oldByPiece = byPiece_;
    auto oldByColor = byColor_;
    auto oldMailbox = mailbox_;
    Color oldStm = stm_;
    Square oldEp = ep_;
    int oldCastle = castle_;
    int oldHalfMoves = halfMoves_;
    int oldFullMoves = fullMoves_;
    uint64_t oldHash = hash_;
    IncrementalEvalState oldEvalState = evalState_;
    auto oldHistory = posHistory_;

    auto restoreOldState = [&]() {
        byPiece_ = oldByPiece;
        byColor_ = oldByColor;
        mailbox_ = oldMailbox;
        stm_ = oldStm;
        ep_ = oldEp;
        castle_ = oldCastle;
        halfMoves_ = oldHalfMoves;
        fullMoves_ = oldFullMoves;
        hash_ = oldHash;
        evalState_ = oldEvalState;
        posHistory_ = oldHistory;
    };

    std::fill(byPiece_.begin(), byPiece_.end(), Bitboard(0));
    std::fill(byColor_.begin(), byColor_.end(), Bitboard(0));
    std::fill(mailbox_.begin(), mailbox_.end(), NO_PIECE);
    stm_ = WHITE;
    ep_ = SQ_NONE;
    castle_ = 0;
    halfMoves_ = 0;
    fullMoves_ = 1;
    hash_ = 0;
    clearEvalState();

    std::istringstream ss(fen);
    std::string placement, stm, castle, ep, half, full;

    ss >> placement >> stm >> castle >> ep >> half >> full;
    if (placement.empty() || stm.empty() || castle.empty() || ep.empty() || half.empty() || full.empty()) {
        restoreOldState();
        return false;
    }

    // Piece placement
    Square sq = A8;
    int file = 0;
    int rank = 7;
    for (char c : placement) {
        if (c == '/') {
            if (file != 8 || rank == 0) {
                restoreOldState();
                return false;
            }
            rank--;
            file = 0;
            sq = makeSquare(FILE_A, Rank(rank));
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            int empty = c - '0';
            if (empty < 1 || empty > 8 || file + empty > 8) {
                restoreOldState();
                return false;
            }
            file += empty;
            sq = Square(sq + empty);
        } else {
            if (sq >= 64 || file >= 8) {
                restoreOldState();
                return false;
            }
            Piece p = NO_PIECE;
            switch (c) {
            case 'P': p = W_PAWN; break; case 'N': p = W_KNIGHT; break;
            case 'B': p = W_BISHOP; break; case 'R': p = W_ROOK; break;
            case 'Q': p = W_QUEEN; break; case 'K': p = W_KING; break;
            case 'p': p = B_PAWN; break; case 'n': p = B_KNIGHT; break;
            case 'b': p = B_BISHOP; break; case 'r': p = B_ROOK; break;
            case 'q': p = B_QUEEN; break; case 'k': p = B_KING; break;
            }
            if (p == NO_PIECE) {
                restoreOldState();
                return false;
            }
            byPiece_[p] |= squareBb(sq);
            byColor_[colorOf(p)] |= squareBb(sq);
            mailbox_[sq] = p;
            sq = Square(sq + 1);
            file++;
        }
    }
    if (rank != 0 || file != 8) {
        restoreOldState();
        return false;
    }

    // Side to move
    if (stm != "w" && stm != "b") {
        restoreOldState();
        return false;
    }
    stm_ = (stm == "b") ? BLACK : WHITE;

    // Castling rights
    if (castle == "-") {
        castle_ = 0;
    } else {
        bool seenK = false, seenQ = false, seenk = false, seenq = false;
        for (char c : castle) {
            switch (c) {
            case 'K':
                if (seenK) { restoreOldState(); return false; }
                seenK = true;
                castle_ |= WK;
                break;
            case 'Q':
                if (seenQ) { restoreOldState(); return false; }
                seenQ = true;
                castle_ |= WQ;
                break;
            case 'k':
                if (seenk) { restoreOldState(); return false; }
                seenk = true;
                castle_ |= BK;
                break;
            case 'q':
                if (seenq) { restoreOldState(); return false; }
                seenq = true;
                castle_ |= BQ;
                break;
            default:
                restoreOldState();
                return false;
            }
        }
    }

    // En passant
    if (ep != "-") {
        if (ep.size() != 2 || ep[0] < 'a' || ep[0] > 'h' || ep[1] < '1' || ep[1] > '8') {
            restoreOldState();
            return false;
        }
        File f = File(ep[0] - 'a');
        Rank r = Rank(ep[1] - '1');
        if ((stm_ == WHITE && r != RANK_6) || (stm_ == BLACK && r != RANK_3)) {
            restoreOldState();
            return false;
        }
        ep_ = makeSquare(f, r);
    }

    // Clocks
    auto parsedHalf = parseIntStrict(half);
    auto parsedFull = parseIntStrict(full);
    if (!parsedHalf || !parsedFull) {
        restoreOldState();
        return false;
    }
    halfMoves_ = *parsedHalf;
    fullMoves_ = *parsedFull;
    if (halfMoves_ < 0 || fullMoves_ < 1) {
        restoreOldState();
        return false;
    }

    // Build hash and incremental eval state
    hash_ = 0;
    clearEvalState();
    for (int s = 0; s < 64; s++) {
        Piece p = mailbox_[s];
        if (p != NO_PIECE) {
            hash_ ^= zobristPieces_[p][s];
            addPieceToEval(p, Square(s));
        }
    }
    if (stm_ == BLACK) hash_ ^= zobristSide_;
    hash_ ^= zobristCastle_[castle_];
    if (ep_ != SQ_NONE && hasEnPassantCapture(mailbox_, stm_, ep_))
        hash_ ^= zobristEp_[fileOf(ep_)];

    // Validate: must have exactly one king per side
    if (popcount(pieces(WHITE, KING)) != 1 || popcount(pieces(BLACK, KING)) != 1) {
        restoreOldState();
        return false;
    }

    Square whiteKing = kingSquare(WHITE);
    Square blackKing = kingSquare(BLACK);
    if (kingsAdjacent(whiteKing, blackKing)) {
        restoreOldState();
        return false;
    }

    if ((castle_ & WK) && (mailbox_[E1] != W_KING || mailbox_[H1] != W_ROOK)) {
        restoreOldState();
        return false;
    }
    if ((castle_ & WQ) && (mailbox_[E1] != W_KING || mailbox_[A1] != W_ROOK)) {
        restoreOldState();
        return false;
    }
    if ((castle_ & BK) && (mailbox_[E8] != B_KING || mailbox_[H8] != B_ROOK)) {
        restoreOldState();
        return false;
    }
    if ((castle_ & BQ) && (mailbox_[E8] != B_KING || mailbox_[A8] != B_ROOK)) {
        restoreOldState();
        return false;
    }

    if (ep_ != SQ_NONE) {
        Color us = stm_;
        if (mailbox_[ep_] != NO_PIECE) {
            restoreOldState();
            return false;
        }

        Square pawnSquare = Square(us == WHITE ? ep_ - 8 : ep_ + 8);
        if (pawnSquare < A1 || pawnSquare > H8 || mailbox_[pawnSquare] != makePiece(~us, PAWN)) {
            restoreOldState();
            return false;
        }
    }

    // Validate: no pawns on first or last rank
    if ((pieces(PAWN) & (0xFFULL | 0xFF00000000000000ULL)) != 0) {
        restoreOldState();
        return false;
    }

    // Validate: side to move must not attack the opponent king
    if (attacks::isSquareAttacked(*this, kingSquare(~stm_), stm_)) {
        restoreOldState();
        return false;
    }

    posHistory_.clear();
    return true;
}

std::string Board::fen() const {
    std::ostringstream ss;
    for (Rank r = RANK_8; r >= RANK_1; r = Rank(r - 1)) {
        int empty = 0;
        for (File f = FILE_A; f <= FILE_H; f = File(f + 1)) {
            Square s = makeSquare(f, r);
            Piece p = mailbox_[s];
            if (p == NO_PIECE) {
                empty++;
            } else {
                if (empty) { ss << empty; empty = 0; }
                const char* pc = "PNBRQKpnbrqk";
                ss << pc[p];
            }
        }
        if (empty) ss << empty;
        if (r > RANK_1) ss << '/';
    }

    ss << ' ' << (stm_ == WHITE ? 'w' : 'b');
    ss << ' ';
    if (castle_ == 0) ss << '-';
    else {
        if (castle_ & WK) ss << 'K';
        if (castle_ & WQ) ss << 'Q';
        if (castle_ & BK) ss << 'k';
        if (castle_ & BQ) ss << 'q';
    }
    ss << ' ' << (ep_ == SQ_NONE ? "-" : squareToString(ep_));
    ss << ' ' << halfMoves_ << ' ' << fullMoves_;
    return ss.str();
}

void Board::clearEvalState() {
    evalState_ = IncrementalEvalState{};
}

void Board::addPieceToEval(Piece p, Square s) {
    if (p == NO_PIECE) return;
    PieceType pt = typeOf(p);
    Color c = colorOf(p);
    if (pt != KING)
        evalState_.material += c == WHITE ? Eval::pieceValue(pt) : -Eval::pieceValue(pt);
    evalState_.pstMg += Eval::pieceSquareMg(p, s);
    evalState_.pstEg += Eval::pieceSquareEg(p, s);
    evalState_.phase += Eval::phaseValue(pt);
    if (pt == PAWN)
        evalState_.pawnHash ^= zobristPawns_[c][s];
}

void Board::removePieceFromEval(Piece p, Square s) {
    if (p == NO_PIECE) return;
    PieceType pt = typeOf(p);
    Color c = colorOf(p);
    if (pt != KING)
        evalState_.material -= c == WHITE ? Eval::pieceValue(pt) : -Eval::pieceValue(pt);
    evalState_.pstMg -= Eval::pieceSquareMg(p, s);
    evalState_.pstEg -= Eval::pieceSquareEg(p, s);
    evalState_.phase -= Eval::phaseValue(pt);
    if (pt == PAWN)
        evalState_.pawnHash ^= zobristPawns_[c][s];
}

void Board::putPiece(Piece p, Square s) {
    if (p == NO_PIECE) return;
    byPiece_[p] |= squareBb(s);
    byColor_[colorOf(p)] |= squareBb(s);
    mailbox_[s] = p;
    hash_ ^= zobristPieces_[p][s];
    addPieceToEval(p, s);
}

void Board::removePiece(Piece p, Square s) {
    if (p == NO_PIECE) return;
    byPiece_[p] &= ~squareBb(s);
    byColor_[colorOf(p)] &= ~squareBb(s);
    mailbox_[s] = NO_PIECE;
    hash_ ^= zobristPieces_[p][s];
    removePieceFromEval(p, s);
}

void Board::movePiece(Piece p, Square from, Square to) {
    removePiece(p, from);
    putPiece(p, to);
}

bool Board::makeMove(Move move, UndoInfo& undo) {
    undo.oldEp = ep_;
    undo.oldCastle = castle_;
    undo.oldHalfMoves = halfMoves_;
    undo.oldFullMoves = fullMoves_;
    undo.oldHash = hash_;
    undo.oldEvalState = evalState_;
    undo.captured = NO_PIECE;
    undo.oldHistorySize = static_cast<int>(posHistory_.size());

    if (move.from < A1 || move.from > H8 || move.to < A1 || move.to > H8)
        return false;

    Piece p = mailbox_[move.from];
    if (p == NO_PIECE) return false;
    Color us = stm_;
    if (colorOf(p) != us) return false;
    bool oldEpHasCapture = hasEnPassantCapture(mailbox_, us, ep_);
    PieceType pt = typeOf(p);

    bool promotionTarget = pt == PAWN && (rankOf(move.to) == RANK_8 || rankOf(move.to) == RANK_1);
    if (move.promotion != PIECE_TYPE_NB &&
        (!promotionTarget || move.promotion < KNIGHT || move.promotion > QUEEN)) {
        return false;
    }

    // Classify move if not pre-classified
    if (move.type == NORMAL) {
        if (pt == PAWN && move.to == ep_)
            move.type = EN_PASSANT;
        else if (pt == PAWN && (rankOf(move.to) == RANK_8 || rankOf(move.to) == RANK_1))
            move.type = (mailbox_[move.to] != NO_PIECE) ? PROMOTION_CAPTURE : PROMOTION;
        else if (pt == KING && (move.to == move.from + 2 || move.to == move.from - 2))
            move.type = CASTLING;
        else if (mailbox_[move.to] != NO_PIECE)
            move.type = CAPTURE;
    }

    if (move.type == CASTLING) {
        if (pt != KING) return false;

        const Color enemy = ~us;
        if (us == WHITE) {
            if (move.from != E1) return false;
            if (move.to == G1) {
                if (!(castle_ & WK) || mailbox_[H1] != W_ROOK ||
                    mailbox_[F1] != NO_PIECE || mailbox_[G1] != NO_PIECE ||
                    attacks::isSquareAttacked(*this, E1, enemy) ||
                    attacks::isSquareAttacked(*this, F1, enemy) ||
                    attacks::isSquareAttacked(*this, G1, enemy)) {
                    return false;
                }
            } else if (move.to == C1) {
                if (!(castle_ & WQ) || mailbox_[A1] != W_ROOK ||
                    mailbox_[D1] != NO_PIECE || mailbox_[C1] != NO_PIECE ||
                    mailbox_[B1] != NO_PIECE ||
                    attacks::isSquareAttacked(*this, E1, enemy) ||
                    attacks::isSquareAttacked(*this, D1, enemy) ||
                    attacks::isSquareAttacked(*this, C1, enemy)) {
                    return false;
                }
            } else {
                return false;
            }
        } else {
            if (move.from != E8) return false;
            if (move.to == G8) {
                if (!(castle_ & BK) || mailbox_[H8] != B_ROOK ||
                    mailbox_[F8] != NO_PIECE || mailbox_[G8] != NO_PIECE ||
                    attacks::isSquareAttacked(*this, E8, enemy) ||
                    attacks::isSquareAttacked(*this, F8, enemy) ||
                    attacks::isSquareAttacked(*this, G8, enemy)) {
                    return false;
                }
            } else if (move.to == C8) {
                if (!(castle_ & BQ) || mailbox_[A8] != B_ROOK ||
                    mailbox_[D8] != NO_PIECE || mailbox_[C8] != NO_PIECE ||
                    mailbox_[B8] != NO_PIECE ||
                    attacks::isSquareAttacked(*this, E8, enemy) ||
                    attacks::isSquareAttacked(*this, D8, enemy) ||
                    attacks::isSquareAttacked(*this, C8, enemy)) {
                    return false;
                }
            } else {
                return false;
            }
        }
    }

    // Reject friendly-piece captures
    if (mailbox_[move.to] != NO_PIECE && colorOf(mailbox_[move.to]) == us)
        return false;
    if (mailbox_[move.to] != NO_PIECE && typeOf(mailbox_[move.to]) == KING)
        return false;

    if ((move.type == CAPTURE || move.type == PROMOTION_CAPTURE) && mailbox_[move.to] == NO_PIECE)
        return false;
    if ((move.type == NORMAL || move.type == PROMOTION) && mailbox_[move.to] != NO_PIECE)
        return false;
    if (move.type == EN_PASSANT) {
        int capturedIdx = us == WHITE ? move.to - 8 : move.to + 8;
        if (pt != PAWN || mailbox_[move.to] != NO_PIECE ||
            capturedIdx < 0 || capturedIdx >= 64 ||
            mailbox_[Square(capturedIdx)] != makePiece(~us, PAWN)) {
            return false;
        }
    }

    // Verify pseudo-legal piece movement (non-castling only)
    if (move.type != CASTLING) {
        Bitboard reachable = 0;
        switch (pt) {
        case PAWN: {
            int push = (us == WHITE) ? 8 : -8;
            int oneIdx = move.from + push;
            if (oneIdx >= 0 && oneIdx < 64 && mailbox_[Square(oneIdx)] == NO_PIECE) {
                reachable |= squareBb(Square(oneIdx));
                int startRank = (us == WHITE) ? RANK_2 : RANK_7;
                int twoIdx = move.from + 2 * push;
                if (rankOf(move.from) == startRank && twoIdx >= 0 && twoIdx < 64 && mailbox_[Square(twoIdx)] == NO_PIECE)
                    reachable |= squareBb(Square(twoIdx));
            }
            if (move.type == CAPTURE || move.type == EN_PASSANT || move.type == PROMOTION_CAPTURE)
                reachable |= attacks::pawnAttacks(move.from, us);
            break;
        }
        case KNIGHT: reachable = attacks::knightAttacks(move.from); break;
        case BISHOP: reachable = attacks::bishopAttacks(move.from, occupied()); break;
        case ROOK:   reachable = attacks::rookAttacks(move.from, occupied()); break;
        case QUEEN:  reachable = attacks::queenAttacks(move.from, occupied()); break;
        case KING:   reachable = attacks::kingAttacks(move.from); break;
        default: return false;
        }
        if (!(reachable & squareBb(move.to))) return false;
    }

    // Reject promotions without a valid promotion piece
    if ((move.type == PROMOTION || move.type == PROMOTION_CAPTURE) &&
        (move.promotion < KNIGHT || move.promotion > QUEEN))
        return false;

    undo.move = move;
    posHistory_.push_back(hash_);

    // Remove piece from source
    removePiece(p, move.from);

    // Handle captures
    switch (move.type) {
    case CAPTURE:
    case PROMOTION_CAPTURE:
        undo.captured = mailbox_[move.to];
        removePiece(undo.captured, move.to);
        break;
    case EN_PASSANT:
        if (us == WHITE) {
            Square cs = Square(move.to - 8);
            undo.captured = mailbox_[cs];
            removePiece(undo.captured, cs);
        } else {
            Square cs = Square(move.to + 8);
            undo.captured = mailbox_[cs];
            removePiece(undo.captured, cs);
        }
        break;
    default:
        break;
    }

    // Place piece on destination
    Piece destPiece = p;
    if (move.type == PROMOTION || move.type == PROMOTION_CAPTURE)
        destPiece = makePiece(us, move.promotion);
    putPiece(destPiece, move.to);

    // Handle castling (rook move)
    if (move.type == CASTLING) {
        if (move.to == G1)      movePiece(W_ROOK, H1, F1);
        else if (move.to == C1) movePiece(W_ROOK, A1, D1);
        else if (move.to == G8) movePiece(B_ROOK, H8, F8);
        else if (move.to == C8) movePiece(B_ROOK, A8, D8);
    }

    // Update castling rights
    int oldCastle = castle_;
    castle_ &= ~castleRightsMask[move.from];
    castle_ &= ~castleRightsMask[move.to];
    if (oldCastle != castle_) {
        hash_ ^= zobristCastle_[oldCastle];
        hash_ ^= zobristCastle_[castle_];
    }

    // Update en passant
    Square oldEp = ep_;
    ep_ = SQ_NONE;
    if (pt == PAWN && ((us == WHITE && rankOf(move.to) == RANK_4 && rankOf(move.from) == RANK_2) ||
                       (us == BLACK && rankOf(move.to) == RANK_5 && rankOf(move.from) == RANK_7))) {
        ep_ = Square(us == WHITE ? move.from + 8 : move.from - 8);
    }
    if (oldEp != ep_) {
        if (oldEp != SQ_NONE && oldEpHasCapture)
            hash_ ^= zobristEp_[fileOf(oldEp)];
        if (ep_ != SQ_NONE && hasEnPassantCapture(mailbox_, ~us, ep_))
            hash_ ^= zobristEp_[fileOf(ep_)];
    }

    // Update half-move clock
    if (pt == PAWN || move.type == CAPTURE || move.type == EN_PASSANT)
        halfMoves_ = 0;
    else
        halfMoves_++;

    // Update full move number
    if (us == BLACK) fullMoves_++;

    // Toggle side
    hash_ ^= zobristSide_;
    stm_ = ~us;

    // Check legality: after the move, is the moving side's king in check?
    // The side that just moved is `us`; opponent is now `stm_`
    Square kSq = kingSquare(us);
    if (kSq != SQ_NONE && attacks::isSquareAttacked(*this, kSq, stm_)) {
        unmakeMove(move, undo);
        return false;
    }

    return true;
}

void Board::unmakeMove(Move /*move*/, const UndoInfo& undo) {
    const Move& m = undo.move;
    Piece p = mailbox_[m.to];
    Color us = ~stm_; // The side that made the move

    // Restore state
    stm_ = us;
    ep_ = undo.oldEp;
    castle_ = undo.oldCastle;
    halfMoves_ = undo.oldHalfMoves;
    fullMoves_ = undo.oldFullMoves;

    // Remove piece from destination
    if (m.type == PROMOTION || m.type == PROMOTION_CAPTURE) {
        removePiece(p, m.to);
        putPiece(makePiece(us, PAWN), m.from);
    } else {
        removePiece(p, m.to);
        putPiece(p, m.from);
    }

    // Restore captured pieces
    switch (m.type) {
    case CAPTURE:
    case PROMOTION_CAPTURE:
        putPiece(undo.captured, m.to);
        break;
    case EN_PASSANT:
        if (us == WHITE)
            putPiece(undo.captured, Square(m.to - 8));
        else
            putPiece(undo.captured, Square(m.to + 8));
        break;
    default:
        break;
    }

    // Undo castling rook move
    if (m.type == CASTLING) {
        if (m.to == G1)      movePiece(W_ROOK, F1, H1);
        else if (m.to == C1) movePiece(W_ROOK, D1, A1);
        else if (m.to == G8) movePiece(B_ROOK, F8, H8);
        else if (m.to == C8) movePiece(B_ROOK, D8, A8);
    }

    hash_ = undo.oldHash;
    evalState_ = undo.oldEvalState;
    posHistory_.resize(undo.oldHistorySize);
}

void Board::makeNullMove(NullUndo& undo) {
    undo.oldEp = ep_;
    undo.oldHalfMoves = halfMoves_;
    undo.oldHash = hash_;

    Color us = stm_;
    if (ep_ != SQ_NONE && hasEnPassantCapture(mailbox_, us, ep_))
        hash_ ^= zobristEp_[fileOf(ep_)];

    hash_ ^= zobristSide_;
    stm_ = ~stm_;

    ep_ = SQ_NONE;

    halfMoves_++;
}

void Board::unmakeNullMove(const NullUndo& undo) {
    stm_ = ~stm_;
    ep_ = undo.oldEp;
    halfMoves_ = undo.oldHalfMoves;
    hash_ = undo.oldHash;
}

bool Board::isMoveLegal(Move move) const {
    Board temp = *this;
    UndoInfo undo;
    return temp.makeMove(move, undo);
}

bool Board::isInCheck() const {
    Square kSq = kingSquare(stm_);
    return kSq != SQ_NONE && attacks::isSquareAttacked(*this, kSq, ~stm_);
}

Piece Board::pieceOn(Square s) const { return mailbox_[s]; }

Bitboard Board::pieces() const { return byColor_[WHITE] | byColor_[BLACK]; }
Bitboard Board::pieces(Color c) const { return byColor_[c]; }
Bitboard Board::pieces(PieceType pt) const { return byPiece_[makePiece(WHITE, pt)] | byPiece_[makePiece(BLACK, pt)]; }
Bitboard Board::pieces(Color c, PieceType pt) const { return byPiece_[makePiece(c, pt)]; }
Bitboard Board::occupied() const { return pieces(); }
Bitboard Board::empty() const { return ~pieces(); }

Square Board::kingSquare(Color c) const {
    Bitboard kings = pieces(c, KING);
    return kings ? lsb(kings) : SQ_NONE;
}

bool Board::isRepetition() const {
    int count = 0;
    for (uint64_t h : posHistory_) {
        if (h == hash_) count++;
    }
    return count >= 2;
}

} // namespace chess
