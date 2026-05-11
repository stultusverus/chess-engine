#include "board.h"
#include "attacks.h"
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <sstream>
#include <string>
#include <cctype>

namespace chess {

// --- Zobrist keys ---
namespace {
    int safeParseInt(const std::string& s, int defaultVal) {
        if (s.empty()) return defaultVal;
        char* end = nullptr;
        errno = 0;
        long v = std::strtol(s.c_str(), &end, 10);
        if (errno != 0 || end == s.c_str() || *end != '\0') return defaultVal;
        if (v < INT_MIN || v > INT_MAX) return defaultVal;
        return static_cast<int>(v);
    }

    uint64_t zobristPieces_[PIECE_NB][64];
    uint64_t zobristCastle_[16];
    uint64_t zobristEp_[8];
    uint64_t zobristSide_;

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
    zobristSide_ = prng(state);
    for (int i = 0; i < 16; i++)
        zobristCastle_[i] = prng(state);
    for (int f = 0; f < 8; f++)
        zobristEp_[f] = prng(state);
}

// --- Board ---

Board::Board() {
    setFen(STARTPOS_FEN);
}

Board::Board(const std::string& fen) {
    setFen(fen);
}

void Board::setFen(const std::string& fen) {
    std::fill(byPiece_.begin(), byPiece_.end(), Bitboard(0));
    std::fill(byColor_.begin(), byColor_.end(), Bitboard(0));
    std::fill(mailbox_.begin(), mailbox_.end(), NO_PIECE);
    stm_ = WHITE;
    ep_ = SQ_NONE;
    castle_ = 0;
    halfMoves_ = 0;
    fullMoves_ = 1;
    hash_ = 0;

    std::istringstream ss(fen);
    std::string placement, stm, castle, ep, half, full;

    ss >> placement >> stm >> castle >> ep >> half >> full;

    // Piece placement
    Square sq = A8;
    for (char c : placement) {
        if (c == '/') {
            if (sq == 0) break;
            int prevRank = rankOf(Square(sq - 1));
            if (prevRank <= 0) break;
            sq = makeSquare(FILE_A, Rank(prevRank - 1));
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            int empty = c - '0';
            if (empty < 1 || empty > 8) break;
            for (int i = 0; i < empty; i++) {
                sq = Square(sq + 1);
                if (sq < 0 || sq >= 64) break;
            }
        } else {
            if (sq >= 64) break;
            Piece p = NO_PIECE;
            switch (c) {
            case 'P': p = W_PAWN; break; case 'N': p = W_KNIGHT; break;
            case 'B': p = W_BISHOP; break; case 'R': p = W_ROOK; break;
            case 'Q': p = W_QUEEN; break; case 'K': p = W_KING; break;
            case 'p': p = B_PAWN; break; case 'n': p = B_KNIGHT; break;
            case 'b': p = B_BISHOP; break; case 'r': p = B_ROOK; break;
            case 'q': p = B_QUEEN; break; case 'k': p = B_KING; break;
            }
            if (p != NO_PIECE) {
                byPiece_[p] |= squareBb(sq);
                byColor_[colorOf(p)] |= squareBb(sq);
                mailbox_[sq] = p;
            }
            sq = Square(sq + 1);
        }
    }

    // Side to move
    stm_ = (stm == "b") ? BLACK : WHITE;

    // Castling rights
    if (castle.find('K') != std::string::npos) castle_ |= WK;
    if (castle.find('Q') != std::string::npos) castle_ |= WQ;
    if (castle.find('k') != std::string::npos) castle_ |= BK;
    if (castle.find('q') != std::string::npos) castle_ |= BQ;

    // En passant
    if (ep != "-" && ep.size() >= 2 && ep[0] >= 'a' && ep[0] <= 'h' && ep[1] >= '1' && ep[1] <= '8') {
        File f = File(ep[0] - 'a');
        Rank r = Rank(ep[1] - '1');
        ep_ = makeSquare(f, r);
    }

    // Clocks
    halfMoves_ = safeParseInt(half, 0);
    fullMoves_ = safeParseInt(full, 1);

    // Build hash
    hash_ = 0;
    for (int s = 0; s < 64; s++) {
        Piece p = mailbox_[s];
        if (p != NO_PIECE) hash_ ^= zobristPieces_[p][s];
    }
    if (stm_ == BLACK) hash_ ^= zobristSide_;
    hash_ ^= zobristCastle_[castle_];
    if (ep_ != SQ_NONE) hash_ ^= zobristEp_[fileOf(ep_)];
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

void Board::putPiece(Piece p, Square s) {
    if (p == NO_PIECE) return;
    byPiece_[p] |= squareBb(s);
    byColor_[colorOf(p)] |= squareBb(s);
    mailbox_[s] = p;
    hash_ ^= zobristPieces_[p][s];
}

void Board::removePiece(Piece p, Square s) {
    if (p == NO_PIECE) return;
    byPiece_[p] &= ~squareBb(s);
    byColor_[colorOf(p)] &= ~squareBb(s);
    mailbox_[s] = NO_PIECE;
    hash_ ^= zobristPieces_[p][s];
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
    undo.captured = NO_PIECE;

    Piece p = mailbox_[move.from];
    if (p == NO_PIECE) return false;
    Color us = stm_;
    if (colorOf(p) != us) return false;
    PieceType pt = typeOf(p);

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

    undo.move = move;

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
        if (oldEp != SQ_NONE) hash_ ^= zobristEp_[fileOf(oldEp)];
        if (ep_ != SQ_NONE)   hash_ ^= zobristEp_[fileOf(ep_)];
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
    if (attacks::isSquareAttacked(*this, kSq, stm_)) {
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
}

void Board::makeNullMove(NullUndo& undo) {
    undo.oldEp = ep_;
    undo.oldHalfMoves = halfMoves_;
    undo.oldHash = hash_;

    hash_ ^= zobristSide_;
    stm_ = ~stm_;

    if (ep_ != SQ_NONE) {
        hash_ ^= zobristEp_[fileOf(ep_)];
        ep_ = SQ_NONE;
    }

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
    return attacks::isSquareAttacked(*this, kingSquare(stm_), ~stm_);
}

Piece Board::pieceOn(Square s) const { return mailbox_[s]; }

Bitboard Board::pieces() const { return byColor_[WHITE] | byColor_[BLACK]; }
Bitboard Board::pieces(Color c) const { return byColor_[c]; }
Bitboard Board::pieces(PieceType pt) const { return byPiece_[makePiece(WHITE, pt)] | byPiece_[makePiece(BLACK, pt)]; }
Bitboard Board::pieces(Color c, PieceType pt) const { return byPiece_[makePiece(c, pt)]; }
Bitboard Board::occupied() const { return pieces(); }
Bitboard Board::empty() const { return ~pieces(); }

Square Board::kingSquare(Color c) const { return lsb(pieces(c, KING)); }

} // namespace chess
